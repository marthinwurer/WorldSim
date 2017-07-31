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
//#define RENDER_GL
//#define DO_CL

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

#ifdef DO_CL
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
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

//#define DO_EROSION
#define DO_THERMAL_EROSION
#define DO_WATER
#define DO_STAVO_WATER

//#define DO_WEATHER


const int SCREEN_WIDTH = 1024; // the width of the screen in pixels
const int SCREEN_HEIGHT = 1024; // the height of the screen in pixels

const int FRACTAL_POWER = 8; // the power of two that represents the current map size
const int NUM_THREADS = 12; // the number of threads to use in the threadpool

float min_water = 0.1; // the minimum amount of water where the tile will be seen as having water in it.
float height_multiplier = 8192.0f; // the amount the fractal height is multiplied by to find the height.

const float BASE_SEA_LEVEL = 0.5;

// the time in seconds between each timestep
float timestep = 900.0f;

// the square edge length in meters (assume that everything is a perfect square/cube)
//float squarelen = 1156250.0f;
float squarelen = 39063.0f;


// acceleration due to gravity
float gravity = 9.81f;


rng_state_t my_rand;
threadpool_t * thread_pool;


int display_perspective = 0;
int display_mode = 5;

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

void render_map(SDL_Renderer* gRenderer, map2d* disp_map, int x_off, int y_off){

	if( x_off >= SCREEN_WIDTH || y_off >= SCREEN_HEIGHT){
		return;
	}

	float min_v = value(disp_map, 0, 0);
	float max_v = min_v;

	// find the min and max vals
	for( int yy = 0; yy < disp_map->height; yy++ ){
		for( int xx = 0; xx < disp_map->width; xx++ ){
			float val = value(disp_map, xx, yy);
			min_v = min(min_v, val);
			max_v = max(max_v, val);
		}
	}
//	printf("min: %f, max: %f\n", min_v, max_v);

	// normalize them
	float difference = max_v - min_v;
	for( int yy = 0; yy < disp_map->height; yy++ ){
		for( int xx = 0; xx < disp_map->width; xx++ ){
			int ind = m_index(disp_map, xx, yy);
			float val = (disp_map->values[ind] - min_v) / difference;
			SDL_Color color;
			color = greyscale_gradient(1.0, val);
//        			color = alpine_gradient(BASE_SEA_LEVEL, 1.0-val);

			drawPoint(gRenderer, xx + x_off, yy + y_off, color.r, color.g, color.b, color.a);
		}
	}
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
    map2d * heightmap = DSCreate(FRACTAL_POWER, &my_rand); // the current heightmap
    map2d * sediment = DSCreate(FRACTAL_POWER, &my_rand); // the sediment layer. gravel, dirt, sand, etc.
    map2d * gradient = sobel_gradient(heightmap, &maxval); // the slope of the current heightmap
	map2d * boundaries = DSCreate(FRACTAL_POWER, &my_rand); // plate boundaries
	map2d * real_height = new_map2d(heightmap->width, heightmap->height);
	map2d * suspended_material = new_map2d(heightmap->width, heightmap->height);


	map2d * water = new_map2d(heightmap->width, heightmap->height); // the water map
    map2d * oldwatermap = new_map2d(heightmap->width, heightmap->height); // water map from last step
    map2d * ground_water = new_map2d(heightmap->width, heightmap->height); // water in the top level of dirt
    map2d * aquifer_water = new_map2d(heightmap->width, heightmap->height); // water in the auqifer
	map2d * evaporated_water = DSCreate(FRACTAL_POWER, &my_rand); // the rain map
	map2d * water_gradient = sobel_gradient(heightmap, &maxval); // water gradient (show waves

	map2d * pressure = new_map2d(heightmap->width, heightmap->height); // atmospheric pressure at each point
	map2d * surface_temperature = new_map2d(heightmap->width, heightmap->height); // surface real temperature
	map2d * surface_potential_temperature = new_map2d(heightmap->width, heightmap->height); // surface potential temperature
	map2d * convergence = new_map2d(heightmap->width, heightmap->height); // field towards the point
	// these are at the SE corners of each cell.
	map2d * sn_velocity = new_map2d(heightmap->width, heightmap->height); // velocity towards the north of the current tile. U, at corners
	map2d * we_velocity = new_map2d(heightmap->width, heightmap->height); // velocity towards the east of the current tile. V, at corners
	// values at edges
	map2d * sn_v_edge = new_map2d(heightmap->width, heightmap->height); // edge velocity from the south to the north. At edges
	map2d * we_v_edge = new_map2d(heightmap->width, heightmap->height); // edge velocity from the west to the east. At edges
	map2d * tracer = new_map2d(heightmap->width, heightmap->height);

#ifdef DO_STAVO_WATER
	map2d ** velocities = make_array(4, heightmap->width, heightmap->height);
	map2d ** new_velocities = make_array(4, heightmap->width, heightmap->height);
	map2d ** pipes = make_array(4, heightmap->width, heightmap->height);
#else
    map2d ** velocities = water_pipes(heightmap->width, heightmap->width);
    map2d ** momentums = water_pipes(heightmap->width, heightmap->width);

#endif



	int tiles = heightmap->width * heightmap->height;

	// process all of the map tiles
	// calculate the sea level difference and save it in the water map
	// directly assign those points that are less than 0.5
	for( int yy = 0; yy < heightmap->height; yy++){
		for( int xx = 0; xx < heightmap->width; xx++){

#if 0
			/* parabolic */
			float x = xx;
			float y = yy;
			float dim = heightmap->width;
			x = x / dim - 0.5;
			float val = (x * x) / 2.0 + y / dim;

			map_set(heightmap, xx, yy, val);

#elif 0
			/* slopes */
			float val = value(heightmap, xx, yy) * 0.01;
			if (xx < 200){
			map_set(heightmap, xx, yy, xx/512.0 + val/512.0);
			}
			else if (xx <800){
				map_set(heightmap, xx, yy, 200.0/512.0 + val/512.0);
			}
			else{
				map_set(heightmap, xx, yy, (xx-600)/512.0 + val/512.0);
			}
#elif 0
			/* single slope */
			map_set(heightmap, xx, yy, xx/(float)heightmap->width);
#elif 0
			/* flat */
			map_set(heightmap, xx, yy, BASE_SEA_LEVEL);
#elif 0
			float val = 1 / (value(heightmap, xx, yy) + .5) - .7;
			map_set(heightmap, xx, yy, val);


#endif
			if( value(heightmap, xx, yy) < BASE_SEA_LEVEL){
				map_set(water, xx, yy, BASE_SEA_LEVEL - value(heightmap, xx, yy) );
			}

			int index = m_index(heightmap, xx, yy);
			water->values[index] *= height_multiplier;
			heightmap->values[index] *= height_multiplier;


		}
	}
//	map_set(water, 300, 300, 300);

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

	// set up weather things
#ifdef DO_WEATHER

	// set up the pressure and velocity maps
	for( int ii = 0; ii < tiles; ii++){
		// p = rt
		// r = 287.1
		// 287 *
		// do sea level pressure version first
//		pressure->values[ii] = 0.0117*287*temp_map->values[ii];
//		pressure->values[ii] = 1000.0;

		ew_velocity->values[ii] = 0.0;
		ns_velocity->values[ii] = 0.0;
		surface_temperature->values[ii] = 273.15f;
	}
	set_initial_pressures(heightmap, water, surface_temperature, pressure, BASE_SEA_LEVEL * height_multiplier);

//	map_set(ew_velocity, heightmap->width/2, heightmap->height/2, -1000);
//	map_set(ew_velocity, heightmap->width/2, -10, -1000);
	for( int yy = 0; yy < heightmap->height; yy++){
		for( int xx = 0; xx < heightmap->width; xx++){
//			if( xx % 32 == 8 || yy % 32 == 8){
				map_set(tracer, xx, yy, 1.0);
//			}

			if( yy == heightmap->height - 1){
				ns_velocity->values[m_index(ns_velocity, xx, yy)] = 0.0;

			}
		}
	}

#endif

	// get the total water - to be used to unbreak conservation of mass
    double totalwater = 0.0f;

    totalwater = map2d_total(water);
    double vapor; // the amount of water to precipitate.
    float maxwater = evaporate(water, ground_water, &vapor, evaporated_water, surface_temperature);
	float watermax = 0;




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

    // calculate boolean
    int play = 0;
    int step = 1;


    // mouse location
    int mouse_x = 0;
    int mouse_y = 0;



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
//        check_nan(heightmap, __FILE__, __LINE__);


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
							map2d_delete(evaporated_water);
							evaporated_water = DSCreate(FRACTAL_POWER, &my_rand);

	//                		map_set(water,200, 200, 1);
							//                		dispDS( rain_map );
							//                		fflush(stdout);
							//                		sleep(4);
							break;

						case(SDLK_r): // generate a new map
							map2d_delete( heightmap );
							map2d_delete( gradient);
							map2d_delete( water );

							heightmap = DSCreate(FRACTAL_POWER, &my_rand);
							gradient = sobel_gradient(heightmap, &maxval);
							water = new_map2d(heightmap->width, heightmap->height);
							// calculate the sea level difference and save it in the water map
							// directly assign those points that are less than 0.5
							for( int yy = 0; yy < heightmap->height; yy++){
								for( int xx = 0; xx < heightmap->width; xx++){
									if( value(heightmap, xx, yy) < 0.5){
										map_set(water, xx, yy, 0.5 - value(heightmap, xx, yy) );
									}
								}
							}

#ifdef DO_STAVO_WATER
#else
							// wipe the old momentums and velocities.
							velocities = water_pipes(heightmap->width, heightmap->width);
							momentums = water_pipes(heightmap->width, heightmap->width);
#endif

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
						case(SDLK_p):
								play = !play;

						break;
						case(SDLK_s):
								step = !step;

						break;
						case(SDLK_n):
						// zero the tracer and set a grid in the tracer.
						for( int yy = 0; yy < heightmap->height; yy++){
							for( int xx = 0; xx < heightmap->width; xx++){
								if( xx % 2 != yy % 2){
									map_set(tracer, xx, yy, 1.0);
								}
								else{
									map_set(tracer, xx, yy, 0.0);

								}

							}
						}
						break;
						case(SDLK_m):

						// zero the tracer and set a random point to 1.
						for( int yy = 0; yy < heightmap->height; yy++){
							for( int xx = 0; xx < heightmap->width; xx++){
								map_set(tracer, xx, yy, 0.0);

							}
						}
						map_set(tracer, next(&my_rand) % heightmap->height, next(&my_rand) % heightmap->width, 1.0);

						break;


                	}
                	break;
                	case SDL_MOUSEMOTION:
                		mouse_x = event.motion.x;
                		mouse_y = event.motion.y;
                		break;
                	case SDL_MOUSEBUTTONDOWN:
                		map_set(tracer, mouse_x, mouse_y, 1.0);
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

        	for( int yy = 0; yy < heightmap->height; yy++){
        		for( int xx = 0; xx < heightmap->width; xx++){

        			SDL_Color color;
        			color = alpine_gradient(0.5f, heightmap->values[xx + yy * heightmap->height]);
        			if( value(water, xx, yy) > min_water){
        				color = interpolate_colors(
        						color,
								water_color(0.5f, value(water, xx, yy)+heightmap->values[xx + yy * heightmap->height]),
								(value(water, xx, yy) / height_multiplier /2 + .25) / 0.5);
        			}
//        			else{
        				color = shade( color, value(gradient, xx, yy)/height_multiplier, 0.008);
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
    		for( int xx = 0; xx < heightmap->width; xx++){
				int height = 0;
    			SDL_Color color;

    			// draw the water
    			height = (value(water, xx, yy) + value(heightmap, xx, yy)) * SCREEN_HEIGHT / 4;
    			color = water_color(0.5f, 0);
    			drawRect(gRenderer, xx,
    					SCREEN_HEIGHT - height/height_multiplier,
						color.r, color.g, color.b, color.a);

    			// draw the land
    			color = alpine_gradient(0.5f, heightmap->values[xx + yy * heightmap->height]);
    			height = heightmap->values[xx + yy * heightmap->height] * SCREEN_HEIGHT / 4;


    			drawRect(gRenderer, xx, SCREEN_HEIGHT - height/height_multiplier,
    					color.r, color.g, color.b, color.a);
    		}
        	// display a small mark where the cross section view is.
        	drawPoint(gRenderer, SCREEN_WIDTH-10, cross_section_row, 255, 255, 255, 255);

        }
        // do water velocity
        else if( display_mode == 2){
        	watermax = 0;
        	map2d * watervelocity = new_map2d(heightmap->width, heightmap->height);

        	for( int yy = 0; yy < heightmap->height; yy++){
        		for( int xx = 0; xx < heightmap->width; xx++){

        			// sum the water velocity
        			float vel = 0;
#ifdef DO_STAVO_WATER
        			for( int ii = 0; ii < 4; ii++){
        				vel += velocities[ii]->values[xx + yy * heightmap->height];
        			}
#else
        			for( int ii = 0; ii < 8; ii++){
        				vel += velocities[ii]->values[xx + yy * heightmap->height];
        			}
#endif
        			watervelocity->values[xx + yy * heightmap->height] = vel;

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

        	for( int yy = 0; yy < heightmap->height; yy++){
        		for( int xx = 0; xx < heightmap->width; xx++){

//        			float weight = 0.025;
        			float weight = averagev;

        			SDL_Color color;
        			color = greyscale_gradient( weight, watervelocity->values[xx + yy * heightmap->height]);
        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}
        	map2d_delete(watervelocity);
        }
        else if (display_mode == 3){
        	for( int yy = 0; yy < heightmap->height; yy++){
        		for( int xx = 0; xx < heightmap->width; xx++){

        			SDL_Color color;
        			color = greyscale_gradient(0.25f, value(water, xx, yy));

        			//                SDL_Color color = greyscale_gradient( maxval, gradient->values[xx + yy * heightmap->height]);
        			drawPoint(gRenderer, xx, yy, color.r, color.g, color.b, color.a);
        		}
        	}
        }
        	// do openGL rendering
        else if (display_mode == 4){

        }
        else if (display_mode == 5){
//        	map2d * water_divergance = new_map2d(heightmap->width, heightmap->height);
//
//        	for( int yy = 0; yy < heightmap->height; yy++){
//        		for( int xx = 0; xx < heightmap->width; xx++){
//
//        			// sum the water velocity
//        			float vel = 0;
//#ifdef DO_STAVO_WATER
//        			for( int ii = 0; ii < 4; ii++){
//        				vel += velocities[ii]->values[xx + yy * heightmap->height];
//        			}
//#else
//        			for( int ii = 0; ii < 8; ii++){
//        				vel += velocities[ii]->values[xx + yy * heightmap->height];
//        			}
//#endif
//        			water_divergance->values[xx + yy * heightmap->height] = vel;
//
//        			watermax = max(vel, watermax);
//        		}
//        	}

        	map2d * disp_map = pressure;

//        	check_nan(convergence, __FILE__, __LINE__);

        	render_map(gRenderer, disp_map, 0, 0);
        	render_map(gRenderer, convergence, heightmap->width, 0);
        	render_map(gRenderer, tracer, heightmap->width * 2, 0);

        	printf("value at mouse: %f    ", value(disp_map, mouse_x, mouse_y));

        	char title[128];
        	sprintf(title, "value at mouse: %f    ", value(disp_map, mouse_x, mouse_y));

        	SDL_SetWindowTitle(window, title);

        	float min_v = value(disp_map, 0, 0);
        	float max_v = min_v;

        	// find the min and max vals
        	for( int yy = 0; yy < disp_map->height; yy++ ){
        		for( int xx = 0; xx < disp_map->width; xx++ ){
        			float val = value(disp_map, xx, yy);
        			min_v = min(min_v, val);
        			max_v = max(max_v, val);
        		}
        	}
        	printf("min: %f, max: %f\n", min_v, max_v);


        }




        //Update screen
        SDL_RenderPresent( gRenderer );

#endif
        // let me pause execution
        if( play || step){
        	step = 0; // let me step through frames

        	map2d * temp;
        	map2d ** ttemp;

#ifdef DO_EROSION
#ifdef DO_THERMAL_EROSION
        	temp = thermal_erosion(heightmap, water);
        	map2d_delete(heightmap);
        	heightmap = temp;
#endif
#endif


        	temp = sobel_gradient(heightmap, &maxval);
        	map2d_delete(gradient);
        	gradient = temp;

#if 0
        	//        // test for source and drain
        	map_set(water, 30, 512, 0.0);
        	float source = value(water, 500, 900) + vapor;
        	map_set(water, 500, 900, source);
#endif



#ifdef DO_WEATHER
#endif


#ifdef DO_WATER

        	rainfall(water, evaporated_water, vapor);


#ifdef DO_STAVO_WATER

        	stavo_water_movement(heightmap, water, oldwatermap, velocities, new_velocities, pipes);
        	swapmap(&water, &oldwatermap);
        	swapmap2(&velocities, &new_velocities);
//        	check_nan(water, __FILE__, __LINE__);





#else


        	temp = water_movement(water, heightmap, momentums, velocities);
        	map2d_delete(oldwatermap);
        	oldwatermap = water;
        	water = temp;
        	check_nan(heightmap, __FILE__, __LINE__);
        	check_nan(water, __FILE__, __LINE__);

#ifdef DO_EROSION
        	temp = hydraulic_erosion(heightmap, velocities);
        	map2d_delete(heightmap);
        	heightmap = temp;
        	check_nan(heightmap, __FILE__, __LINE__);

#endif

#endif
        	maxwater = evaporate(water, ground_water, &vapor, evaporated_water, surface_temperature);
#endif



        	// do the plate tectonics
        	if( tectonics){
        		temp = basic_tectonics(heightmap, boundaries);
        		map2d_delete(heightmap);
        		heightmap = temp;
        		temp = sobel_gradient(heightmap, &maxval);
        		map2d_delete(gradient);
        		gradient = temp;

        		// then run two iterations of the thermal erosion algorithm.
        		temp = thermal_erosion(heightmap, water);
        		map2d_delete(heightmap);
        		heightmap = temp;
        		temp = thermal_erosion(heightmap, water);
        		map2d_delete(heightmap);
        		heightmap = temp;

        		tectonics = 0;
        	}

        }
//        play = 0;
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

