/*
 * DiamondSquare.h
 *
 *  Created on: May 2, 2016
 *      Author: benjamin
 */

#ifndef DIAMONDSQUARE_H_
#define DIAMONDSQUARE_H_

#include "xoroshiro128plus.h"
#include "map2d.h"

map2d * DSCreate(int power, rng_state_t * rand);

#endif /* DIAMONDSQUARE_H_ */
