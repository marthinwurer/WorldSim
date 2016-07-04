/**

    Interoplation.c

    @author Benjamin Maitland

    this file contains all logic for interpolating color cradients to floats
*/

#include <SDL2/SDL.h>

SDL_Color interpolate_colors( SDL_Color c1, SDL_Color c2, float fraction){
    SDL_Color toReturn = c1;
    float recip = 1.0 - fraction;
    toReturn.r = ((float)c2.r * fraction ) + ((float)c1.r * recip);
    toReturn.a = ((float)c2.a * fraction ) + ((float)c1.a * recip);
    toReturn.g = ((float)c2.g * fraction ) + ((float)c1.g * recip);
    toReturn.b = ((float)c2.b * fraction ) + ((float)c1.b * recip);
    return toReturn;
}


SDL_Color alpine_gradient(float sealevel, float height){
    //                       r   g   b   a
    static SDL_Color black = {0, 0, 0, 255};
    static SDL_Color blue = {0, 0, 255, 255};
    static SDL_Color green = {0, 255, 0, 255};
    static SDL_Color dgreen = {56, 124, 68, 255};
    static SDL_Color brown = {111, 78, 55, 255};
    static SDL_Color white = {255, 255, 255, 255};

    if( height < sealevel ){
        return interpolate_colors(black, blue, height / sealevel);
    }
    else if( height < 2.0/3.0)
    {
        return interpolate_colors(green, dgreen, (height - sealevel)/( 2.0/3.0 - sealevel));
    }
    else if( height < 5.0/6.0)
    {
        return interpolate_colors( dgreen, brown, (height - 2.0/3.0) / (1.0/6.0));
    }
    else
    {
        return interpolate_colors( brown, white, (height - 5.0/6.0) / (1.0/6.0));
    }


}

SDL_Color greyscale_gradient(float max, float current){
    static SDL_Color black = {0, 0, 0, 255};
    static SDL_Color white = {255, 255, 255, 255};

    if( current >= max){
    	return white;
    }
    else{
    	return interpolate_colors(black, white, current/max);
    }

}

