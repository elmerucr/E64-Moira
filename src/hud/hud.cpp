/*
 * hud.cpp
 * E64
 *
 * Copyright © 2021-2023 elmerucr. All rights reserved.
 */

#include "hud.hpp"
#include "common.hpp"
#include "sdl2.hpp"

char text_buffer[2048];

#define	RAM_SIZE_CPU_VISIBLE	0x1000000

/*
 * hex2int
 * take a hex string and convert it to a 32bit number (max 8 hex digits)
 * from https://stackoverflow.com/questions/10156409/convert-hex-string-char-to-int
 *
 * This function is slightly adopted to check for true values. It returns false
 * when there's wrong input.
 */
bool E64::hud_t::hex_string_to_int(const char *temp_string, uint32_t *return_value)
{
	uint32_t val = 0;
	while (*temp_string) {
		/* Get current character then increment */
		uint8_t byte = *temp_string++;
		/* Transform hex character to the 4bit equivalent number */
		if (byte >= '0' && byte <= '9') {
			byte = byte - '0';
		} else if (byte >= 'a' && byte <='f') {
			byte = byte - 'a' + 10;
		} else if (byte >= 'A' && byte <='F') {
			byte = byte - 'A' + 10;
		} else {
			/* Problem, return false and do not write the return value */
			return false;
		}
		/* Shift 4 to make space for new digit, and add the 4 bits of the new digit */
		val = (val << 4) | (byte & 0xf);
	}
	*return_value = val;
	return true;
}

E64::hud_t::hud_t()
{
	printf("[HUD] heads up display constructor\n");
	TTL74LS148 = new TTL74LS148_ic();
	blitter = new blitter_ic(HUD_PIXELS_PER_SCANLINE, HUD_SCANLINES);
	cia = new cia_ic();
	timer = new timer_ic(TTL74LS148);
	
	stats_view = &blitter->blit[0];
	blitter->terminal_init(stats_view->number, 0x1a, 0x00, 1,1,60,4, GREEN_06,
				  (GREEN_01 & 0x0fff) | 0xa000);
	
	terminal = &blitter->blit[1];
	blitter->terminal_init(terminal->number, 0x1a, 0x00, 1,1, 80, 25, GREEN_05,
				(GREEN_01 & 0x0fff) | 0xa000);
	
	cpu_view = &blitter->blit[2];
	blitter->terminal_init(cpu_view->number, 0x1a, 0x00, 1,1,80,10, GREEN_05,
				(GREEN_01 & 0x0fff) | 0xa000);
	
	disassembly_view = &blitter->blit[3];
	blitter->terminal_init(disassembly_view->number, 0x1a, 0b00000000, 1,1,80,10, GREEN_05,
					(GREEN_01 & 0x0fff) | 0xa000);

	stack_view = &blitter->blit[4];
	blitter->terminal_init(stack_view->number, 0x1a, 0x00, 1,1,40,10, GREEN_05,
					(GREEN_01 & 0x0fff) | 0x0000);
	
	bar_80x1_empty = &blitter->blit[6];
	blitter->terminal_init(bar_80x1_empty->number, 0x1a, 0x00, 1,1,80,1, GREEN_05,
					(GREEN_01 & 0x0fff) | 0xa000);

	bar_80x1_line_transparent = &blitter->blit[5];
	blitter->terminal_init(bar_80x1_line_transparent->number, 0x02, 0x00, 80,1,80,1, GREEN_05, 0x0000);
	
	other_info = &blitter->blit[8];
	blitter->terminal_init(other_info->number, 0x1a, 0x00, 1,1,80,1, GREEN_05,
				  (GREEN_01 & 0x0fff) | 0xa000);
	
	notification = &blitter->blit[9];
	blitter->terminal_init(notification->number, 0x1a, 0x00, 1,1,60,2, GREEN_06,
				    (GREEN_01 & 0x0fff) | 0xa000);
	
	recording_icon = &blitter->blit[10];
	blitter->terminal_init(recording_icon->number, 0x1a, 0x00, 1,1,8,1, 0xff00, 0x0000);
	
	dasm_as_hex = false;
	stats_visible = false;
	stats_pos = HUD_SCANLINES;
	
	irq_line = true;
	
	notify_frame_counter = 0;
	notify_frames = FPS * 4;	// 4 seconds
	
	frame_is_done = false;
	frame_cycle_saldo = 0;
	
	blitter->set_clear_color(0x0000);
}

