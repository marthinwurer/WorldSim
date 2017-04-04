/*
 ============================================================================
 Name        : MySim.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : A world simulator
 ============================================================================
 */

#define _BSD_SOURCE

#define RENDER_SCREEN
#define RENDER_GL

#include <stdio.h>
#include <stdlib.h>

#ifdef RENDER_SCREEN
#include <SDL2/SDL.h>
#endif
#ifdef RENDER_GL

// GLEW
#define GLEW_STATIC
#include <GL/glew.h>
// GLFW
#include <GLFW/glfw3.h>

#endif

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

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
#include "weather.h"

#define DO_EROSION
//#define DO_THERMAL_EROSION


const int SCREEN_WIDTH = 1024; // the width of the screen in pixels
const int SCREEN_HEIGHT = 1024; // the height of the screen in pixels

const int FRACTAL_POWER = 5; // the power of two that represents the current map size
const int NUM_THREADS = 12; // the number of threads to use in the threadpool

float min_water = 0.00001; // the minimum amount of water where the tile will be seen as having water in it.

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

#ifdef RENDER_SCREEN
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
#endif


int main(void) {
    time_t currtime;

    time(&currtime);

    // the traditional seed is 5554
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

    /////////////////////////////////////////
    // MAPS
    /////////////////////////////////////////

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
#if 0
			/* parabolic */
			float x = xx;
			float y = yy;
			float dim = fractal->width;
			x = x / dim - 0.5;
			float val = (x * x) / 2.0 + y / dim;

			map_set(fractal, xx, yy, val);
			printf("%f ", val);

#elif 0
			/* slopes */
			float val = value(fractal, xx, yy) * 0.01;
			if (xx < 200){
			map_set(fractal, xx, yy, xx/512.0 + val/512.0);
			}
			else if (xx <800){
				map_set(fractal, xx, yy, 200.0/512.0 + val/512.0);
			}
			else{
				map_set(fractal, xx, yy, (xx-600)/512.0 + val/512.0);
			}
#elif 0
			/* single slope */
			map_set(rain_map, xx, yy, xx/1024.0);
#endif
			if( value(fractal, xx, yy) < BASE_SEA_LEVEL){
				map_set(water, xx, yy, BASE_SEA_LEVEL - value(fractal, xx, yy) );
			}
		}
//		printf("\n");
	}
//    // loop until everything is flattened out.
//	int loopcount = 0;
//	for(;;){
//		map2d * temp;
//		temp = thermal_erosion(fractal, water);
//		map2d_delete(fractal);
//		fractal = temp;
//		if (loopcount % 50 == 0){
//			temp = sobel_gradient(fractal, &maxval);
//			map2d_delete(gradient);
//	        gradient = temp;
//	        if(maxval <= 0.009f){
//	        	break;
//	        }
//		}
//	}


	// get the total water - to be used to unbreak conservation of mass
    double totalwater = 0.0f;

    totalwater = map2d_total(water);
