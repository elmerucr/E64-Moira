/*
 * video.hpp
 * E64
 *
 * Copyright © 2020-2023 elmerucr. All rights reserved.
 */

#include <SDL2/SDL.h>

#ifndef VIDEO_HPP
#define VIDEO_HPP

namespace E64 {

struct window_size {
	uint16_t x;
	uint16_t y;
};

class video_t {
private:
	const struct window_size window_sizes[7] = {
		{  512, 320 },
		{  640, 400 },
		{  768, 480 },
		{  960, 600 },
		{ 1024, 640 },
		{ 1152, 720 },
		{ 1280, 800 }
	};
	
//	SDL_Rect vm_screen_size;
//	SDL_Rect scanline_screen_size;
	//SDL_Rect hud_screen_size;  ==> Not needed, texture is fully blitted (Nullptr in rendercopy)
	
//	uint8_t current_vm_screen_width{64};
//	uint8_t current_vm_screen_height{40};
	
	SDL_Window *window;
	SDL_Renderer *renderer;
	bool vsync;
	SDL_Texture *vm_texture;
	SDL_Texture *hud_texture;
	SDL_Texture *scanlines_texture;
	uint8_t current_window_size;
	bool fullscreen;
	int window_width;
	int window_height;
	
	uint16_t *scanline_buffer;
	
	void create_vm_texture(bool linear_filtering);
	void create_hud_texture(bool linear_filtering);
	void create_scanlines_texture(bool linear_filtering);
	void create_textures();
	
	uint8_t scanlines_alpha;
	bool vm_linear_filtering;
	bool hud_linear_filtering;
	bool scanlines_linear_filtering;
public:
	video_t();
	~video_t();
	
	void update_textures();
	void update_screen();
	void update_title();
	void increase_window_size();
	void decrease_window_size();
	void toggle_fullscreen();
	void change_scanlines_intensity();
	void toggle_linear_filtering();
    
	// getters
	uint16_t current_window_width() { return window_sizes[current_window_size].x; }
	uint16_t current_window_height() { return window_sizes[current_window_size].y; }
	inline bool vsync_enabled() { return vsync; }
	inline bool vsync_disabled() { return !vsync; }
	
	inline uint8_t get_scanlines_alpha() { return scanlines_alpha; }
	inline bool is_using_vm_linear_filtering() { return vm_linear_filtering; }
	inline bool is_using_hud_linear_filtering() { return hud_linear_filtering; }
	inline bool is_using_scanlines_linear_filtering() { return scanlines_linear_filtering; }
	inline bool is_fullscreen() { return fullscreen; }
};

}

#endif
