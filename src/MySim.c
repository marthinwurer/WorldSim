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

const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 1024;

const int FRACTAL_POWER = 10;


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
    DiamondSquare_s * fractal = DSCreate(FRACTAL_POWER, &rand);


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

        // ENTER THE MAIN GAME LOOP
    for(;;){


        // process all events in the queue.
        while( SDL_PollEvent( &event )){
            switch( event.type){
                case SDL_QUIT:
                    exit(0);
                    break;
                
                case SDL_KEYDOWN:
                    DSDelete( fractal );
                    fractal = DSCreate(FRACTAL_POWER, &rand);
                    break;
            }
        }
        
    
        //Clear screen
        SDL_SetRenderDrawColor( gRenderer, 0, 0, 0, 0xFF );
        SDL_RenderClear( gRenderer );
    
        for( int yy = 0; yy < fractal->height; yy++){
            for( int xx = 0; xx < fractal->width; xx++){
    
                Uint8 color = fractal->values[xx + yy * fractal->height] * 255;
                drawPoint(gRenderer, xx, yy, color, color, color, 0xFF);
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

