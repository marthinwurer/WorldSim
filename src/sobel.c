/*
 * sobel.c
 *
 *  Created on: Jul 2, 2016
 *      Author: benjamin
 */

#include <stdlib.h>
#include <math.h>

#include "DiamondSquare.h"

map2d * sobel_gradient(map2d * initial, float * maxval ){
	map2d * toReturn = new_map2d(initial->width, initial->height);

	float max = 0.0;

	for( int xx = 0; xx < initial->width; xx++){
		for( int yy = 0; yy < initial->height; yy++){

			// get the 8 surrounding values
			float ul = value(initial, xx - 1, yy - 1);
			float u  = value(initial, xx - 0, yy - 1);
			float ur = value(initial, xx + 1, yy - 1);
			float l  = value(initial, xx - 1, yy - 0);
			float r  = value(initial, xx + 1, yy - 0);
			float bl = value(initial, xx - 1, yy + 1);
			float b  = value(initial, xx + 0, yy + 1);
			float br = value(initial, xx + 1, yy + 1);

			// calculate Gx and Gy
			float horizontal = ur - ul + r + r - l - l + br - bl;
			float vertical = ur + u + u + ul - br - b - b - bl;
			horizontal /= 8.0;  // division by 8 gets the real slope
			vertical /= 8.0;

			// get the final gradient value
			float gradient = sqrtf( horizontal * horizontal + vertical * vertical );
			// set the sign of the slope
			if( horizontal < 0){
				gradient *= -1.0f;
			}

			map_set( toReturn, xx, yy, gradient);


			if( gradient > max){
				max = gradient;
			}

		}
	}

	if( maxval != NULL){
		*maxval = max;
	}

	return toReturn;
}