E64::hud_t::~hud_t()
{
	delete timer;
	delete cia;
	delete blitter;
	delete TTL74LS148;
}

void E64::hud_t::reset()
{
	blitter->reset();
	blitter->set_clear_color(0x0000);
	blitter->set_hor_border_color(0x0000);
	blitter->set_hor_border_size(0);
	
	cia->reset();
	cia->set_keyboard_repeat_delay(50);
	cia->set_keyboard_repeat_speed(5);
	cia->generate_key_events();
	
	timer->reset();
	timer->set(0, 3600);	// check keyboard, generate key events...
	
	blitter->terminal_clear(terminal->number);
	blitter->terminal_printf(terminal->number, "E64 Computer System (C)2019-%u elmerucr", E64_YEAR);
	blitter->terminal_prompt(terminal->number);
	blitter->terminal_activate_cursor(terminal->number);
	
	/*
	 * bar_80x1 is one big tile, so make sure first tile = "sprite 0"
	 */
	blitter->terminal_set_tile(bar_80x1_line_transparent->number, 0, 0);
	for (int i=0; i<(8*640); i++)
		blitter->set_pixel(bar_80x1_line_transparent->number, i, 0x0000);
	for (int i = (3*640); i<(4*640); i++)
		blitter->set_pixel(bar_80x1_line_transparent->number, i, GREEN_05);
	
	blitter->terminal_clear(bar_80x1_empty->number);
	
	blitter->terminal_clear(other_info->number);

	blitter->terminal_clear(recording_icon->number);
	blitter->terminal_printf(recording_icon->number, "rec ");
	blitter->terminal_putsymbol(recording_icon->number, 0x07);
}

void E64::hud_t::process_keypress()
{
	while (cia->io_read_8(0x00)) {
		blitter->terminal_deactivate_cursor(terminal->number);
	
		uint8_t key_value = cia->io_read_8(0x04);
		switch (key_value) {
			case ASCII_CURSOR_LEFT:
				blitter->terminal_cursor_left(terminal->number);
				break;
			case ASCII_CURSOR_RIGHT:
				blitter->terminal_cursor_right(terminal->number);
				break;
			case ASCII_CURSOR_UP:
				blitter->terminal_cursor_up(terminal->number);
				break;
			case ASCII_CURSOR_DOWN:
				blitter->terminal_cursor_down(terminal->number);
				break;
			case ASCII_BACKSPACE:
				blitter->terminal_backspace(terminal->number);
				break;
			case ASCII_F1:
				// advance one instruction
				if (machine.mode == E64::PAUSED) {
					machine.run(0);
				}
				break;
			case ASCII_F2:
				// advance eight instructions
				if (machine.mode == E64::PAUSED) {
					for (int i=0; i<8; i++) machine.run(0);
				}
				break;
			case ASCII_F3:
				// advance sixtyfour instructions
				if (machine.mode == E64::PAUSED) {
					for (int i=0; i<64; i++) machine.run(0);
				}
				break;
			case ASCII_F4:
				// toggle disassembly view to hex
				dasm_as_hex = !dasm_as_hex;
				break;
			case ASCII_LF:
			{
				char *buffer = blitter->terminal_enter_command(terminal->number);
				process_command(buffer);
			}
				break;
			default:
				blitter->terminal_putchar(terminal->number, key_value);
				break;
		}
		blitter->terminal_activate_cursor(terminal->number);
	}
}

void E64::hud_t::update_stats_view()
{
	blitter->terminal_clear(stats_view->number);
	blitter->blit[stats_view->number].set_foreground_color(GREEN_06);
	blitter->terminal_puts(stats_view->number, stats.summary());
	if (!machine.buffer_within_specs()) {
		buffer_warning_frame_counter = 20;
	}
	
	if (buffer_warning_frame_counter) {
		for (int i = 120; i < 150; i++) {
			blitter->terminal_set_tile_fg_color(stats_view->number, i, 0xff00);
		}
		buffer_warning_frame_counter--;
	}
}

