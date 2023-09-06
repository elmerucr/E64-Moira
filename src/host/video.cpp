/*
 * video.cpp
 * E64
 *
 * Copyright © 2020-2023 elmerucr. All rights reserved.
 */

#include "video.hpp"
#include "common.hpp"
#include <cstring>

E64::video_t::video_t()
{
	//SDL_Init(SDL_INIT_VIDEO);

	// print the list of video backends
	int num_video_drivers = SDL_GetNumVideoDrivers();
	printf("[SDL Display] %d video backend(s) compiled into SDL: ",
	       num_video_drivers);
	for (int i=0; i<num_video_drivers; i++)
		printf(" \'%s\' ", SDL_GetVideoDriver(i));
	printf("\n");
	printf("[SDL Display] Now using backend '%s'\n", SDL_GetCurrentVideoDriver());

	/* default starts at 3
	 * TODO: add config option to lua?
	 */
	current_window_size = 3;
	
	/*
	 * Start with windowed screen
	 */
	fullscreen = false;

	/*
	 * Create window - title will be set later on by update_title()
	 * Note: Usage of SDL_WINDOW_ALLOW_HIGHDPI actually helps: interpolation
	 * of pixels at unlogical window sizes looks a lot better!
	 */
	window = SDL_CreateWindow("", SDL_WINDOWPOS_CENTERED,
				  SDL_WINDOWPOS_CENTERED,
				  window_sizes[current_window_size].x,
				  window_sizes[current_window_size].y,
				  SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
				  SDL_WINDOW_ALLOW_HIGHDPI);
    
	SDL_GetWindowSize(window, &window_width, &window_height);
	printf("[SDL Display] Window dimension: %u x %u pixels\n",
	       window_width, window_height);
	
	update_title();
	
	/*
	 * now change to former window/fullscreen setting
	 */
	if (host.settings->fullscreen_at_init) {
		toggle_fullscreen();
	}

	SDL_DisplayMode current_mode;

	SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &current_mode);

	printf("[SDL Display] Current desktop dimension: %i x %i\n",
	       current_mode.w, current_mode.h);

	printf("[SDL Display] refresh rate of current display is %iHz\n",
	       current_mode.refresh_rate);
	
	/*
	 * Create renderer and link it to window
	 */
    
	if (current_mode.refresh_rate == FPS) {
		printf("[SDL Display] this is equal to the FPS of E64, trying for vsync\n");
		renderer = SDL_CreateRenderer(window, -1,
					      SDL_RENDERER_ACCELERATED |
					      SDL_RENDERER_TARGETTEXTURE |
					      SDL_RENDERER_PRESENTVSYNC);
	} else {
		printf("[SDL Display] this differs from the FPS of E64, going for software FPS\n");
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
	}
	
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);	// black used for clearing actions

	/*
	 * Setting the logical size fixes aspect ratio
	 */
	//SDL_RenderSetLogicalSize(renderer, PIXELS_PER_SCANLINE, SCANLINES);

	SDL_RendererInfo current_renderer;
	SDL_GetRendererInfo(renderer, &current_renderer);
	vsync = (current_renderer.flags & SDL_RENDERER_PRESENTVSYNC) ? true : false;

	printf("[SDL Renderer Name] %s\n", current_renderer.name);
	printf("[SDL Renderer] %saccelerated\n",
	       (current_renderer.flags & SDL_RENDERER_ACCELERATED) ? "" : "not ");
	printf("[SDL Renderer] vsync is %s\n", vsync ? "enabled" : "disabled");
	
	/*
	 * Retrieve linear filtering from settings
	 */
	vm_linear_filtering = host.settings->vm_linear_filtering_at_init;
	hud_linear_filtering = host.settings->hud_linear_filtering_at_init;
	scanlines_linear_filtering = host.settings->scanlines_linear_filtering_at_init;

	/*
	 * Create two textures that are able to refresh very frequently
	 */
	vm_texture = nullptr;
	hud_texture = nullptr;
	create_vm_texture(vm_linear_filtering);
	create_hud_texture(hud_linear_filtering);
	
	/*
	 * Scanlines: A static texture that mimics scanlines
	 */
	scanline_buffer = new uint16_t[4 * VM_MAX_PIXELS_PER_SCANLINE * VM_MAX_SCANLINES];
	scanlines_texture = nullptr;
	create_scanlines_texture(scanlines_linear_filtering);

	/*
	 * Make sure mouse cursor isn't visible
	 */
	SDL_ShowCursor(SDL_DISABLE);
	
	scanlines_alpha = host.settings->scanlines_alpha_at_init;
}

E64::video_t::~video_t()
{
	printf("[SDL] cleaning up video\n");
	
	SDL_DestroyTexture(scanlines_texture);
	SDL_DestroyTexture(hud_texture);
	SDL_DestroyTexture(vm_texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	//SDL_Quit();
	
	delete [] scanline_buffer;
}

void E64::video_t::create_vm_texture(bool linear_filtering)
{
	if (vm_texture) SDL_DestroyTexture(vm_texture);
	
	if (linear_filtering) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	} else {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	}
	
	vm_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB4444,
				    SDL_TEXTUREACCESS_STREAMING,
				    VM_MAX_PIXELS_PER_SCANLINE, VM_MAX_SCANLINES);
	SDL_SetTextureBlendMode(vm_texture, SDL_BLENDMODE_NONE);
}

