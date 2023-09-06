/*
 * m68k.cpp
 * E64
 *
 * Copyright © 2022-2023 elmerucr. All rights reserved.
 */

#include "m68k.hpp"
#include "common.hpp"

u8 E64::m68k_ic::read8(u32 addr) const
{
	return machine.mmu->read_memory_8(addr);
}

u16 E64::m68k_ic::read16(u32 addr) const
{
	return (machine.mmu->read_memory_8(addr) << 8) | machine.mmu->read_memory_8(addr+1);
}

void E64::m68k_ic::write8 (u32 addr, u8 val) const
{
	machine.mmu->write_memory_8(addr, val);
}

void E64::m68k_ic::write16(u32 addr, u16 val) const
{
	machine.mmu->write_memory_8(addr, (val & 0xff00) >> 8);
	machine.mmu->write_memory_8(addr + 1, val & 0x00ff);
}

void E64::m68k_ic::breakpointReached(u32 addr)
{
	breakpoint_reached = true;
}

void E64::m68k_ic::status(char *text_buffer)
{
	char stat_reg[32];
	disassembleSR(stat_reg);
	
	sprintf(text_buffer,
		"   PC:%08x D0:%08x A0:%08x\n"
		"               D1:%08x A1:%08x\n"
		"  VBR:%08x D2:%08x A2:%08x\n"
		"  ISP:%08x D3:%08x A3:%08x\n"
		"  MSP:%08x D4:%08x A4:%08x\n"
		"  USP:%08x D5:%08x A5:%08x\n"
		"               D6:%08x A6:%08x\n"
		"   ipl pins:%u  D7:%08x A7:%08x\n\n"
		"      SR:%s (%04x)",
		getPC(),  getD(0), getA(0),
		getD(1), getA(1),
		getVBR(), getD(2), getA(2),
		getISP(), getD(3), getA(3),
		getMSP(), getD(4), getA(4),
		getUSP(), getD(5), getA(5),
		getD(6), getA(6),
		getIPL() & 0b111, getD(7), getA(7),
		stat_reg, getSR()
		);
}

void E64::m68k_ic::stacks(char *text_buffer, int no)
{
	for (int i=no; i != -1; i--) {
		uint32_t isp = (getISP() + i) & 0xffffffff;
		uint32_t msp = (getMSP() + i) & 0xffffffff;
		uint32_t usp = (getUSP() + i) & 0xffffffff;
		text_buffer += sprintf(text_buffer,
				 "%08x %02x  %08x %02x  %08x %02x\n",
				 isp, machine.mmu->read_memory_8(isp),
				 msp, machine.mmu->read_memory_8(msp),
				 usp, machine.mmu->read_memory_8(usp));
	}
	sprintf(text_buffer, "\n   ISP          MSP          USP");
}