void E64::hud_t::update_views()
{
	blitter->terminal_clear(cpu_view->number);
	machine.m68k->status(text_buffer);
	blitter->terminal_printf(cpu_view->number, "%s", text_buffer);
	
	blitter->terminal_clear(disassembly_view->number);
	
	uint32_t pc = machine.m68k->getPC();
	uint32_t old_pc;
	
	/*
	 * Disassembler
	 */
	for (int i=0; i<10; i++) {
		uint16_t old_color = terminal->get_foreground_color();
		if (machine.m68k->debugger.breakpoints.isSetAt(pc)) {
			disassembly_view->set_foreground_color(AMBER_07);
		}
		if (disassembly_view->terminal_get_current_column() != 0) {
			blitter->terminal_putchar(disassembly_view->number, '\n');
		}
		old_pc = pc;
		pc += machine.m68k->disassemble(pc, text_buffer);
		if (dasm_as_hex) {
			blitter->terminal_printf(disassembly_view->number, ",%08x", old_pc);
			while (old_pc != pc) {
				blitter->terminal_printf(disassembly_view->number, " %04x", machine.mmu->read_memory_16(old_pc));
				old_pc += 2;
			}
		} else {
			blitter->terminal_printf(disassembly_view->number, ",%08x %s", old_pc, text_buffer);
		}
		disassembly_view->set_foreground_color(old_color);
	}
	
	/*
	 * Stack view
	 */
	blitter->terminal_clear(stack_view->number);
	machine.m68k->stacks(text_buffer, 7);
	blitter->terminal_printf(stack_view->number, "%s", text_buffer);
	
	/*
	 * Misc
	 */
	blitter->terminal_clear(other_info->number);
	blitter->terminal_printf(other_info->number, " blitter=%%%c%c%c%c%c%c%c%c timer=%%%c%c%c%c%c%c%c%c   Frame cycles:%6u/%6u",
				 machine.blitter->io_read_8(0x00) & 0b10000000 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b01000000 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b00100000 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b00010000 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b00001000 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b00000100 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b00000010 ? '1' : '0',
				 machine.blitter->io_read_8(0x00) & 0b00000001 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b10000000 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b01000000 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b00100000 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b00010000 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b00001000 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b00000100 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b00000010 ? '1' : '0',
				 machine.timer->io_read_8(0x00) & 0b00000001 ? '1' : '0',
				 machine.frame_cycles(), CPU_CYCLES_PER_FRAME);
}

void E64::hud_t::run(uint16_t cycles)
{
	timer->run(cycles);
	if (TTL74LS148->get_interrupt_level() != 0) {
		for (int i=0; i<8; i++) {
			if (timer->io_read_8(0x00) & (0b1 << i)) {
				switch (i) {
					case 0:
						timer_0_event();
						/*
						 * Acknowledge interrupt
						 */
						timer->io_write_8(0x00, 0b00000001);
						break;
					case 1:
						timer_1_event();
						/*
						 * Acknowledge interrupt
						 */
						timer->io_write_8(0x00, 0b00000010);
					default:
						break;
				}
			}
		}
	}
	
	cia->run(cycles);
	
	frame_cycle_saldo += cycles;
	
	if (frame_cycle_saldo > CPU_CYCLES_PER_FRAME) {
		frame_is_done = true;
		frame_cycle_saldo -= CPU_CYCLES_PER_FRAME;
	}
}

void E64::hud_t::timer_0_event()
{
	blitter->terminal_process_cursor_state(terminal->number);
}

void E64::hud_t::timer_1_event()
{

}

void E64::hud_t::timer_2_event()
{
	//
}

void E64::hud_t::timer_3_event()
{
	//
}

void E64::hud_t::timer_4_event()
{
	//
}

void E64::hud_t::timer_5_event()
{
	//
}

void E64::hud_t::timer_6_event()
{
	//
}

void E64::hud_t::timer_7_event()
{
	//
}

