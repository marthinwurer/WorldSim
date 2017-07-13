

#include "utilities.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

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

int check_nan(map2d * data, const char * file, int line){
	for( int yy = 0; yy < data->height; yy++){
		for( int xx = 0; xx < data->width; xx++){
			if( isnan(value(data, xx, yy))){
				printf("NaN at (%d, %d), %s:%d\n", xx, yy, file, line);
				exit(123);
			}
		}
	}
	return 0;
}

float cn(float data, const char * file, int line){
	if( isnan(data)){
		printf("NaN at %s:%d\n", file, line);
		exit(123);
	}
	return data;
}



int signof(float val){
	return (val > 0) - (val < 0);
}










