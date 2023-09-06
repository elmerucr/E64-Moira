/*
 * host.cpp
 * E64
 *
 * Copyright © 2020-2023 elmerucr. All rights reserved.
 */

#include <cstdio>

#include "host.hpp"
#include "common.hpp"

E64::host_t::host_t()
{
	printf("[Host] E64 version %i.%i.%i (C)2019-%i elmerucr\n",
	       E64_MAJOR_VERSION,
	       E64_MINOR_VERSION,
	       E64_BUILD, E64_YEAR);
	
	SDL_Init(SDL_INIT_VIDEO);

	SDL_version compiled;
	SDL_VERSION(&compiled);
	printf("[SDL] Compiled against SDL version %d.%d.%d\n", compiled.major, compiled.minor, compiled.patch);
	
	SDL_version linked;
	SDL_GetVersion(&linked);
	printf("[SDL] Linked against SDL version %d.%d.%d\n", linked.major, linked.minor, linked.patch);

	char *base_path = SDL_GetBasePath();
	printf("[SDL] Base path is: %s\n", base_path);
	SDL_free(base_path);

	sdl_preference_path = SDL_GetPrefPath("elmerucr", "E64");
	printf("[SDL] Preference path is: %s\n", sdl_preference_path);
	
	settings = new settings_t();
	video = new video_t();
}

E64::host_t::~host_t()
{
	printf("[Host] closing E64\n");
	delete settings;
	delete video;
	
	SDL_free(sdl_preference_path);
	
	SDL_Quit();
}