void E64::hud_t::redraw()
{
	blitter->clear_framebuffer();
	
	if (machine.mode == E64::PAUSED) {
		bar_80x1_empty->set_x_pos(0);
		bar_80x1_empty->set_y_pos(-4);
		blitter->draw_blit(bar_80x1_empty);
		
		cpu_view->set_x_pos(0);
		cpu_view->set_y_pos(4);
		blitter->draw_blit(cpu_view);
		
		stack_view->set_x_pos(328);
		stack_view->set_y_pos(4);
		blitter->draw_blit(stack_view);
		
		bar_80x1_empty->set_x_pos(0);
		bar_80x1_empty->set_y_pos(84);
		blitter->draw_blit(bar_80x1_empty);
		
		bar_80x1_line_transparent->set_x_pos(0);
		bar_80x1_line_transparent->set_y_pos(84);
		blitter->draw_blit(bar_80x1_line_transparent);
		
		other_info->set_x_pos(0);
		other_info->set_y_pos(92);
		blitter->draw_blit(other_info);
		
		bar_80x1_empty->set_x_pos(0);
		bar_80x1_empty->set_y_pos(100);
		blitter->draw_blit(bar_80x1_empty);
		
		bar_80x1_line_transparent->set_x_pos(0);
		bar_80x1_line_transparent->set_y_pos(100);
		blitter->draw_blit(bar_80x1_line_transparent);
		
		disassembly_view->set_x_pos(0);
		disassembly_view->set_y_pos(108);
		blitter->draw_blit(disassembly_view);
		
		bar_80x1_empty->set_x_pos(0);
		bar_80x1_empty->set_y_pos(188);
		blitter->draw_blit(bar_80x1_empty);
		
		bar_80x1_line_transparent->set_x_pos(0);
		bar_80x1_line_transparent->set_y_pos(188);
		blitter->draw_blit(bar_80x1_line_transparent);
		
		terminal->set_x_pos(0);
		terminal->set_y_pos(196);
		blitter->draw_blit(terminal);
		
		bar_80x1_empty->set_x_pos(0);
		bar_80x1_empty->set_y_pos(396);
		blitter->draw_blit(bar_80x1_empty);
	} else {
		// machine running, check for recording activity
		if (machine.recording() && (recording_frame_counter & 0x80)) {
			recording_icon->set_x_pos(HUD_PIXELS_PER_SCANLINE - 64);
			recording_icon->set_y_pos(HUD_SCANLINES - 8);
			blitter->draw_blit(recording_icon);
		}
		recording_frame_counter -= 3;
	}
	
	if (stats_pos < HUD_SCANLINES) {
		stats_view->set_x_pos((HUD_PIXELS_PER_SCANLINE / 2) - (stats_view->get_width() / 2));
		stats_view->set_y_pos(stats_pos);
		blitter->draw_blit(stats_view);
	}
	if (stats_visible && (stats_pos > (HUD_SCANLINES - stats_view->get_height()))) stats_pos--;
	if (!stats_visible && (stats_pos < HUD_SCANLINES)) stats_pos++;
	
	if (notify_frame_counter) {
		int16_t temp_y_pos = 88 - abs(notify_frame_counter - notify_frames);
		if (temp_y_pos > 0 ) temp_y_pos = 0;
		notification->set_x_pos((HUD_PIXELS_PER_SCANLINE - blitter->blit[notification->number].get_width()) / 2);
		notification->set_y_pos(temp_y_pos);
		blitter->draw_blit(notification);
		notify_frame_counter--;
	}
}

