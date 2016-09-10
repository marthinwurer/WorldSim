/*
 ============================================================================
 Name        : MySim.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <time.h>


#include "xoroshiro128plus.h"
#include "DiamondSquare.h"
#include "interpolation.h"
#include "sobel.h"

const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 1024;

const int FRACTAL_POWER = 10;

int display_perspective = 0;
int display_mode = 0;

/*#version 430

in vec3 fragPos;
in vec3 fragNorm;
in vec2 fragTexUV;
uniform sampler2D sampler;
out vec4 fragColor;

void main()
{
	vec3 lightDir = normalize(vec3(0,0,1));
	float lamb = max(dot(lightDir, fragNorm), 0);
	vec3 viewDir = normalize(-fragPos);
	vec3 halfDir = normalize(lightDir + viewDir);
	float spcAngl = max(dot(halfDir, fragNorm), 0);
	float specular = pow(spcAngl, 16.0);
	float light = .1 + .6 * lamb + .3 * specular;
	fragColor = texture(sampler, fragTexUV) * light;
}

sam's lighting shader

this is slightly hacked
but this is a light shining in the direction of the positive z axis
given by lightdir
lamb (lambda) is the dot of the light direction with the normal
then some specular lighting stuff

the actual amount of light on a given pixel emitted from the shader is a base of .1
(so things are not completely black when no light is shining on it), then .6 defined
 by the light direction, then .3 defined by the specular reflection amount
for you I may want to use a light value of like .3 + .7 * dotOfLightDirAndNormal

learnopengl.com



*/


void drawPoint(SDL_Renderer* renderer,
        int           x,
        int           y,
        Uint8         r,
        Uint8         g,
        Uint8         b,
        Uint8         a){
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderDrawPoint(renderer, x, y);
}


int main(void) {
    time_t currtime;
    rng_state_t rand;

    time(&currtime);
    seed(&rand, (int) currtime, currtime * 3);


    printf( "%i\n", 1/2);

    puts("!!!Hello World!!!"); /* prints !!!Hello World!!! */
    printf("%u\n", (unsigned int) next(&rand));
    printf("%u\n", (unsigned int) next(&rand));
    printf("%u\n", (unsigned int) next(&rand));
    printf("%u\n", (unsigned int) next(&rand));
    printf("%u\n", (unsigned int) next(&rand));
    float maxval = 0.0;
    map2d * fractal = DSCreate(FRACTAL_POWER, &rand);
    map2d * gradient = sobel_gradient(fractal, &maxval);


    //The window we'll be rendering to
    SDL_Window* window = NULL;

    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        exit(0);
    }

    //Create window
    window = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
    if( window == NULL )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        exit(0);
    }
    //Create renderer for window
    SDL_Renderer* gRenderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED );
    if( gRenderer == NULL )
    {
        printf( "Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
        exit(0);
    }

    // event union
    SDL_Event event;

    // the current display state


        // ENTER THE MAIN GAME LOOP
    for(;;){


        // process all events in the queue.
        while( SDL_PollEvent( &event )){
            switch( event.type){
                case SDL_QUIT:
                    exit(0);
                    break;
                
                case SDL_KEYDOWN:
                    map2d_delete( fractal );
                    map2d_delete( gradient);
                    fractal = DSCreate(FRACTAL_POWER, &rand);
                    gradient = sobel_gradient(fractal, &maxval);

                    break;
            }
        }
        
    
        //Clear screen
        SDL_SetRenderDrawColor( gRenderer, 0, 0, 0, 0xFF );
        SDL_RenderClear( gRenderer );
    
        for( int yy = 0; yy < fractal->height; yy++){
            for( int xx = 0; xx < fractal->width; xx++){
    
                SDL_Color color = alpine_gradient(0.5f, fractal->values[xx + yy * fractal->height]);
//                SDL_Color color = greyscale_gradient( maxval, gradient->values[xx + yy * fractal->height]);
                drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
            }
        }
        //Update screen
        SDL_RenderPresent( gRenderer );
    
        // wait 
        SDL_Delay( 200 );
    
    }

    //Destroy window
    SDL_DestroyWindow( window );

    //Quit SDL subsystems
    SDL_Quit();

    return 0;
}

