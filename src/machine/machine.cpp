/*
 * machine.cpp
 * E64
 *
 * Copyright © 2019-2023 elmerucr. All rights reserved.
 */

#include "machine.hpp"
#include "sdl2.hpp"
#include "common.hpp"

#include <cmath>
#include <unistd.h>

E64::machine_t::machine_t()
{
	underruns = equalruns = overruns = 1;
	under_lap = equal_lap = over_lap = 1;
	
	mmu = new mmu_ic();
	
	/*
	 * Create exception/priority handler + m68k cpu. Then connect
	 * m68k to exception handler.
	 */
	TTL74LS148 = new TTL74LS148_ic();
	m68k = new m68k_ic();
	m68k->setModel(M68EC020, M68EC020);
	m68k->debugger.reset();
	m68k->setDasmSyntax(DASM_MOIRA);
	m68k->setDasmIndentation(8);
	TTL74LS148->connect_m68k(m68k);
	
	/*
	 * Create timer and connect it to priority encoder
	 */
	timer = new timer_ic(TTL74LS148);
	
	blitter = new blitter_ic(VM_MAX_PIXELS_PER_SCANLINE, VM_MAX_SCANLINES);
	blitter->connect_exceptions_ic(TTL74LS148);
	
	sound = new sound_ic();
	
	cia = new cia_ic();
	
	/*
	 * Init clocks (frequency dividers)
	 */
	cpu_to_sid = new clocks(CPU_CLOCK_SPEED, SID_CLOCK_SPEED);
	
	recording_sound = false;
}

E64::machine_t::~machine_t()
{
	uint64_t total = underruns + equalruns + overruns;
	
	printf("[Machine] Audiobuffer overall performance:\n"
	       "[Machine]  underruns:     %6.2f%%\n"
	       "[Machine]  equalruns:     %6.2f%%\n"
	       "[Machine]  overruns:      %6.2f%%\n",
	       (double)underruns*100/total,
	       (double)equalruns*100/total,
	       (double)overruns*100/total);
	
	if (recording_sound) {
		stop_recording_sound();
	}
	
	delete cpu_to_sid;
	delete cia;
	delete sound;
	delete blitter;
	delete timer;
	delete m68k;
	delete TTL74LS148;
	delete mmu;
}

bool E64::machine_t::run(uint16_t cycles)
{
	m68k_cycle_saldo += cycles;
	
	/*
	 * A fine grained cycles_step is needed to be able run the
	 * proper amount of cycles on both cia and timer after each
	 * cpu instruction. That way, there'll be a decent emulation
	 * of interrupt triggering and acknowledgements.
	 *
	 * Note: This implies that cia and timer run at the same clock
	 * speed as the cpu.
	 */
	uint8_t cycles_step;
	
	int32_t consumed_cycles = 0;
	
	do {
		m68k->execute();
		cycles_step = m68k->getClock() - m68k->old_clock;
		m68k->old_clock += cycles_step;
		cia->run(cycles_step);
		timer->run(cycles_step);
		consumed_cycles += cycles_step;
	} while ((!m68k->breakpoint_reached) && (consumed_cycles < m68k_cycle_saldo));
	
	/*
	 * After reaching a breakpoint, it can be expected that the full
	 * amount of desired cycles wasn't completed yet. To make sure
	 * the step function in debugger mode works correctly, empty any
	 * remaining desired cycles by putting the cycle_saldo on 0.
	 */
	if (m68k->breakpoint_reached) {
		m68k_cycle_saldo = 0;
	} else {
		m68k_cycle_saldo -= consumed_cycles;
	}

	/*
	 * Run cycles on sound device & start audio if buffer is large
	 * enough. The aim is to have as much synchronization between
	 * CPU, timers and sound as possible. That way, music and other
	 * sound effects will sound as regularly as possible.
	 * If buffer size deviates too much, an adjusted amount of cycles
	 * will be run on sound.
	 */
	unsigned int audio_queue_size = stats.current_audio_queue_size();

	if (!recording_sound) {
		/* not recording sound */
		if (audio_queue_size < (0.5 * AUDIO_BUFFER_SIZE)) {
			sound->run(cpu_to_sid->clock(1.05 * consumed_cycles));
			underruns++;
		} else if (audio_queue_size < 1.2 * AUDIO_BUFFER_SIZE) {
			sound->run(cpu_to_sid->clock(consumed_cycles));
			equalruns++;
		} else if (audio_queue_size < 2.0 * AUDIO_BUFFER_SIZE) {
			sound->run(cpu_to_sid->clock(0.95 * consumed_cycles));
			overruns++;
		} else overruns++;
	} else {
		/* recording sound */
		if (audio_queue_size < (0.5 * AUDIO_BUFFER_SIZE)) {
			underruns++;
		} else if (audio_queue_size < 1.2 * AUDIO_BUFFER_SIZE) {
			equalruns++;
		} else {
			overruns++;
		}
		
		sound->run(cpu_to_sid->clock(consumed_cycles));
		
		float sample;
		
		while (machine.sound->record_buffer_pop(&sample)) {
			host.settings->write_to_wav(sample);
		}
	}
	
	if (audio_queue_size > (3*AUDIO_BUFFER_SIZE/4))
		E64::sdl2_start_audio();
	
	frame_cycle_saldo += consumed_cycles;
	
	if (frame_cycle_saldo > CPU_CYCLES_PER_FRAME) {
		frame_is_done = true;
		
		/*
		 * Warn blitter for possible IRQ pull
		 */
		blitter->notify_screen_refreshed();
		
		
		frame_cycle_saldo -= CPU_CYCLES_PER_FRAME;
		
		/*
		 * Then run blitter
		 */
		while (blitter->run_next_operation()) {}
	}
	
	if (m68k->breakpoint_reached) {
		m68k->breakpoint_reached = false;
		return true;
	} else {
		return false;
	}
}

void E64::machine_t::reset()
{
	printf("[Machine] System reset\n");
	
	m68k_cycle_saldo = 0;
	frame_cycle_saldo = 0;
	frame_is_done = false;
	
	mmu->reset();
	sound->reset();
	blitter->reset();
	timer->reset();
	cia->reset();
	
	m68k->reset();
	m68k->old_clock = 0;
	m68k->setClock(m68k->old_clock);
}

void E64::machine_t::toggle_recording_sound()
{
	if (!recording_sound) {
		start_recording_sound();
	} else {
		stop_recording_sound();
	}
}

void E64::machine_t::start_recording_sound()
{
	recording_sound = true;
	sound->clear_record_buffer();
	host.settings->create_wav();
	hud.show_notification("start recording sound");
}

void E64::machine_t::stop_recording_sound()
{
	recording_sound = false;
	host.settings->finish_wav();
	hud.show_notification("stop recording sound");
}

bool E64::machine_t::buffer_within_specs()
{
	bool result = !((underruns > under_lap) || (overruns > over_lap));
	
	under_lap = underruns;
	equal_lap = equalruns;
	over_lap  = overruns ;
	
	return result;
}

void E64::machine_t::flip_modes()
{
	if (mode == RUNNING) {
		mode = PAUSED;
	} else {
		mode = RUNNING;
	}
}
