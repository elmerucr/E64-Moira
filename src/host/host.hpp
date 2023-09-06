/*
 * host.hpp
 * E64
 *
 * Copyright © 2020-2023 elmerucr. All rights reserved.
 */

#include <SDL2/SDL.h>

#ifndef HOST_HPP
#define HOST_HPP

#include "settings.hpp"
#include "video.hpp"

namespace E64
{

class host_t {
public:
	host_t();
	~host_t();
	
	char *sdl_preference_path;
	
	settings_t *settings;
	video_t *video;
};

}

#endif
