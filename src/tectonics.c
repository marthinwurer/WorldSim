/*
 * tectonics.c
 *
 *  Created on: Sep 16, 2016
 *      Author: benjamin
 */

#include "map2d.h"
#include "DiamondSquare.h"
extern const int FRACTAL_POWER;
extern rng_state_t my_rand;


/*
 * Crappy tectonics algorithm. Just move eveything in the plate 1 in a direction.
 */
map2d * basic_tectonics(map2d * input, map2d * boundaries){

	// create one to return
	map2d * toReturn = new_map2d(input->width, input->height);

	// make a new Diamond-square to determine plate boundaries

	// loop twice, once for greater than and once for less than

	// directly assign those points that are less than 0.5
	for( int yy = 0; yy < input->height; yy++){
		for( int xx = 0; xx < input->width; xx++){
			int index = m_index(input, xx, yy);
			int move = m_index(input, xx + 1, yy + 1);

			if( boundaries->values[index] > 0.5){
				toReturn->values[index] += input->values[index];
			}else{
				toReturn->values[move] += input->values[index];
			}

		}
	}


	return toReturn;
}
