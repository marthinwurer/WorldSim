/**
    This file contains the erosion algorithms.

*/


#include <stdlib.h>
#include <stdio.h>
#include "xoroshiro128plus.h"
#include "DiamondSquare.h"
#include "map2d.h"

const float diagdist = 1.41421356237;
const float diagval = 1.0/1.41421356237;

const float mindist = 0.008;

// TODO: change min difference for diagonals - currently causing artifacts

/**
 * do the basic thermal erosion algorithm as described in Balazs Jako's paper.
 */

map2d * thermal_erosion(map2d * input){
	// make a new map to return.
	map2d * toReturn = new_map2d(input->width, input->height);

	// make an array of eight pointers to each of the pipes.
	map2d ** pipes = malloc( 8 * sizeof( map2d *));

	// make all the pipes
	for( int ii = 0; ii < 8; ii++)
	{
		pipes[ii] = new_map2d(input->width, input->height);
	}

//	printf("\n");

	// loop through all the positions on the board
	for( int yy = 0; yy < input->height; yy++){
		for( int xx = 0; xx < input->width; xx++){
			// get the values of all the neighboring cells
			// 0 1 2
			// 7 x 3
			// 6 5 4
			float current = value(input, xx, yy);
			int indexes[8];
			indexes[0] = m_index(input, xx - 1, yy - 1);
			indexes[1] = m_index(input, xx, yy - 1);
			indexes[2] = m_index(input, xx + 1, yy - 1);
			indexes[3] = m_index(input, xx + 1, yy);
			indexes[4] = m_index(input, xx + 1, yy + 1);
			indexes[5] = m_index(input, xx, yy + 1);
			indexes[6] = m_index(input, xx - 1, yy + 1);
			indexes[7] = m_index(input, xx - 1, yy);

			float differences[8];
			float proportions[8];
			float maxdiff = 0;
			float count = 0;
			// compute the differences and also store the max difference
			// count the number of lower neighbors there are with weights
			for( int ii = 0; ii < 8; ii++){

				differences[ii] = current - input->values[indexes[ii]];
				if(differences[ii] > maxdiff){
					maxdiff = differences[ii];
				}
				if(differences[ii] > (ii % 2 ? mindist : diagdist * mindist)){
					// ternary to save code
					proportions[ii] = (differences[ii] - mindist) * (ii % 2 ? 1 : diagval);
					count += proportions[ii];
				}
				else{
					proportions[ii] = 0;
				}
			}

			// test for the case of none being moved (too low of angle)
			if(count == 0){
				maxdiff = 0;
			}

			// calculate the total to be moved.
			float total = maxdiff / 4;
			float left = current - total;
			float totalmoved = 0;

			// dont do the extra work; on GPU would be pipelined through
//			if( count != 0){
				// move that proportion to that tile.
				for( int ii = 0; ii < 8; ii++){
					float tomove = 0;
					if( count != 0){
						tomove = total * (proportions[ii]/count);
					}
					// assign it to the pipe
					pipes[ii]->values[indexes[ii]] = tomove;
					totalmoved += tomove;
				}
//			}
//			printf("%f, ",count);

			// remove the moved volume
			map_set(toReturn, xx, yy, left);

		}
//		printf("\n");
	}

	// debug code
//	dispDS(toReturn);
//	dispDS(pipes[0]);


	// reduce part of map-reduce
	for( int yy = 0; yy < input->height; yy++){
		for( int xx = 0; xx < input->width; xx++){
			// get the current value
			float current = value(toReturn, xx, yy);
			// add the pipe values to it
			for( int ii = 0; ii < 8; ii ++){
				current += value(pipes[ii], xx, yy);
			}
			// assign it back
			map_set(toReturn, xx, yy, current);
		}
	}

	//free everything
	for( int ii = 0; ii < 8; ii++){
		map2d_delete(pipes[ii]);
	}
	free( pipes);

	return toReturn;
}
