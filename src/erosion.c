/**
    This file contains the erosion algorithms.

*/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "xoroshiro128plus.h"
#include "DiamondSquare.h"
#include "map2d.h"
#include "threadpool.h"
#include "utilities.h"

const float diagdist = 1.41421356237;
const float diagval = 1.0/1.41421356237;

const float mindist = 0.008;

const float RAIN_CONSTANT = 0.016;

extern const int NUM_THREADS;
extern threadpool_t * thread_pool;

// a struct to be passed into the threadpool
struct poolpram_s{
	map2d *restrict input;
	map2d **restrict pipes;
	map2d *restrict toReturn;
	int offset;
	int increment;
	int done;
};

/**
 * function to actually do the erosion to make everything restrict
 */

/**
 * A function to send to the pool
 */
void erode_thread( void * arg){
//	printf("%p \n", arg);
	struct poolpram_s * param = (struct poolpram_s *)arg;

	// do the thermal erosion algorithm

	// loop through all the positions on the board
	for( int yy = param->offset; yy < param->input->height; yy += param->increment){
		for( int xx = 0; xx < param->input->width; xx++){
			// get the values of all the neighboring cells
			// 0 1 2
			// 7 x 3
			// 6 5 4
			float current = value(param->input, xx, yy);
			int indexes[8];
			indexes[0] = m_index(param->input, xx - 1, yy - 1);
			indexes[1] = m_index(param->input, xx, yy - 1);
			indexes[2] = m_index(param->input, xx + 1, yy - 1);
			indexes[3] = m_index(param->input, xx + 1, yy);
			indexes[4] = m_index(param->input, xx + 1, yy + 1);
			indexes[5] = m_index(param->input, xx, yy + 1);
			indexes[6] = m_index(param->input, xx - 1, yy + 1);
			indexes[7] = m_index(param->input, xx - 1, yy);

			float differences[8];
			float proportions[8];
			float maxdiff = 0;
			float count = 0;
			// compute the differences and also store the max difference
			// count the number of lower neighbors there are with weights
			for( int ii = 0; ii < 8; ii++){

				differences[ii] = current - param->input->values[indexes[ii]];
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
			float total = maxdiff / 3;
			float left = current - total;
			float totalmoved = 0;


			// move that proportion to that tile.
			for( int ii = 0; ii < 8; ii++){
				float tomove = 0;
				if( count != 0){
					tomove = total * (proportions[ii]/count);
				}
				// assign it to the pipe
				param->pipes[ii]->values[indexes[ii]] = tomove;
				totalmoved += tomove;
			}


			// remove the moved volume
			map_set(param->toReturn, xx, yy, left);

		}
	}
	param->done = 1;
}

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
#define POOL_THREAD
#ifdef POOL_THREAD

	// make an array of pool structs.
	struct poolpram_s ** pool_data = malloc( NUM_THREADS * sizeof( struct poolpram_s *));

	// make the structs and start the threads
	for( int ii = 0; ii < NUM_THREADS; ii++){
		pool_data[ii] = malloc( sizeof( struct poolpram_s));
		pool_data[ii]->done = 0;
		pool_data[ii]->input = input;
		pool_data[ii]->pipes = pipes;
		pool_data[ii]->toReturn = toReturn;
		pool_data[ii]->offset = ii;
		pool_data[ii]->increment = NUM_THREADS;

		threadpool_add(thread_pool, erode_thread, (void *)(pool_data[ii]), 0);
	}

	// wait for all of the threads to finish
	for( int ii = 0; ii < NUM_THREADS; ii++){
		while(pool_data[ii]->done == 0){
			usleep(1);
		}
		free(pool_data[ii]);
	}
	free(pool_data);
#else

	struct poolpram_s * pool_data  = malloc( sizeof( struct poolpram_s));
	pool_data->done = 0;
	pool_data->input = input;
	pool_data->pipes = pipes;
	pool_data->toReturn = toReturn;
	pool_data->offset = 0;
	pool_data->increment = 1;

	threadpool_add(thread_pool, erode_thread, (void *)(pool_data), 0);

	while(pool_data->done == 0){
		usleep(1);
	}

	free(pool_data);

#endif



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


/**
 * Do basic evaporation: remove half of the water from the tile.
 *
 * MODIFIES THE INPUT
 */
float evaporate(map2d * input){
	float maxval = 0.0;
	for( int yy = 0; yy < input->height; yy++){
		for( int xx = 0; xx < input->width; xx++){
			int ii = m_index(input, xx, yy);
			input->values[ii] -= RAIN_CONSTANT * 1.01f;

			input->values[ii] = max(input->values[ii], 0.0);

			if( input->values[ii] > maxval){
				maxval = input->values[ii];
			}
		}
	}
	return maxval;
}

/**
 * Do basic rainfall: add a portion of the rain map to the .
 *
 * MODIFIES THE INPUT
 */
void rainfall(map2d *restrict input, map2d *restrict rain_map){
	for( int yy = 0; yy < input->height; yy++){
		for( int xx = 0; xx < input->width; xx++){
			int ii = m_index(input, xx, yy);
			input->values[ii] = input->values[ii] + RAIN_CONSTANT;
		}
	}
}

//#ifdef nope
#define WATER_INDEXES 4
/**
 * Move the water depending on the height of the surrounding terrain and the water there.
 */
map2d * water_movement(map2d * restrict water, map2d * restrict height){

	// make a new map to return.
	map2d * toReturn = new_map2d(water->width, water->height);

	// make an array of eight pointers to each of the pipes.
	map2d ** pipes = malloc( 8 * sizeof( map2d *));

	// make all the pipes
	for( int ii = 0; ii < 8; ii++)
	{
		pipes[ii] = new_map2d(water->width, water->height);
	}

	// do the water calculations

	// loop through all the positions on the board
	for( int yy = 0; yy < water->height; yy ++){
		for( int xx = 0; xx < water->width; xx++){
			// get the values of all the neighboring cells
			// 0 1 2
			// 7 x 3
			// 6 5 4
			float currwater = value(water, xx, yy);
			float currheight = value(height, xx, yy);
			float current = currwater + currheight;

			int indexes[WATER_INDEXES];
			indexes[0] = m_index(water, xx, yy - 1);
			indexes[1] = m_index(water, xx + 1, yy);
			indexes[2] = m_index(water, xx, yy + 1);
			indexes[3] = m_index(water, xx - 1, yy);

			float differences[WATER_INDEXES];
			float proportions[WATER_INDEXES];
			float maxdiff = 0;
			float count = 0;
			// compute the differences and also store the max difference
			// count the number of lower neighbors there are with weights
			for( int ii = 0; ii < WATER_INDEXES; ii++){

				differences[ii] = current - (water->values[indexes[ii]] + height->values[indexes[ii]]);
				if(differences[ii] > maxdiff){
					maxdiff = differences[ii];
				}
				if(differences[ii] > 0){
					// ternary to save code
					proportions[ii] = (differences[ii]) ;
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
			float total = min( currwater, maxdiff/2);
			float left = currwater - total;
			float totalmoved = 0;


			// move that proportion to that tile.
			for( int ii = 0; ii < WATER_INDEXES; ii++){
				float tomove = 0;
				if( count != 0){
					tomove = total * (proportions[ii]/count);
				}
				// assign it to the pipe
				pipes[ii]->values[indexes[ii]] = tomove;
				totalmoved += tomove;
			}


			// remove the moved volume
			map_set(toReturn, xx, yy, left);

		}
	}

	// reduce part of map-reduce
	for( int yy = 0; yy < water->height; yy++){
		for( int xx = 0; xx < water->width; xx++){
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
//#endif
