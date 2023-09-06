/*
 * main.cpp
 * E64
 *
 * Copyright © 2022-2023 elmerucr. All rights reserved.
 */

#include <cstdio>
#include <chrono>
#include <thread>
#include "common.hpp"
#include "hud.hpp"
#include "sdl2.hpp"

#define	CYCLES_PER_STEP	511

/*
 * global components
 */
E64::host_t	host;
E64::hud_t	hud;
E64::stats_t	stats;
E64::machine_t	machine;
bool		app_running;
std::chrono::time_point<std::chrono::steady_clock> start_time, end_time, refresh_moment;

static void finish_frame();

int main(int argc, char **argv)
{
	host.settings->process_command_line_arguments(argc, argv);
	
	E64::sdl2_init();
	
	app_running = true;
	
	hud.reset();
	machine.reset();
	stats.reset();

	/*
	 * Initial machine mode
	 * Must be E64::RUNNING or E64::PAUSED
	 *
	 * TODO: Make this part of the configuration file?
	 */
	machine.mode = E64::RUNNING;
	
	host.video->update_title();
	
	start_time = refresh_moment = std::chrono::steady_clock::now();

	while (app_running) {
		switch (machine.mode) {
			case E64::RUNNING:
				if (machine.run(CYCLES_PER_STEP)) {
					machine.flip_modes();
					hud.blitter->terminal_printf(hud.terminal->number, "breakpoint reached at $%06x\n", machine.m68k->getPC());
				}
				if (machine.frame_done()) finish_frame();
				break;
			case E64::PAUSED:
				hud.run(CYCLES_PER_STEP);
				if (hud.frame_done()) finish_frame();
				break;
		}
	}
	
	end_time = std::chrono::steady_clock::now();
	
	printf("[E64] Virtual machine ran for %.2f seconds\n",
	       (double)std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() / 1000
	);

	E64::sdl2_cleanup();
	return 0;
}

static void finish_frame()
{
	if (E64::sdl2_process_events() == E64::QUIT_EVENT) app_running = false;
	
	if (machine.mode == E64::PAUSED) {
		hud.process_keypress();
		hud.update_views();
	}
	
	hud.update_stats_view();
	hud.redraw();
	
	// time measurement
	stats.start_update_textures_time();

	host.video->update_textures();
	
	// time measurement
	stats.start_idle_time();
	
	/*
	 * If vsync is enabled, the update screen function takes more
	 * time, i.e. it will return after a few milliseconds, exactly
	 * when vertical refresh is done. This will avoid tearing.
	 * There's no need then to let the system sleep with a
	 * calculated value. But we will still have to do a time
	 * measurement for estimation of idle time.
	 *
	 * When there's no vsync, a sleep time is implemented manually.
	 */
	if (host.video->vsync_disabled()) {
		refresh_moment += std::chrono::microseconds(stats.frametime);
		/*
		 * Check if the next update is in the past,
		 * this can be the result of a debug session.
		 * If so, calculate a new update moment. This will
		 * avoid "playing catch-up" by the virtual machine.
		 */
		if (refresh_moment > std::chrono::steady_clock::now()) {
			std::this_thread::sleep_until(refresh_moment);
		} else {
			refresh_moment = std::chrono::steady_clock::now();
		}
	}
	
	host.video->update_screen();
	
	/*
	 * time measurement, starting vm time
	 *
	 * This point marks the start of a new frame, also at this very
	 * moment it's good to measure the soundbuffer size.
	 */
	stats.start_vm_time();
	
	stats.process_parameters();
}
