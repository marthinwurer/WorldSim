

#include <SDL2/SDL.h>

#ifndef INTERPOLATION_H_
#define INTERPOLATION_H_
SDL_Color alpine_gradient(float sealevel, float height);

SDL_Color greyscale_gradient(float max, float current);

SDL_Color shade( SDL_Color initial, float slope, float maxslope);

#endif
