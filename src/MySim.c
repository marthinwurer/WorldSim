/*
 ============================================================================
 Name        : MySim.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : A world simulator
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
#include "utilities.h"

#define RENDER_SCREEN

const int SCREEN_WIDTH = 1024; // the width of the screen in pixels
const int SCREEN_HEIGHT = 1024; // the height of the screen in pixels

const int FRACTAL_POWER = 10; // the power of two that represents the current map size
const int NUM_THREADS = 12; // the number of threads to use in the threadpool

float min_water = 0.004; // the minimum amount of water where the tile will be seen as having water in it.

const float BASE_SEA_LEVEL = 0.5;

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

    currtime = 5554;

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
    map2d * oldwatermap = new_map2d(fractal->width, fractal->height);
	map2d * rain_map = DSCreate(FRACTAL_POWER, &my_rand); // the rain map
	map2d * water_gradient = sobel_gradient(fractal, &maxval); // water gradient (show waves


	// process all of the map tiles
	// calculate the sea level difference and save it in the water map
	// directly assign those points that are less than 0.5
	for( int yy = 0; yy < fractal->height; yy++){
		for( int xx = 0; xx < fractal->width; xx++){
//			float val = value(fractal, xx, yy);
//			val = val * val;
////			map_set(fractal, xx, yy, val);
//			if (xx < 200){
//			map_set(fractal, xx, yy, xx/512.0 + val/512.0);
//			}
//			else if (xx <800){
//				map_set(fractal, xx, yy, 200.0/512.0 + val/512.0);
//			}
//			else{
//				map_set(fractal, xx, yy, (xx-600)/512.0 + val/512.0);
//			}
////			map_set(rain_map, xx, yy, xx/1024.0);
			if( value(fractal, xx, yy) < BASE_SEA_LEVEL){
				map_set(water, xx, yy, BASE_SEA_LEVEL - value(fractal, xx, yy) );
			}
		}
	}
//	map_set(fractal, 300, 300, 0.0);
    float vapor; // the amount of water to precipitate.
    float maxwater = evaporate(water, &vapor);
	float watermax = 0;


    map2d ** velocities = water_pipes(fractal->width, fractal->width);
    map2d ** momentums = water_pipes(fractal->width, fractal->width);


    int tectonics = 0;

    int count = 0;

    int cross_section_row = 200;

    int current_velocity = 0;
    float pastvelocities[100];
    for( int ii = 0; ii < 100; ii++){
    	pastvelocities[ii] = 0.0;
    }




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

    	printf("iteration %d, max %f, first %f, maxwater %f, v %f\n",count, maxval, gradient->values[0], maxwater, watermax);
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

//                		map_set(water,200, 200, 1);
                		//                		dispDS( rain_map );
                		//                		fflush(stdout);
                		//                		sleep(4);
						break;

                	case(SDLK_r): // generate a new map
						map2d_delete( fractal );
						map2d_delete( gradient);
						map2d_delete( water );

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

						// wipe the old momentums and velocities.
					    velocities = water_pipes(fractal->width, fractal->width);
					    momentums = water_pipes(fractal->width, fractal->width);


						break;

                	case(SDLK_w): // change display modes
                		display_mode = (display_mode + 1) % 3;
                	break;

                	case(SDLK_t):
                			tectonics = 1;
                	break;

                	case(SDLK_o):
                			cross_section_row--;
                	break;
                	case(SDLK_l):
                			cross_section_row++;
                	break;
                	case(SDLK_i):
                			min_water *= 2;
                	break;
                	case(SDLK_k):
							min_water /= 2;
                	break;

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
        			if( value(water, xx, yy) > min_water){
        				color = interpolate_colors(
        						color,
								water_color(0.5f, value(water, xx, yy)+fractal->values[xx + yy * fractal->height]),
								(value(water, xx, yy)/2 + .25) / 0.5);
        			}
//        			else{
        				color = shade( color, value(gradient, xx, yy), 0.008);
//        			}

        			//                SDL_Color color = greyscale_gradient( maxval, gradient->values[xx + yy * fractal->height]);
        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}

        	// display a small mark where the cross section view is.
        	drawPoint(gRenderer, SCREEN_WIDTH-10, cross_section_row, 255, 255, 255, 255);
        }
        // do the sideways view
        else if (display_mode == 1){
        	int yy = cross_section_row;
    		for( int xx = 0; xx < fractal->width; xx++){
				int height = 0;
    			SDL_Color color;

    			// draw the water
    			height = (value(water, xx, yy) + value(fractal, xx, yy)) * SCREEN_HEIGHT / 4;
    			color = water_color(0.5f, 0);
    			drawRect(gRenderer, xx,
    					SCREEN_HEIGHT - height,
						color.r, color.g, color.b, color.a);

    			// draw the land
    			color = alpine_gradient(0.5f, fractal->values[xx + yy * fractal->height]);
    			height = fractal->values[xx + yy * fractal->height] * SCREEN_HEIGHT / 4;


    			drawRect(gRenderer, xx, SCREEN_HEIGHT - height,
    					color.r, color.g, color.b, color.a);
    		}
        	// display a small mark where the cross section view is.
        	drawPoint(gRenderer, SCREEN_WIDTH-10, cross_section_row, 255, 255, 255, 255);

        }
        // do water velocity
        else if( display_mode == 2){
        	watermax = 0;
        	map2d * watervelocity = new_map2d(fractal->width, fractal->height);

        	for( int yy = 0; yy < fractal->height; yy++){
        		for( int xx = 0; xx < fractal->width; xx++){

        			// sum the water velocity
        			float vel = 0;
        			for( int ii = 0; ii < 8; ii++){
        				vel += velocities[ii]->values[xx + yy * fractal->height];
        			}
        			watervelocity->values[xx + yy * fractal->height] = vel;

        			watermax = max(vel, watermax);
        		}
        	}

        	// update the average velocity over the last 100 cycles.
        	pastvelocities[current_velocity] = watermax;
        	current_velocity = (current_velocity + 1) % 100;
        	float totalv = 0.0;
            for( int ii = 0; ii < 100; ii++){
            	totalv += pastvelocities[ii];
            }
            float averagev = totalv / 100;

        	for( int yy = 0; yy < fractal->height; yy++){
        		for( int xx = 0; xx < fractal->width; xx++){

//        			float weight = 0.025;
        			float weight = averagev;

        			SDL_Color color;
        			color = greyscale_gradient( weight, watervelocity->values[xx + yy * fractal->height]);
        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}
        	map2d_delete(watervelocity);
        }




        //Update screen
        SDL_RenderPresent( gRenderer );

#endif
        map2d * temp;
		temp = thermal_erosion(fractal, water);
		map2d_delete(fractal);
		fractal = temp;
		temp = sobel_gradient(fractal, &maxval);
		map2d_delete(gradient);
        gradient = temp;

//        // test for source and drain
//        map_set(water, 30, 512, 0.0);
//        map_set(water, 900, 512, 1.0);



//        if (count % 4){
        	rainfall(water, rain_map, vapor);
//        }

        temp = water_movement(water, fractal, momentums, velocities);
        map2d_delete(oldwatermap);
        oldwatermap = water;
        water = temp;

		temp = hydraulic_erosion(fractal, oldwatermap, velocities);
		map2d_delete(fractal);
		fractal = temp;

//        if (count % 4 == 3){
        	maxwater = evaporate(water, &vapor);
//        }
//        dispDS(water);





        // do plate tectonics if everything has settled down.
//        if(maxval < 0.01){
        if( tectonics){
			temp = basic_tectonics(fractal, boundaries);
			map2d_delete(fractal);
			fractal = temp;
			temp = sobel_gradient(fractal, &maxval);
			map2d_delete(gradient);
	        gradient = temp;

	        // then run two iterations of the thermal erosion algorithm.
			temp = thermal_erosion(fractal, water);
			map2d_delete(fractal);
			fractal = temp;
			temp = thermal_erosion(fractal, water);
			map2d_delete(fractal);
			fractal = temp;

			tectonics = 0;
        }

        // get the time elapsed
        gettimeofday(&end, NULL);

        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;

        mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

        printf("Elapsed time: %ld milliseconds\n", mtime);
        count ++;
    }
#ifdef RENDER_SCREEN
    //Destroy window
    SDL_DestroyWindow( window );

    //Quit SDL subsystems
    SDL_Quit();
#endif
    return 0;
}