void E64::hud_t::process_command(char *buffer)
{
	bool have_prompt = true;
	
	char *token0, *token1;
	token0 = strtok(buffer, " ");
	
	if (token0 == NULL) {
		have_prompt = false;
		blitter->terminal_putchar(terminal->number, '\n');
	} else if (token0[0] == ':') {
		have_prompt = false;
		enter_monitor_line(buffer);
	} else if (token0[0] == ';') {
		have_prompt = false;
		enter_monitor_word_line(buffer);
	} else if (strcmp(token0, "b") == 0) {
		token1 = strtok(NULL, " ");
		blitter->terminal_putchar(terminal->number, '\n');
		if (token1 == NULL) {
			unsigned int no_of_breakpoints = (unsigned int)machine.m68k->debugger.breakpoints.elements();
			if (no_of_breakpoints == 0) {
				blitter->terminal_printf(terminal->number, "currently no cpu breakpoints defined");
			} else {
				blitter->terminal_printf(terminal->number, "  # address active");
				for (int i=0; i<no_of_breakpoints; i++) {
					blitter->terminal_printf(terminal->number, "\n%3u $%06x  %s", i,
						 machine.m68k->debugger.breakpoints.guardAddr(i),
						 machine.m68k->debugger.breakpoints.isEnabled(i) ? "yes" : "no");
				}
			}
		} else {
			uint32_t temp_32bit;
			if (hex_string_to_int(token1, &temp_32bit)) {
				temp_32bit &= (RAM_SIZE_CPU_VISIBLE - 1);
				if (machine.m68k->debugger.breakpoints.isSetAt(temp_32bit)) {
					machine.m68k->debugger.breakpoints.removeAt(temp_32bit);
				} else {
					machine.m68k->debugger.breakpoints.setAt(temp_32bit);
				}
				blitter->terminal_printf(terminal->number, "breakpoint %s at $%06x",
						machine.m68k->debugger.breakpoints.isSetAt(temp_32bit) ? "set" : "cleared",
						temp_32bit);
			} else {
				blitter->terminal_puts(terminal->number, "error: invalid address");
			}
		}
	} else if (strcmp(token0, "bc") == 0 ) {
		blitter->terminal_puts(terminal->number, "\nclearing all breakpoints");
		machine.m68k->debugger.breakpoints.removeAll();
	} else if (strcmp(token0, "mw") == 0) {
		have_prompt = false;
		token1 = strtok(NULL, " ");

		uint8_t lines_remaining = terminal->terminal_lines_remaining();

		if (lines_remaining == 0) lines_remaining = 1;

		uint32_t blit_memory_location = 0x000000;

		if (token1 == NULL) {
			for (int i=0; i<lines_remaining; i++) {
				blitter->terminal_putchar(terminal->number, '\n');
				memory_word_dump(blit_memory_location, 1);
				blit_memory_location = (blit_memory_location + 16) & 0xfffffe;
			}
		} else {
			if (!hex_string_to_int(token1, &blit_memory_location)) {
				blitter->terminal_putchar(terminal->number, '\n');
				blitter->terminal_puts(terminal->number, "error: invalid address\n");
			} else {
				for (int i=0; i<lines_remaining; i++) {
					blitter->terminal_putchar(terminal->number, '\n');
					memory_word_dump(blit_memory_location & 0xfffffe, 1);
					blit_memory_location = (blit_memory_location + 16) & 0xfffffe;
				}
			}
		}
	} else if (strcmp(token0, "c") == 0 ) {
		have_prompt = false;
		E64::sdl2_wait_until_enter_released();
		blitter->terminal_putchar(terminal->number, '\n');
		//terminal->terminal_putchar('\n');
		machine.flip_modes();
		
		/*
		 * This extra call ensures the keystates are nice when
		 * entering the machine again
		 */
		sdl2_process_events();
	} else if (strcmp(token0, "clear") == 0 ) {
		have_prompt = false;
		blitter->terminal_clear(terminal->number);
		//terminal->terminal_clear();
	} else if (strcmp(token0, "exit") == 0) {
		have_prompt = false;
		E64::sdl2_wait_until_enter_released();
		app_running = false;
	} else if (strcmp(token0, "m") == 0) {
		have_prompt = false;
		token1 = strtok(NULL, " ");
		
		uint8_t lines_remaining = terminal->terminal_lines_remaining();
		
		if (lines_remaining == 0) lines_remaining = 1;

		uint32_t temp_pc = machine.m68k->getPC();
	
		if (token1 == NULL) {
			for (int i=0; i<lines_remaining; i++) {
				blitter->terminal_putchar(terminal->number, '\n');
				memory_dump(temp_pc, 1);
				temp_pc = (temp_pc + 8) & 0xffffff;
			}
		} else {
			if (!hex_string_to_int(token1, &temp_pc)) {
				blitter->terminal_putchar(terminal->number, '\n');
				blitter->terminal_puts(terminal->number, "error: invalid address\n");
			} else {
				for (int i=0; i<lines_remaining; i++) {
					blitter->terminal_putchar(terminal->number, '\n');
					memory_dump(temp_pc & (RAM_SIZE_CPU_VISIBLE - 1), 1);
					temp_pc = (temp_pc + 8) & 0xffffff;
				}
			}
		}
	} else if (strcmp(token0, "reset") == 0) {
		E64::sdl2_wait_until_enter_released();
		machine.reset();
	} else if (strcmp(token0, "timer") == 0) {
		machine.timer->status(text_buffer, 512);
		blitter->terminal_printf(terminal->number, "%s", text_buffer);
	} else if (strcmp(token0, "ver") == 0) {
		blitter->terminal_printf(terminal->number, "\nE64 (C)2019-%i - version %i.%i (%i)", E64_YEAR, E64_MAJOR_VERSION, E64_MINOR_VERSION, E64_BUILD);
	} else {
		blitter->terminal_putchar(terminal->number, '\n');
		blitter->terminal_printf(terminal->number, "error: unknown command '%s'", token0);
	}
	
	if (have_prompt) {
		blitter->terminal_prompt(terminal->number);
	}
}

