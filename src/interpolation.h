

#include <SDL2/SDL.h>

#ifndef INTERPOLATION_H_
#define INTERPOLATION_H_

SDL_Color interpolate_colors( SDL_Color c1, SDL_Color c2, float fraction);

SDL_Color water_color(float sealevel, float height);

SDL_Color alpine_gradient(float sealevel, float height);

SDL_Color greyscale_gradient(float max, float current);

SDL_Color shade( SDL_Color initial, float slope, float maxslope);

#endif
