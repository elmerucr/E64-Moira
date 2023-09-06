/*
 * hud.hpp
 * E64
 *
 * Copyright © 2021-2023 elmerucr. All rights reserved.
 */

#include <cstdlib>
#include <cstdint>

#include "blitter.hpp"
#include "cia.hpp"
#include "timer.hpp"
#include "TTL74LS148.hpp"

#ifndef HUD_HPP
#define HUD_HPP

#define MAXINPUT 1024

namespace E64 {

class hud_t {
private:
	bool irq_line;
	
	void process_command(char *buffer);
	
	uint16_t notify_frame_counter;
	uint16_t notify_frames;
	
	bool	dasm_as_hex;
	bool	stats_visible;
	int16_t	stats_pos;
	
	uint8_t recording_frame_counter;
	uint8_t buffer_warning_frame_counter;
	
	
	int32_t frame_cycle_saldo;
	bool frame_is_done;
public:
	hud_t();
	~hud_t();

	void memory_dump(uint32_t address, int rows);
	void memory_word_dump(uint32_t address, int rows);
	void enter_monitor_line(char *buffer);
	void enter_monitor_word_line(char *buffer);
	bool hex_string_to_int(const char *temp_string, uint32_t *return_value);
	
	TTL74LS148_ic *TTL74LS148;
	blitter_ic *blitter;
	cia_ic *cia;
	timer_ic *timer;
	
	blit_t *stats_view;
	blit_t *terminal;
	blit_t *cpu_view;
	blit_t *disassembly_view;
	blit_t *stack_view;
	blit_t *bar_80x1_empty;
	blit_t *bar_80x1_line_transparent;
	blit_t *other_info;
	blit_t *recording_icon;
	
	blit_t *notification;
	
	void reset();
	void run(uint16_t cycles);
	void update_views();
	void update_stats_view();
	void process_keypress();
	void redraw();
	void show_notification(const char *format, ...);
	void toggle_stats();
	
	// events
	void timer_0_event();
	void timer_1_event();
	void timer_2_event();
	void timer_3_event();
	void timer_4_event();
	void timer_5_event();
	void timer_6_event();
	void timer_7_event();
	
	inline bool frame_done() {
		bool result = frame_is_done;
		if (frame_is_done)
			frame_is_done = false;
		return result;
	}
};

}

#endif
