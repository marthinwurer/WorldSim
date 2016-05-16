/*  Written in 2016 by David Blackman and Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty.

See <http://creativecommons.org/publicdomain/zero/1.0/>. */

#include <stdint.h>

#include "xoroshiro128plus.h"

/* This is the successor to xorshift128+. It is the fastest full-period
   generator passing BigCrush without systematic failures, but due to the
   relatively short period it is acceptable only for applications with a
   mild amount of parallelism; otherwise, use a xorshift1024* generator.

   Beside passing BigCrush, this generator passes the PractRand test suite
   up to (and included) 16TB, with the exception of binary rank tests,
   which fail due to the lowest bit being an LFSR; all other bits pass all
   tests. We suggest to use a sign test to extract a random Boolean value.
   
   Note that the generator uses a simulated rotate operation, which most C
   compilers will turn into a single instruction. In Java, you can use
   Long.rotateLeft(). In languages that do not make low-level rotation
   instructions accessible xorshift128+ could be faster.

   The state must be seeded so that it is not everywhere zero. If you have
   a 64-bit seed, we suggest to seed a splitmix64 generator and use its
   output to fill s. */

static inline uint64_t rotl(const uint64_t x, int k) {
	return (x << k) | (x >> (64 - k));
}

uint64_t next(rng_state_t *state) {
	const uint64_t s0 = state->s[0];
	uint64_t s1 = state->s[1];
	const uint64_t result = s0 + s1;

	s1 ^= s0;
	state->s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14); // a, b
	state->s[1] = rotl(s1, 36); // c

	return result;
}


/* This is the jump function for the generator. It is equivalent
   to 2^64 calls to next(); it can be used to generate 2^64
   non-overlapping subsequences for parallel computations. */

void jump(rng_state_t *state) {
	static const uint64_t JUMP[] = { 0xbeac0467eba5facb, 0xd86b048b86aa9922 };

	uint64_t s0 = 0;
	uint64_t s1 = 0;
	for(int i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
		for(int b = 0; b < 64; b++) {
			if (JUMP[i] & 1ULL << b) {
				s0 ^= state->s[0];
				s1 ^= state->s[1];
			}
			next(state);
		}

	state->s[0] = s0;
	state->s[1] = s1;
}

/*  With the exception of generators designed to provide directly 
    double-precision floating-point numbers, the fastest way to 
    convert in C99 a 64-bit unsigned integer x to a 64-bit double is: */


static inline double to_double(uint64_t x) {
       const union { uint64_t i; double d; } u = { .i = UINT64_C(0x3FF) << 52 | x >> 12 };
       return u.d - 1.0;
}

/*  The code above cooks up by bit manipulation a real number in the
    interval [1..2), and then subtracts one to obtain a real number
    in the interval [0..1). If x is chosen uniformly among 64-bit
    integers, d is chosen uniformly among dyadic rationals of the form:
        k / 2^−52.
*/




/** 
    Get the next double in the range [0, 1)
*/
double nextDouble(rng_state_t *state){
    return to_double(next( state ));
}

/**
    Seed the state
*/
void seed( rng_state_t *state, uint64_t high, uint64_t low) {
    state->s[0] = high;
    state->s[1] = low;
}
