/*
 * utilities.h
 *
 *  Created on: May 2, 2016
 *      Author: benjamin
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

#include "map2d.h"

float bilinearInterpolation(
		float fracx, float fracy,
		float negxposy, float posxposy, float negxnegy, float posxnegy);





static inline int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}

#define MIN(a, b) ((a) > (b) ? (b) : (a))
static inline float min(float a, float b)
{
    return a > b ? b : a;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
static inline float max(float a, float b)
{
    return a > b ? a : b;
}

int check_nan(map2d * data, const char * file, int line);

int signof(float val);

#endif /* UTILITIES_H_ */
