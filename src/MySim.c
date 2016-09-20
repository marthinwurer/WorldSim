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
#include <sys/time.h>
#include <unistd.h>


#include "xoroshiro128plus.h"
#include "DiamondSquare.h"
#include "interpolation.h"
#include "sobel.h"
#include "erosion.h"
#include "tectonics.h"
#include "threadpool.h"

#define RENDER_SCREEN

const int SCREEN_WIDTH = 1024; // the width of the screen in pixels
const int SCREEN_HEIGHT = 1024; // the height of the screen in pixels

const int FRACTAL_POWER = 10; // the power of two that represents the current map size
const int NUM_THREADS = 4; // the number of threads to use in the threadpool

const float MIN_WATER = 0.02; // the minimum amount of water where the tile will be seen as having water in it.

rng_state_t my_rand;
threadpool_t * thread_pool;


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

void drawRect(SDL_Renderer* renderer,
        int           x,
        int           y,
        Uint8         r,
        Uint8         g,
        Uint8         b,
        Uint8         a){
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_Rect rect = {x, y, 1, SCREEN_HEIGHT - y};
    SDL_RenderFillRect(renderer, &rect);
}


int main(void) {
    time_t currtime;

    time(&currtime);
    seed(&my_rand, (int) currtime, currtime * 3); // seed the RNG

    struct timeval start, end;

    long mtime, seconds, useconds;

    gettimeofday(&start, NULL);
    gettimeofday(&end, NULL);

    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;

    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

    printf("Elapsed time: %ld milliseconds\n", mtime);


    // initialize the threadpool
    thread_pool = threadpool_create(NUM_THREADS, NUM_THREADS*2, 0);

    float maxval = 0.0;
    map2d * fractal = DSCreate(FRACTAL_POWER, &my_rand); // the current heightmap
//    fractal = new_map2d(fractal->width, fractal->height);
    map2d * gradient = sobel_gradient(fractal, &maxval); // the slope of the current heightmap
//    map_set(fractal, 300, 300, 100);
	map2d * boundaries = DSCreate(FRACTAL_POWER, &my_rand); // plate boundaries

	map2d * water = new_map2d(fractal->width, fractal->height); // the water map
	map2d * rain_map = DSCreate(FRACTAL_POWER, &my_rand); // the rain map

	// calculate the sea level difference and save it in the water map
	// directly assign those points that are less than 0.5
	for( int yy = 0; yy < fractal->height; yy++){
		for( int xx = 0; xx < fractal->width; xx++){
			if( value(fractal, xx, yy) < 0.5){
				map_set(water, xx, yy, 0.5 - value(fractal, xx, yy) );
			}
		}
	}

    float maxwater = evaporate(water);

    map2d ** momentums = water_pipes(fractal->width, fractal->width);




#ifdef RENDER_SCREEN
    //The window we'll be rendering to
    SDL_Window* window = NULL;

    //Initialize SDL
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
    {
        printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
        exit(0);
    }

    //Create window
    window = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED,
    		SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
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
#endif

        // ENTER THE MAIN GAME LOOP
    for(;;){
        gettimeofday(&start, NULL);

    	printf("max %f, first %f, maxwater %f\n", maxval, gradient->values[0], maxwater);
    	fflush(stdout);

#ifdef RENDER_SCREEN
        // process all events in the queue.
        while( SDL_PollEvent( &event )){
            switch( event.type){
                case SDL_QUIT:
                    exit(0);
                    break;
                
                case SDL_KEYDOWN:
                	switch( event.key.keysym.sym){
                	case(SDLK_d): // generate new boundaries for plates and rain.
                		map2d_delete(boundaries);

                		boundaries = DSCreate(FRACTAL_POWER, &my_rand);
                		map2d_delete(rain_map);
                		rain_map = DSCreate(FRACTAL_POWER, &my_rand);

                		//                		dispDS( rain_map );
                		//                		fflush(stdout);
                		//                		sleep(4);
						break;

                	case(SDLK_r): // generate a new map
						map2d_delete( fractal );
						map2d_delete( gradient);
						map2d_delete(water);
						fractal = DSCreate(FRACTAL_POWER, &my_rand);
						gradient = sobel_gradient(fractal, &maxval);
						water = new_map2d(fractal->width, fractal->height);
						// calculate the sea level difference and save it in the water map
						// directly assign those points that are less than 0.5
						for( int yy = 0; yy < fractal->height; yy++){
							for( int xx = 0; xx < fractal->width; xx++){
								if( value(fractal, xx, yy) < 0.5){
									map_set(water, xx, yy, 0.5 - value(fractal, xx, yy) );
								}
							}
						}
						break;

                	case(SDLK_w): // change display modes
                		display_mode = (display_mode + 1) % 2;

                	}
                	break;
            }
        }

        //Clear screen
        SDL_SetRenderDrawColor( gRenderer, 0, 0, 0, 0xFF );
        SDL_RenderClear( gRenderer );


        // do the top down view
        if( display_mode == 0){

        	for( int yy = 0; yy < fractal->height; yy++){
        		for( int xx = 0; xx < fractal->width; xx++){

        			SDL_Color color;
        			color = alpine_gradient(0.5f, fractal->values[xx + yy * fractal->height]);
        			if( value(water, xx, yy) > MIN_WATER){
        				color = interpolate_colors(
        						color,
								water_color(0.5f, value(water, xx, yy)+fractal->values[xx + yy * fractal->height]),
								(value(water, xx, yy)/2 + .25) / 0.5);
        			}
        			color = shade( color, value(gradient, xx, yy), 0.008);

        			//                SDL_Color color = greyscale_gradient( maxval, gradient->values[xx + yy * fractal->height]);
        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}
        }
        // do the sideways view
        else if (display_mode == 1){
        	int yy = 200;
    		for( int xx = 0; xx < fractal->width; xx++){
				int height = 0;
    			SDL_Color color;

    			// draw the water
    			height = (value(water, xx, yy) + value(fractal, xx, yy)) * SCREEN_HEIGHT / 2;
    			color = water_color(0.5f, 0);
    			drawRect(gRenderer, xx,
    					SCREEN_HEIGHT - height,
						color.r, color.g, color.b, color.a);

    			// draw the land
    			color = alpine_gradient(0.5f, fractal->values[xx + yy * fractal->height]);
    			height = fractal->values[xx + yy * fractal->height] * SCREEN_HEIGHT / 2;


    			drawRect(gRenderer, xx, SCREEN_HEIGHT - height,
    					color.r, color.g, color.b, color.a);
    		}
        }




        //Update screen
        SDL_RenderPresent( gRenderer );
    
        // wait 
//        SDL_Delay( 1000 );
#endif
		map2d * temp = thermal_erosion(fractal);
		map2d_delete(fractal);
		fractal = temp;
		temp = sobel_gradient(fractal, &maxval);
		map2d_delete(gradient);
        gradient = temp;

        rainfall(water, rain_map);
//        printf("afterrain\n");
//        dispDS(water);
        temp = water_movement(water, fractal, momentums);
        map2d_delete(water);
        water = temp;
//        printf("afterflow\n");
//        dispDS(water);

        maxwater = evaporate(water);
//        dispDS(water);





        // do plate tectonics if everything has settled down.
        if(maxval < 0.01){
			temp = basic_tectonics(fractal, boundaries);
			map2d_delete(fractal);
			fractal = temp;
			temp = sobel_gradient(fractal, &maxval);
			map2d_delete(gradient);
	        gradient = temp;

	        // then run two iterations of the thermal erosion algorithm.
			temp = thermal_erosion(fractal);
			map2d_delete(fractal);
			fractal = temp;
			temp = thermal_erosion(fractal);
			map2d_delete(fractal);
			fractal = temp;

        }

        // get the time elapsed
        gettimeofday(&end, NULL);

        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;

        mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

        printf("Elapsed time: %ld milliseconds\n", mtime);
    }
#ifdef RENDER_SCREEN
    //Destroy window
    SDL_DestroyWindow( window );

    //Quit SDL subsystems
    SDL_Quit();
#endif
    return 0;
}

