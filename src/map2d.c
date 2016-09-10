/*
 * map2d.c
 *
 *  Created on: Jul 3, 2016
 *      Author: benjamin
 */

#include <stdlib.h> // malloc
#include <string.h> // memset


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