void E64::hud_t::memory_dump(uint32_t address, int rows)
{
    address = address & 0xfffffe;  // only even addresses allowed!
    
	for (int i=0; i<rows; i++ ) {
		uint32_t temp_address = address;
		blitter->terminal_printf(terminal->number, "\r:%06x ", temp_address);
		for (int i=0; i<8; i++) {
			blitter->terminal_printf(terminal->number, "%02x ", machine.mmu->read_memory_8(temp_address));
			temp_address++;
			temp_address &= RAM_SIZE_CPU_VISIBLE - 1;
		}
	
		terminal->set_foreground_color(GREEN_07);
		terminal->set_background_color((GREEN_04 & 0x0fff) | 0xc000);
		
		temp_address = address;
		for (int i=0; i<8; i++) {
			uint8_t temp_byte = machine.mmu->read_memory_8(temp_address);
			blitter->terminal_putsymbol(terminal->number, temp_byte);
			temp_address++;
			temp_address &= 0xffffff;
		}
		
		terminal->set_foreground_color(GREEN_05);
		terminal->set_background_color((GREEN_01 & 0x0fff) | 0xc000);
		
		blitter->terminal_putsymbol(terminal->number, ' ');
		
		temp_address = address;
		for (int i=0; i<4; i++) {
			terminal->set_foreground_color(machine.mmu->read_memory_16(temp_address) | 0xf000);
			blitter->terminal_putsymbol(terminal->number, 0xa0);
			temp_address += 2;
			temp_address &= 0xfffffe;
		}
		
		address += 8;
		address &= RAM_SIZE_CPU_VISIBLE - 1;
	
		terminal->set_foreground_color(GREEN_05);
		terminal->set_background_color((GREEN_01 & 0x0fff) | 0xc000);
       
		for (int i=0; i<37; i++) {
			blitter->terminal_cursor_left(terminal->number);
		}
	}
}

/*
 * TODO: Change into word_memory_dump, 8 16bit words + colors
 */
void E64::hud_t::memory_word_dump(uint32_t address, int rows)
{
    address = address & 0xfffffe;
    
	for (int i=0; i<rows; i++ ) {
		uint32_t temp_address = address;
		blitter->terminal_printf(terminal->number, "\r;%06x ", temp_address);
		for (int i=0; i<8; i++) {
			blitter->terminal_printf(terminal->number, "%04x ", machine.mmu->read_memory_16(temp_address));
			temp_address += 2;
			temp_address &= 0xfffffe;
		}
		
		terminal->set_foreground_color(GREEN_07);
		terminal->set_background_color((GREEN_04 & 0x0fff) | 0xc000);
		
		temp_address = address;
		for (int i=0; i<16; i++) {
			uint8_t temp_byte = machine.mmu->read_memory_8(temp_address);
			blitter->terminal_putsymbol(terminal->number, temp_byte);
			temp_address++;
			temp_address &= 0xffffff;
		}
		
		terminal->set_foreground_color(GREEN_05);
		terminal->set_background_color((GREEN_01 & 0x0fff) | 0xc000);
		
		blitter->terminal_putsymbol(terminal->number, ' ');
		
		temp_address = address;
		for (int i=0; i<8; i++) {
			terminal->set_foreground_color(machine.mmu->read_memory_16(temp_address) | 0xf000);
			blitter->terminal_putsymbol(terminal->number, 0xa0);
			temp_address += 2;
			temp_address &= 0xfffffe;
		}
		
		address += 16;
		address &= 0xfffffe;
	
		terminal->set_foreground_color(GREEN_05);
       
		for (int i=0; i<65; i++) {
			blitter->terminal_cursor_left(terminal->number);
		}
	}
}

