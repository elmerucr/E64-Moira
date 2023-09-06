/*
 * m68k.hpp
 * E64
 *
 * Copyright © 2022-2023 elmerucr. All rights reserved.
 */

#ifndef M68K_HPP
#define M68K_HPP

#include "Moira.h"

using namespace moira;

namespace E64
{

class m68k_ic : public Moira {
	u8   read8 (u32 addr) const override;
	u16  read16(u32 addr) const override;
	void write8 (u32 addr, u8  val) const override;
	void write16(u32 addr, u16 val) const override;
	void breakpointReached(u32 addr) override;
public:
	void status(char *text_buffer);
	void stacks(char *text_buffer, int no);
	i64 old_clock;
	bool breakpoint_reached;
};

}

#endif
