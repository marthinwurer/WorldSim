/*
 * DiamondSquare.c
 *
 *  Created on: Apr 23, 2016
 *      Author: benjamin
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SFMT.h>

#include "DiamondSquare.h"
#include "utilities.h"

#define ind(x, y) (toReturn->values[mod(x, dimension) + mod( y, dimension) * dimension])

#define calc(x, y) (mod(x, dimension) + mod( y, dimension) * dimension)

/*
 * DSCreate - takes a power of two of size, and a random number generator, and
 * 		creates a new diamond-square fractal of the given size
 */
DiamondSquare_s * DSCreate(int power, sfmt_t * rand){

	DiamondSquare_s * toReturn = malloc( sizeof(DiamondSquare_s));
	int dimension = pow(2, power);
	toReturn->height = dimension;
	toReturn->width = dimension;
	toReturn->values = malloc( sizeof( float ) * dimension * dimension);


	float min = 0.0;
	float max = 0.0;

	int step = dimension;
	float height = dimension;

	printf("dimension: %i, squared: %i\n", dimension, dimension * dimension);

	// seed the initial map
	float seed = nextDouble(rand) * height;
	ind(0,0) = seed;
	min = seed;
	max = seed;

	height /= 2.0;

	for(; step > 1; height /= 2.0, step = step / 2 ){
		printf( "step = %i\n", step);
		int half = step * 2;
		float h2 = height/2.0f;

		// do the diamond step
		for( int yy = 0; yy < dimension; yy += step){
			for( int xx = 0; xx < dimension; xx += step){

//				printf("%i %i %i %i %i %i\n", xx, yy, calc(xx, yy) ,
//						calc( xx + step, yy) ,
//						calc(xx, yy + step) ,
//						calc(xx + step, yy + step));
//

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

//				printf("top %i %i %i %i %i %i\n", xx, yy, calc(xx, yy) ,
//						calc( xx + step, yy) ,
//						calc(xx + half, yy + half)  ,
//						calc(xx + half, yy - half));
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
//				printf("left %i %i %i %i %i %i\n", xx, yy, calc(xx, yy) ,
//						calc( xx , yy + step) ,
//						calc(xx + half, yy + half)  ,
//						calc(xx - half, yy + half));
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


	return toReturn;
}
