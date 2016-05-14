
#include <SFMT.h>

#include "utilities.h"

float bilinearInterpolation(
		float fracx, float fracy,
		float negxposy, float posxposy, float negxnegy, float posxnegy){

	return  (1 - fracx) *
			((1 - fracy) * negxnegy +
					fracy * negxposy) +
					fracx *
					((1 - fracy) * posxnegy +
							fracy * posxposy);
}

double nextDouble(sfmt_t * rand){
	return sfmt_genrand_real1(rand);
}
