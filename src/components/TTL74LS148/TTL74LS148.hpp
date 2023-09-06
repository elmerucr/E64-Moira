/*
 * TTL74LS148.hpp
 * E64
 *
 * Copyright © 2019-2023 elmerucr. All rights reserved.
 *
 * exception collector and priority encoder, 8 input lines, 3 bit output
 */

#ifndef TTL74LS148_HPP
#define TTL74LS148_HPP

#include <cstdint>
#include "m68k.hpp"

namespace E64
{

class TTL74LS148_ic {
private:
	struct device {
		bool state;
		int level;
	};
	struct device devices[256];
	uint8_t number_of_devices;
	int interrupt_level;
	
	bool m68k_connected;
	m68k_ic *m68k;
public:
	TTL74LS148_ic();
	
	void pull_line(uint8_t handler);
	void release_line(uint8_t handler);
	
	void connect_m68k(m68k_ic *chip);
	
	/*
	 * Recalculates interrupt level based on individual connections
	 */
	void update_interrupt_level();
	
	inline int get_interrupt_level() { return interrupt_level; }
	
	/*
	 * When connecting a device, both a pointer to a pin and an interrupt
	 * level (1-6) must be supplied. Returns a unique interrupt_device_no
	 */
	uint8_t connect_device(int level);
};

}

#endif
