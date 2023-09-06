/*
 * mmu.cpp
 * E64
 *
 * Copyright © 2019-2023 elmerucr. All rights reserved.
 */

#include "mmu.hpp"
#include "common.hpp"
#include "rom.hpp"

void E64::mmu_ic::reset()
{
	// if desired & available, update rom image
	if (host.settings->use_custom_rom) {
		printf("[MMU] Trying to use custom rom\n");
		update_rom_image();
	} else {
		printf("[MMU] Using built-in rom\n");
		for(int i=0; i<65536; i++) current_rom_image[i] = rom[i];
	}
}

uint8_t E64::mmu_ic::read_memory_8(uint32_t address)
{
	uint16_t page = (address & 0xffffff) >> 8;
	
	/*
	 * Mirror first 8 bytes in memory to first 8 bytes from current rom.
	 * Respectively inital SSP and PC (reset vectors).
	 */
	if (!(address & 0xfffffff8)) {
		return current_rom_image[address & 0x7];
	}
	
	if ((page & 0xfff8) == 0x0008) {
		switch (page) {
			// $0800 - $0fff io range
			case IO_BLITTER:
				return machine.blitter->io_read_8(address & 0xff);
			case IO_TIMER_PAGE:
				return machine.timer->io_read_8(address & 0xff);
			case IO_CIA_PAGE:
				return machine.cia->io_read_8(address & 0xff);
			case IO_SID_PAGE:
			case IO_ANALOG_PAGE:
			case IO_MIXER_PAGE:
				return machine.sound->read_byte(address & 0x3ff);
			default:
				return machine.blitter->video_memory_read_8(address & 0xffffff);
		}
	} else if ((page & 0xff00) == 0x0100) {
		// $10000 - $1ffff io blit registers (64kb)
		// for now:
		return machine.blitter->io_blit_contexts_read_8(address & 0xffff);
	} else if ((page & 0xff00) == 0x0200) {
		// $10000 - $1ffff 64kb rom
		return current_rom_image[address & 0xffff];
	} else if ((page & 0xff00) == 0x0400) {
		// c64 charrom
		switch (address & 0b1) {
			case 0b0:
				return machine.blitter->cbm_font[((address >> 1) & 0x3fff)] >> 8;
			case 0b1:
				return machine.blitter->cbm_font[((address >> 1) & 0x3fff)] & 0xff;
			default:
				return 0;
		}
	} else if ((page & 0xff00) == 0x0500) {
		// amiga charrom
		switch (address & 0b1) {
			case 0b0:
				return machine.blitter->amiga_font[((address >> 1) & 0x7fff)] >> 8;
			case 0b1:
				return machine.blitter->amiga_font[((address >> 1) & 0x7fff)] & 0xff;
			default:
				return 0;
		}
	} else {
		// use ram
		return machine.blitter->video_memory_read_8(address & 0xffffff);
	}
}

void E64::mmu_ic::write_memory_8(uint32_t address, uint8_t value)
{
	uint16_t page = (address & 0xffffff) >> 8;
	
	if ((page & 0xfff8) == 0x0008) {
		switch (page) {
			// $0800 - $0fff io range will ALWAYS be written to
			case IO_BLITTER:
				machine.blitter->io_write_8(address & 0xff, value);
				break;
			case IO_TIMER_PAGE:
				machine.timer->io_write_8(address & 0xff, value);
				break;
			case IO_CIA_PAGE:
				machine.cia->io_write_8(address & 0xff, value);
				break;
			case IO_SID_PAGE:
			case IO_ANALOG_PAGE:
			case IO_MIXER_PAGE:
				machine.sound->write_byte(address & 0x3ff, value);
				break;
			default:
				// use ram
				machine.blitter->video_memory_write_8(address & 0xffffff, value);
				break;
		}
	} else if ((page & 0xff00) == 0x0100) {
		// $10000 - $1ffff io blit registers (64kb)
		machine.blitter->io_blit_contexts_write_8(address & 0xffff, value);
	} else {
		// now it's ram
		machine.blitter->video_memory_write_8(address & 0xffffff, value);
	}
}

uint16_t E64::mmu_ic::read_memory_16(uint32_t address)
{
	return (read_memory_8(address) << 8) | read_memory_8((address + 1) & 0xffffff);
}

void E64::mmu_ic::write_memory_16(uint32_t address, uint16_t value)
{
	write_memory_8(address, (value & 0xff00) >> 8);
	write_memory_8((address + 1) & 0xffffff, value & 0x00ff);
}

void E64::mmu_ic::update_rom_image()
{
	FILE *f = fopen(host.settings->rom_path, "r");
	
	if (f) {
		printf("[MMU] Found 'rom.bin' in %s, using this image\n",
		       host.settings->settings_dir);
		fread(current_rom_image, 65536, 1, f);
		fclose(f);
	} else {
		printf("[MMU] No 'rom.bin' in %s, using built-in rom\n",
		       host.settings->settings_dir);
		for(int i=0; i<65536; i++) current_rom_image[i] = rom[i];
	}
}

bool E64::mmu_ic::insert_binary(char *file)
{
	uint16_t start_address;
	uint16_t end_address;
	//uint16_t vector;
	
	//const uint8_t magic_code[4] = { 'e'+0x80, '6'+0x80, '4'+0x80, 'x'+0x80 };
	
	FILE *f = fopen(file, "rb");
	uint8_t byte;
	
	if (f) {
		start_address = fgetc(f) << 8;
		start_address |= fgetc(f);
		//vector = fgetc(f) << 8;
		//vector |= fgetc(f);
		
		end_address = start_address;
		
		while(end_address) {
			byte = fgetc(f);
			if( feof(f) ) {
				break;
			}
			write_memory_8(end_address++, byte);
		}
		fclose(f);
		hud.show_notification("%s\n\n"
				      "loading $%04x bytes from $%04x to $%04x",
				      file,
				      end_address - start_address,
				      start_address,
				      end_address);
		printf("[MMU] %s\n"
		       "[MMU] Loading $%04x bytes from $%04x to $%04x\n",
		       file,
		       end_address - start_address,
		       start_address,
		       end_address);
		
		// also update some ram vectors of guest os
		write_memory_8(OS_FILE_START_ADDRESS, start_address >> 8);
		write_memory_8(OS_FILE_START_ADDRESS+1, start_address & 0xff);
		write_memory_8(OS_FILE_END_ADDRESS, end_address >> 8);
		write_memory_8(OS_FILE_END_ADDRESS+1, end_address & 0xff);

		return true;
	} else {
		hud.blitter->terminal_printf(hud.terminal->number, "[MMU] Error: can't open %s\n", file);
		return false;
	}
}
