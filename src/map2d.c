/*
 * map2d.c
 *
 *  Created on: Jul 3, 2016
 *      Author: benjamin
 */

#include <stdlib.h> // malloc
#include <string.h> // memset
#include <stdio.h> // io for dispDS


#include "map2d.h"
#include "utilities.h"


map2d * new_map2d(int width, int height){
    // allocate and initialize the structure and its array.
    map2d * toReturn = malloc( sizeof(map2d));
    toReturn->height = height;
    toReturn->width = width;
        size_t size = sizeof( float ) * width * height;
    toReturn->values = malloc( size);
        memset( toReturn->values, 0, size);

    return toReturn;
}

/*
 * This function gets the value at the given X, Y location in the map.
 */
float value( map2d * ds, int x, int y){
	return ds->values[mod(x, ds->width) + mod( y, ds->height) * ds->height];
}

/*
 * This function calculates the index of a given x, y location for the given map.
 */
int m_index(  map2d * ds, int x, int y){
	return mod(x, ds->width) + mod( y, ds->height) * ds->height;
}

/*
 * This function sets the value of the given map at the given X, Y location to the value given.
 */
void map_set( map2d * ds, int x, int y, float val){
	ds->values[mod(x, ds->width) + mod( y, ds->height) * ds->height] = val;
}


/*
    delete a map2d
*/
void map2d_delete( map2d * toDelete ){
    free( toDelete->values );
    free( toDelete );
}


/*
    dispDS - Display the given DiamondSquare_s
*/
void dispDS( map2d * ds ){
	printf("\n");
    for( int yy = 0; yy < ds->width; yy++ ){
        for( int xx = 0; xx < ds->height; xx++ ){
                    printf("%f, ", ds->values[
                        mod(xx, ds->width) + mod( yy, ds->height) * ds->height]);
        }
                printf("\n");
    }
    fflush(stdout);
}


/*
 * This function totals the values in the map.
 */
double map2d_total( map2d * ds )
{
	double total = 0.0;
	for( int ii = 0; ii < ds->height * ds->width; ii++){
		total += ds->values[ii];
	}
	return total;

}

double map2d_min( map2d * ds )
{
	double min_val = ds->values[0];
	for( int ii = 0; ii < ds->height * ds->width; ii++){
		min_val = min( ds->values[ii], min_val);
	}
	return min_val;

}



/**
 * This function normalizes all values in the map to be within 0 and 1. modifies the given array.
 */
void normalize(map2d * ds){
	// find the max and min values

    float max = ds->values[0];
    float min = ds->values[0];

    for( int yy = 0; yy < ds->height; yy++ ){
        for( int xx = 0; xx < ds->width; xx++ ){
        	int index = m_index(ds, xx, yy);
        	float val = ds->values[index];
        	if( val > max){
        		max = val;
        	}
        	if( val < min){
        		min = val;
        	}
        }
    }
    // normalize the array
    float difference = max - min;
    for( int yy = 0; yy < ds->height; yy++ ){
        for( int xx = 0; xx < ds->width; xx++ ){
        	int index = m_index(ds, xx, yy);
        	ds->values[index] = (ds->values[index] - min) / difference;

        }
    }

}


map2d ** make_array(int size, int x, int y){

	map2d** toreturn = calloc(size, sizeof(map2d *));

	for( int ii = 0; ii < size; ii++){
		toreturn[ii] = new_map2d(x, y);
	}
	return toreturn;
}

void swapmap(map2d ** a, map2d ** b){
	map2d * temp = *a;
	*a = *b;
	*b = temp;
}
void swapmap2(map2d *** a, map2d *** b){
	map2d ** temp = *a;
	*a = *b;
	*b = temp;
}


































