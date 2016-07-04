/*
 * DiamondSquare.h
 *
 *  Created on: May 2, 2016
 *      Author: benjamin
 */

#ifndef DIAMONDSQUARE_H_
#define DIAMONDSQUARE_H_

#include "xoroshiro128plus.h"

typedef struct DiamondSquare_struct{
	int height;
	int width;
	float * values;

} map2d;

map2d * new_map2d(int width, int height);


map2d * DSCreate(int power, rng_state_t * rand);

void DSDelete( map2d * toDelete );

float value( map2d * ds, int x, int y);

void map_set( map2d * ds, int x, int y, float val);

#endif /* DIAMONDSQUARE_H_ */
