/*
 * utilities.h
 *
 *  Created on: May 2, 2016
 *      Author: benjamin
 */

#ifndef UTILITIES_H_
#define UTILITIES_H_

float bilinearInterpolation(
		float fracx, float fracy,
		float negxposy, float posxposy, float negxnegy, float posxnegy);





double nextDouble(sfmt_t * rand);

static inline int mod(int a, int b)
{
    int r = a % b;
    return r < 0 ? r + b : r;
}


#endif /* UTILITIES_H_ */