void E64::video_t::create_hud_texture(bool linear_filtering)
{
	if (hud_texture) SDL_DestroyTexture(hud_texture);
	
	if (linear_filtering) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	} else {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	}

	hud_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB4444,
				    SDL_TEXTUREACCESS_STREAMING,
				    HUD_PIXELS_PER_SCANLINE, HUD_SCANLINES);
	SDL_SetTextureBlendMode(hud_texture, SDL_BLENDMODE_BLEND);
}

void E64::video_t::create_scanlines_texture(bool linear_filtering)
{
	if (scanlines_texture) SDL_DestroyTexture(scanlines_texture);

	if (linear_filtering) {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	} else {
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
	}
	
	scanlines_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB4444,
				    SDL_TEXTUREACCESS_STATIC,
				    VM_MAX_PIXELS_PER_SCANLINE, 4 * VM_MAX_SCANLINES);
	SDL_SetTextureBlendMode(scanlines_texture, SDL_BLENDMODE_BLEND);
	
	for (int i=0; i<4*VM_MAX_SCANLINES; i++) {
		for (int j=0; j < VM_MAX_PIXELS_PER_SCANLINE; j++) {
			uint16_t color;
			switch (i & 0b11) {
				case 0b00: color = 0xf000; break;
				case 0b01: color = 0x0000; break;
				case 0b10: color = 0x0000; break;
				case 0b11: color = 0xf000; break;
				default:   color = 0x0000;
			};
			scanline_buffer[(i * VM_MAX_PIXELS_PER_SCANLINE) + j] = color;
		}
	}
	SDL_UpdateTexture(scanlines_texture, NULL, scanline_buffer,
		VM_MAX_PIXELS_PER_SCANLINE * sizeof(uint16_t));
}

void E64::video_t::update_textures()
{
	SDL_UpdateTexture(vm_texture, NULL, machine.blitter->fb, VM_MAX_PIXELS_PER_SCANLINE * sizeof(uint16_t));
	SDL_UpdateTexture(hud_texture, NULL, hud.blitter->fb, HUD_PIXELS_PER_SCANLINE * sizeof(uint16_t));
}

void E64::video_t::update_screen()
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	
	SDL_RenderCopy(renderer, vm_texture, &machine.blitter->screen_size, NULL);
	SDL_SetTextureAlphaMod(scanlines_texture, scanlines_alpha);
	SDL_RenderCopy(renderer, scanlines_texture, &machine.blitter->scanline_screen_size, NULL);
	
	SDL_RenderCopy(renderer, hud_texture, NULL, NULL);

	SDL_RenderPresent(renderer);
}

void E64::video_t::increase_window_size()
{
	if (current_window_size < 6) current_window_size++;
	SDL_SetWindowSize(window, window_sizes[current_window_size].x,
			  window_sizes[current_window_size].y);
	SDL_GetWindowSize(window, &window_width, &window_height);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	hud.show_notification("Set host window size to %ix%i", window_width, window_height);
}

void E64::video_t::decrease_window_size()
{
	if (current_window_size > 0) current_window_size--;
	SDL_SetWindowSize(window, window_sizes[current_window_size].x,
			  window_sizes[current_window_size].y);
	SDL_GetWindowSize(window, &window_width, &window_height);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	hud.show_notification("Set host window size to %ix%i", window_width, window_height);
}

void E64::video_t::toggle_fullscreen()
{
	fullscreen = !fullscreen;
	if (fullscreen) {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	} else {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_RESIZABLE);
	}
	SDL_GetWindowSize(window, &window_width, &window_height);
	hud.show_notification("Switched to %s mode with size %ix%i",
			      fullscreen ? "fullscreen" : "window",
			      window_width,
			      window_height);
}

void E64::video_t::update_title()
{
	if (machine.mode == E64::PAUSED) {
		SDL_SetWindowTitle(window, "E64 Debug Mode");
		// TODO: ?
		//SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
	} else {
		SDL_SetWindowTitle(window, "E64");
		// TODO: ?
		//SDL_SetWindowIcon(SDL_Window *window, SDL_Surface *icon);
	}
}

void E64::video_t::change_scanlines_intensity()
{
	if (scanlines_alpha < 64) {
		scanlines_alpha = 64;
	} else if (scanlines_alpha < 128) {
		scanlines_alpha = 128;
	} else if (scanlines_alpha < 192) {
		scanlines_alpha = 192;
	} else if (scanlines_alpha < 255) {
		scanlines_alpha = 255;
	} else {
		scanlines_alpha = 0;
	}
	hud.show_notification("                 scanlines alpha = %3u/255", scanlines_alpha);
}

void E64::video_t::toggle_linear_filtering()
{
	switch (machine.mode) {
		case E64::RUNNING:
			vm_linear_filtering = !vm_linear_filtering;
			create_vm_texture(vm_linear_filtering);
			hud.show_notification("                   vm linear filtering = %s", vm_linear_filtering ? "on" : "off");
			break;
		case E64::PAUSED:
			hud_linear_filtering = !hud_linear_filtering;
			create_hud_texture(hud_linear_filtering);
			hud.show_notification("                  hud linear filtering = %s", hud_linear_filtering ? "on" : "off");
			break;
		default:
			break;
	}
}