//	map_set(fractal, 300, 300, 0.0);
    double vapor; // the amount of water to precipitate.
    float maxwater = evaporate(water, &vapor, rain_map);
	float watermax = 0;


    map2d ** velocities = water_pipes(fractal->width, fractal->width);
    map2d ** momentums = water_pipes(fractal->width, fractal->width);


    int tectonics = 0;

    int count = 0;

    int cross_section_row = 200;

    // set up the past velocty stuff.
    int current_velocity = 0;
    float pastvelocities[100];
    for( int ii = 0; ii < 100; ii++){
    	pastvelocities[ii] = 0.0;
    }

    // display boolean
    int display = 1;





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
    window = SDL_CreateWindow( "Erosion Simulation", SDL_WINDOWPOS_UNDEFINED,
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

    ///////////////////////////////////////////////////////
    // OPENGL SETUP
    ///////////////////////////////////////////////////////
#ifdef RENDER_GL

    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

    //OpenGL context
    SDL_GLContext gContext;
    gContext = SDL_GL_CreateContext( window );
    if( gContext == NULL )
    {
    	printf( "OpenGL context could not be created! SDL Error: %s\n", SDL_GetError() );
    	exit(0);
    }

    //Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if( glewError != GLEW_OK )
    {
    	printf( "Error initializing GLEW! %s\n", glewGetErrorString( glewError ) );
    	exit(0);
    }

    //Use Vsync
    if( SDL_GL_SetSwapInterval( 1 ) < 0 )
    {
    	printf( "Warning: Unable to set VSync! SDL Error: %s\n", SDL_GetError() );
    }

    // set up the rendering area
    uint32_t width, height;
    SDL_GL_GetDrawableSize(window, &width, &height);

    glViewport(0, 0, width, height);

    // set up the z buffer
    glEnable(GL_DEPTH_TEST);


    // set up blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



#endif
#endif


        // ENTER THE MAIN LOOP
    for(;;){
        gettimeofday(&start, NULL);

    	printf("iteration %d, max %f, first %f, v %f\n",count, maxval, gradient->values[0], vapor);
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
							display_mode = (display_mode + 1) % 6;
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
						case(SDLK_u):
								display += 1;
								display %= 2;
						break;

                	}
                	break;
            }
        }

        //Clear screen
        SDL_SetRenderDrawColor( gRenderer, 0, 0, 0, 0xFF );
        SDL_RenderClear( gRenderer );


        if( ! display){
        	// if running with no screen updates, do nothing!
        }
        // do the top down view
        else if( display_mode == 0){

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

        			//SDL_Color color = greyscale_gradient( maxval, gradient->values[xx + yy * fractal->height]);
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
        	pastvelocities[current_velocity] = watermax * 0.8f;
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
        else if (display_mode == 3){
        	for( int yy = 0; yy < fractal->height; yy++){
        		for( int xx = 0; xx < fractal->width; xx++){

        			SDL_Color color;
        			color = greyscale_gradient(0.25f, value(water, xx, yy));

        			//                SDL_Color color = greyscale_gradient( maxval, gradient->values[xx + yy * fractal->height]);
        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}
        }
        	// do openGL rendering
        else if (display_mode == 4){

        }
        else if (display_mode == 5){
        	map2d * temp_map = temp_map_from_heightmap(fractal, BASE_SEA_LEVEL, 1.0);
        	float min_v = value(temp_map, 0, 0);
        	float max_v = min_v;

        	// find the min and max vals
        	for( int yy = 0; yy < temp_map->height; yy++ ){
        		for( int xx = 0; xx < temp_map->width; xx++ ){
        			float val = value(temp_map, xx, yy);
        			min_v = min(min_v, val);
        			max_v = max(max_v, val);
        		}
        	}

        	// normalize them
        	float difference = max_v - min_v;
        	for( int yy = 0; yy < temp_map->height; yy++ ){
        		for( int xx = 0; xx < temp_map->width; xx++ ){
        			int ind = m_index(temp_map, xx, yy);
        			float val = (temp_map->values[ind] - min_v) / difference;
        			SDL_Color color;
        			color = greyscale_gradient(1.0, val);

        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}

        	map2d_delete(temp_map);


        }




        //Update screen
        SDL_RenderPresent( gRenderer );

#endif

        map2d * temp;

#ifdef DO_EROSION
#ifdef DO_THERMAL_EROSION
		temp = thermal_erosion(fractal, water);
		map2d_delete(fractal);
		fractal = temp;
#endif
#endif


		temp = sobel_gradient(fractal, &maxval);
		map2d_delete(gradient);
        gradient = temp;

#if 0
//        // test for source and drain
        map_set(water, 30, 512, 0.0);
        float source = value(water, 500, 900) + vapor;
        map_set(water, 500, 900, source);
#endif



        rainfall(water, rain_map, vapor);

        temp = water_movement(water, fractal, momentums, velocities);
        map2d_delete(oldwatermap);
        oldwatermap = water;
        water = temp;

#ifdef DO_EROSION
		temp = hydraulic_erosion(fractal, oldwatermap, velocities);
		map2d_delete(fractal);
		fractal = temp;
#endif
		maxwater = evaporate(water, &vapor, rain_map);





		// do the plate tectonics
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

        // do monitoring calulations
////        if( count % 2 == -1)
//        {
//        	double currwater = map2d_total(water);
//        	// adjust the rainfall to compensate for gaining of mass.
//        	if(currwater > totalwater){
//        		printf("It's borked, irreparable. %f %f\n", currwater, totalwater);
//        		return 1;
//        	}
//        	vapor = totalwater - currwater;
//
//        }

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

