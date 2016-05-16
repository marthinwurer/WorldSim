/*
 * xoroshiro128plus.h
 *
 *  Created on: May 14, 2016
 *      Author: benjamin
 */

#ifndef SRC_XOROSHIRO128PLUS_H_
#define SRC_XOROSHIRO128PLUS_H_

#include <stdint.h>

typedef struct State {
	uint64_t s[2];
} rng_state_t;

void seed( rng_state_t *state, uint64_t high, uint64_t low) ;

uint64_t next(rng_state_t *state) ;

double nextDouble(rng_state_t *state) ;

#endif /* SRC_XOROSHIRO128PLUS_H_ */