void E64::hud_t::enter_monitor_line(char *buffer)
{
	uint32_t address;
	uint32_t arg0, arg1, arg2, arg3;
	uint32_t arg4, arg5, arg6, arg7;
    
	buffer[7]  = '\0';
	buffer[10] = '\0';
	buffer[13] = '\0';
	buffer[16] = '\0';
	buffer[19] = '\0';
	buffer[22] = '\0';
	buffer[25] = '\0';
	buffer[28] = '\0';
	buffer[31] = '\0';
    
	if (!hex_string_to_int(&buffer[1], &address)) {
		blitter->terminal_putchar(terminal->number, '\r');
		blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??????\n");
	} else if (!hex_string_to_int(&buffer[8], &arg0)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<8; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[11], &arg1)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<11; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[14], &arg2)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<14; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[17], &arg3)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<17; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[20], &arg4)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<20; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[23], &arg5)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<23; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[26], &arg6)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<26; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else if (!hex_string_to_int(&buffer[29], &arg7)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<29; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??\n");
	} else {
		uint32_t original_address = address;
	
		arg0 &= 0xff;
		arg1 &= 0xff;
		arg2 &= 0xff;
		arg3 &= 0xff;
		arg4 &= 0xff;
		arg5 &= 0xff;
		arg6 &= 0xff;
		arg7 &= 0xff;
	
		machine.mmu->write_memory_8(address, (uint8_t)arg0); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg1); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg2); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg3); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg4); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg5); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg6); address +=1; address &= 0xffffff;
		machine.mmu->write_memory_8(address, (uint8_t)arg7); address +=1; address &= 0xffffff;
		
		blitter->terminal_putchar(terminal->number, '\r');
	
		memory_dump(original_address, 1);
	
		original_address += 8;
		original_address &= 0xffffff;
		blitter->terminal_printf(terminal->number, "\n:%06x ", original_address);
	}
}

void E64::hud_t::enter_monitor_word_line(char *buffer)
{
	uint32_t address;
	uint32_t arg0, arg1, arg2, arg3;
	uint32_t arg4, arg5, arg6, arg7;
    
	buffer[7]  = '\0';
	buffer[12] = '\0';
	buffer[17] = '\0';
	buffer[22] = '\0';
	buffer[27] = '\0';
	buffer[32] = '\0';
	buffer[37] = '\0';
	buffer[42] = '\0';
	buffer[47] = '\0';
    
	if (!hex_string_to_int(&buffer[1], &address)) {
		blitter->terminal_putchar(terminal->number, '\r');
		blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "??????\n");
	} else if (!hex_string_to_int(&buffer[8], &arg0)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<8; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[13], &arg1)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<13; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[18], &arg2)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<18; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[23], &arg3)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<23; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[28], &arg4)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<28; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[33], &arg5)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<33; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[38], &arg6)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<38; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else if (!hex_string_to_int(&buffer[43], &arg7)) {
		blitter->terminal_putchar(terminal->number, '\r');
		for (int i=0; i<43; i++) blitter->terminal_cursor_right(terminal->number);
		blitter->terminal_puts(terminal->number, "????\n");
	} else {
		uint32_t original_address = address;
		//printf("%04x %04x %04x\n", arg5, arg6, arg7);
	
		arg0 &= 0xffff;
		arg1 &= 0xffff;
		arg2 &= 0xffff;
		arg3 &= 0xffff;
		arg4 &= 0xffff;
		arg5 &= 0xffff;
		arg6 &= 0xffff;
		arg7 &= 0xffff;
	
		machine.mmu->write_memory_16(address, (uint16_t)arg0); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg1); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg2); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg3); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg4); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg5); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg6); address +=2; address &= 0xfffffe;
		machine.mmu->write_memory_16(address, (uint16_t)arg7); address +=2; address &= 0xfffffe;
		
		blitter->terminal_putchar(terminal->number, '\r');
	
		memory_word_dump(original_address, 1);
	
		original_address += 16;
		original_address &= 0xfffffe;
		blitter->terminal_printf(terminal->number, "\n;%06x ", original_address);
	}
}

void E64::hud_t::show_notification(const char *format, ...)
{
	notify_frame_counter = notify_frames;
	
	if (blitter) {
		blitter->terminal_clear(notification->number);
	}
	
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, 1024, format, args);
	va_end(args);

	if (blitter) {
		blitter->terminal_printf(notification->number, "\n\n\n%s", buffer);
	}
}

void E64::hud_t::toggle_stats()
{
	stats_visible = !stats_visible;
}
