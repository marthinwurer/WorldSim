/*
 * DiamondSquare.c
 *
 *  Created on: Apr 23, 2016
 *      Author: benjamin
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "DiamondSquare.h"
#include "utilities.h"
#include "xoroshiro128plus.h"


// indexes into the diamondsquare. For DSCreate only
#define ind(x, y) (toReturn->values[mod(x, dimension) + mod( y, dimension) * dimension])

#define calc(x, y) (mod(x, dimension) + mod( y, dimension) * dimension)

map2d * new_map2d(int width, int height){
    // allocate and initialize the structure and its array.
    map2d * toReturn = malloc( sizeof(map2d));
    toReturn->height = height;
    toReturn->width = width;
        size_t size = sizeof( float ) * width * height;
    toReturn->values = malloc( size);
        memset( toReturn->values, 0, size);

    return toReturn;
}

/*
 * This function gets the value at the given X, Y location in the map.
 */
float value( map2d * ds, int x, int y){
	return ds->values[mod(x, ds->width) + mod( y, ds->height) * ds->height];
}

/*
 * This function sets the value of the given map at the given X, Y location to the value given.
 */
void map_set( map2d * ds, int x, int y, float val){
	ds->values[mod(x, ds->width) + mod( y, ds->height) * ds->height] = val;
}

/*
    dispDS - Display the given DiamondSquare_s
*/
void dispDS( map2d * ds ){
    for( int yy = 0; yy < ds->width; yy++ ){
        for( int xx = 0; xx < ds->height; xx++ ){
                    printf("%f, ", ds->values[
                        mod(xx, ds->width) + mod( yy, ds->height) * ds->height]);
        }
                printf("\n");
    }
}

/*
 * DSCreate - takes a power of two of size, and a random number generator, and
 *      creates a new diamond-square fractal of the given size
 */
map2d * DSCreate(int power, rng_state_t * rand){

    // allocate and initialize the structure and its array.
    int dimension = pow(2, power);
    map2d * toReturn = new_map2d(dimension, dimension);

    // the initial step and height are the dimension
    int step = dimension;
    float height = dimension;

    printf("dimension: %i, squared: %i\n", dimension, dimension * dimension);

    // seed the initial map
    float seed = nextDouble(rand) * height;
    ind(0,0) = seed;

    // store the seed as both the min and the max
    float min = seed;
    float max = seed;

    // do the initial change of the height
    height /= 2.0;

    for(; step > 0; height /= 2.0, step = step / 2 ){
//            dispDS(toReturn);
        printf( "step = %i\n", step);
        int half = step / 2;
        float h2 = height*2.0f;

        // do the diamond step
        for( int yy = 0; yy < dimension; yy += step){
            for( int xx = 0; xx < dimension; xx += step){

                // average the corners
                float average = (ind(xx, yy) +
                        ind( xx + step, yy) +
                        ind(xx, yy + step) +
                        ind(xx + step, yy + step))
                                / 4.0;

                // get the offset
                float offset = nextDouble(rand) * h2 - height;

                float next = average + offset;

                ind(xx + half, yy + half) = next;

                // calculate min and max
                if( next > max ){
                    max = next;
                }
                else if( next < min){
                    min = next;
                }
            }
        }

        // do the square step
        // do the top side, then the left side each iteration
        for( int yy = 0; yy < dimension; yy += step){
            for( int xx = 0; xx < dimension; xx += step){

                // Top Side
                // average the sides
                float average = (ind(xx, yy) +
                        ind( xx + step, yy) +
                        ind(xx + half, yy + half) +
                        ind(xx + half, yy - half))
                                / 4.0;

                // get the offset
                float offset = nextDouble(rand) * h2 - height;

                float next = average + offset;

                ind(xx + half, yy) = next;

                // calculate min and max
                if( next > max ){
                    max = next;
                }
                else if( next < min){
                    min = next;
                }


                // Left Side
                // average the sides
                average = (ind(xx, yy) +
                        ind(xx, yy + step) +
                        ind(xx + half, yy + half) +
                        ind(xx - half, yy + half))
                                / 4.0;

                // get the offset
                offset = nextDouble(rand) * h2 - height;

                next = average + offset;

                ind(xx, yy + half) = next;

                // calculate min and max
                if( next > max ){
                    max = next;
                }
                else if( next < min){
                    min = next;
                }
            }
        }

    }


    // normalize the array
    float difference = max - min;
    for( int yy = 0; yy < dimension; yy++ ){
        for( int xx = 0; xx < dimension; xx++ ){
            ind(xx, yy) = (ind(xx, yy) - min) / difference;
        }
    }

    // return the initialized structure
    return toReturn;
}


/* 
    delete a DiamondSquare
*/
void DSDelete( map2d * toDelete ){
    free( toDelete->values );
    free( toDelete );
}



