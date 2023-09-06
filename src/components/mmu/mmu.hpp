/*
 * mmu.hpp
 * E64
 *
 * Copyright © 2019-2023 elmerucr. All rights reserved.
*/

#ifndef MMU_HPP
#define MMU_HPP

/*
 * Situation 14/1/2023
 *
 * 0x000000 - 0x000007: Reset vectors (read from ROM) (8b / 8b)
 * 0x000008 - 0x0003ff: RAM vectors (1016b / 1kb)
 * 0x000400 - 0x0007ff: Kernel RAM Area (1k / 2kb)
 * 0x000800 - 0x000fff: IO Area (4kb - 2kb = 2kb / 4kb)
 * 0x001000 - 0x00ffff: Kernel RAM Area (60kb / 64kb)
 * 0x010000 - 0x01ffff: IO Blit Contexts (64kb / 128kb)
 * 0x020000 - 0x02ffff: Kernel ROM (64kb / 192kb)
 * 0x030000 - 0x03ffff: Future expansion for kernel (64kb / 256kb)
 * 0x040000 - 0x04ffff: c64 charrom (repeated twice) (64kb / 320kb
 * 0x050000 - 0x05ffff: amiga charrom (64kb / 384kb)
 * 0x060000 - 0x0fffff: Heap + user stack (640kb / 1mb)
 * 0x100000 - 0x1fffff: general RAM (1024kb / 2mb)
 * 0x200000 - 0x3fffff: blitter character data (2mb / 4mb)
 * 0x400000 - 0x5fffff: blitter fg color data  (2mb / 6mb)
 * 0x600000 - 0x7fffff: blitter bg color data  (2mb / 8mb)
 * 0x800000 - 0xffffff: blitter pixel data     (8mb / 16mb)
 */

#include <cstdint>
#include <cstdlib>

#define IO_BLITTER		0x0008
#define IO_TIMER_PAGE		0x0009
#define IO_CIA_PAGE		0x000a
#define IO_SID_PAGE		0x000c
#define IO_ANALOG_PAGE		0x000d
#define IO_MIXER_PAGE		0x000e

namespace E64
{

class mmu_ic {
public:
	void reset();

	uint8_t read_memory_8(uint32_t address);
	void    write_memory_8(uint32_t address, uint8_t value);
	
	uint16_t read_memory_16(uint32_t address);
	void     write_memory_16(uint32_t address, uint16_t value);
	
	uint8_t  current_rom_image[65536];
	
	void update_rom_image();
	
	bool insert_binary(char *file);
};

}

#endif
