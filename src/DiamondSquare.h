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

} DiamondSquare_s;

DiamondSquare_s * DSCreate(int power, rng_state_t * rand);


#endif /* DIAMONDSQUARE_H_ */
