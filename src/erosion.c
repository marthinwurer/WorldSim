/**
    This file contains the erosion algorithms.

*/

#define _BSD_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include "xoroshiro128plus.h"
#include "DiamondSquare.h"
#include "map2d.h"
#include "threadpool.h"
#include "utilities.h"

const float diagdist = 1.41421356237;
const float diagval = 1.0/1.41421356237;

const float mindist = 0.017;

const float HYDRAULIC_EROSION_CONSTANT = 0.01;
const float MOMEMTUM_CONSTANT = 0.95;

extern const int NUM_THREADS;
extern threadpool_t * thread_pool;
extern float timestep;
extern float squarelen;
extern float gravity;

const float turb_coef = 0.005f; // turbulent flat plate parallel to the flow (from wikipedia)
const float kinematic_viscostity_water_20c = 1.004f; // m^2/s. kinematic viscosity of water at 20C.

// a struct to be passed into the threadpool
struct poolpram_s{
	map2d *restrict input;
	map2d *restrict water;
	map2d **restrict pipes;
	map2d *restrict toReturn;
	int offset;
	int increment;
	int done;
};

/**
 * A function to calculate the indexes for a cell's neighbors.
 * Assumes that the arrays have 8 indexes.
 * 0 1 2
 * 7 x 3
 * 6 5 4
 *
 * modifies the indexes array
 */
void calculate_indexes8(int * indexes, int xx, int yy, map2d * index_into ){
	indexes[0] = m_index(index_into, xx - 1, yy - 1);
	indexes[1] = m_index(index_into, xx, yy - 1);
	indexes[2] = m_index(index_into, xx + 1, yy - 1);
	indexes[3] = m_index(index_into, xx + 1, yy);
	indexes[4] = m_index(index_into, xx + 1, yy + 1);
	indexes[5] = m_index(index_into, xx, yy + 1);
	indexes[6] = m_index(index_into, xx - 1, yy + 1);
	indexes[7] = m_index(index_into, xx - 1, yy);
}

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
			int currindex = m_index(param->input, xx, yy);
			float current = value(param->input, xx, yy);
			int indexes[8];
			calculate_indexes8(indexes, xx, yy, param->water);

//			indexes[0] = m_index(param->input, xx - 1, yy - 1);
//			indexes[1] = m_index(param->input, xx, yy - 1);
//			indexes[2] = m_index(param->input, xx + 1, yy - 1);
//			indexes[3] = m_index(param->input, xx + 1, yy);
//			indexes[4] = m_index(param->input, xx + 1, yy + 1);
//			indexes[5] = m_index(param->input, xx, yy + 1);
//			indexes[6] = m_index(param->input, xx - 1, yy + 1);
//			indexes[7] = m_index(param->input, xx - 1, yy);

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

			/* add that much water - there has to have been that much water that moved,
			 * so there will be that much available to take.
			 */
//			param ->water->values[currindex] += totalmoved;

		}
	}


//	// reduce part of map-reduce
//	for( int yy = param->offset; yy < param->input->height; yy += param->increment){
//		for( int xx = 0; xx < param->input->width; xx++){
//			// get the current value
//			float current = value(param->toReturn, xx, yy);
//			// add the pipe values to it
//			for( int ii = 0; ii < 8; ii ++){
//				current += value(param->pipes[ii], xx, yy);
//			}
//			// assign it back
//			map_set(param->toReturn, xx, yy, current);
//		}
//	}

	param->done = 1;
}

/**
 * do the basic thermal erosion algorithm as described in Balazs Jako's paper.
 */
map2d * thermal_erosion(map2d * restrict input, map2d * restrict water){
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
		pool_data[ii]->water = water;
		pool_data[ii]->pipes = pipes;
		pool_data[ii]->toReturn = toReturn;
		pool_data[ii]->offset = ii;
		pool_data[ii]->increment = NUM_THREADS;

		threadpool_add(thread_pool, erode_thread, (void *)(pool_data[ii]), 0);
	}

	// wait for all of the threads to finish
	for( int ii = 0; ii < NUM_THREADS; ii++){
		while(pool_data[ii]->done == 0){
			usleep((unsigned int)1);
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
			// get the current index
			int currindex = m_index(input, xx, yy);
			// add the pipe values to it
			float total = 0.0;
			for( int ii = 0; ii < 8; ii ++){
				total += pipes[ii]->values[currindex];
			}
			// assign it back
			toReturn->values[currindex] += total;
			// subtract that much water
//			water->values[currindex] -= total;
		}
	}

	//free everything
	for( int ii = 0; ii < 8; ii++){
		map2d_delete(pipes[ii]);
	}
	free( pipes);

	return toReturn;
}


//#ifdef nope
#define WATER_INDEXES 8

map2d ** water_pipes(int x_dim, int y_dim){

	// make an array of eight pointers to each of the pipes.
	map2d ** pipes = malloc( WATER_INDEXES * sizeof( map2d *));

	// make all the pipes
	for( int ii = 0; ii < WATER_INDEXES; ii++)
	{
		pipes[ii] = new_map2d(x_dim, y_dim);
	}

	return pipes;
}


struct water_poolpram_s{
	map2d *restrict water;
	map2d *restrict height;
	map2d *restrict*restrict pipes;
	map2d *restrict*restrict velocities;
	map2d *restrict*restrict momentums;
	map2d *restrict toReturn;
	int offset;
	int increment;
	int done;
};

void water_thread( void * arg){
//	printf("%p \n", arg);
	struct water_poolpram_s * param = (struct water_poolpram_s *)arg;


	// loop through all the positions on the board
	for( int yy = param->offset; yy < param->water->height; yy += param->increment){
		for( int xx = 0; xx < param->water->width; xx++){
			// get the values of all the neighboring cells
			// 0 1 2
			// 7 x 3
			// 6 5 4
			float currwater = value(param->water, xx, yy);
			float currheight = value(param->height, xx, yy);
			float current = currwater + currheight;
			int currindex = m_index(param->water, xx, yy);

			int indexes[WATER_INDEXES];
//			indexes[0] = m_index(param->water, xx, yy - 1);
//			indexes[1] = m_index(param->water, xx + 1, yy);
//			indexes[2] = m_index(param->water, xx, yy + 1);
//			indexes[3] = m_index(param->water, xx - 1, yy);
			calculate_indexes8(indexes, xx, yy, param->water);

			// make arrays so that we can do this in a loop.
			float differences[WATER_INDEXES];
			float m_dir[WATER_INDEXES];  // the momentum in that direction
			float proportions[WATER_INDEXES];
			float maxdiff = 0; // the maximum difference with a neighboring tile
			float count = 0;

			float totalmomentum = 0;
			// compute the differences and also store the max difference
			// count the number of lower neighbors there are with weights
			for( int ii = 0; ii < WATER_INDEXES; ii++){

				// compute the momentum that will be applied to the current tile
				m_dir[ii] = param->momentums[ii]->values[indexes[ii]] * MOMEMTUM_CONSTANT;

				// calculate the difference in that direction.
				differences[ii] = current - (param->water->values[indexes[ii]] + param->height->values[indexes[ii]]);
				if(differences[ii] > maxdiff){
					maxdiff = differences[ii];
				}

				proportions[ii] = max(0, differences[ii]) ;
				count += proportions[ii];

				// apply the acceleration to the velocity/momentum
				m_dir[ii] += differences[ii] * .1;
				m_dir[ii] = max( 0, m_dir[ii]); // minimum is zero
				totalmomentum += m_dir[ii];
			}

			// test for the case of none being moved (too low of angle)
			if(count == 0){
				maxdiff = 0;
			}

			// calculate the total to be moved.
			float left = currwater;
			float totalmoved = 0;

			// do the momentum movement
			if( left > 0 && totalmomentum > 0){
				float proportion = 1;
				if( left < totalmomentum){
					proportion = left / totalmomentum;
//					printf("lessleft ");
				}

				// calulate the amount moved
				float moved[WATER_INDEXES];
				for( int ii = 0; ii < WATER_INDEXES; ii++){
					moved[ii] = m_dir[ii] * proportion;//0;
				}
//				float coriolis_proportion = 4.0;
//				for( int ii = 0; ii < WATER_INDEXES; ii++){
//					float tomove = m_dir[ii] * proportion;
//					// do some hacky coriolis force stuff
//					moved[ii] += tomove * ((coriolis_proportion - 1.0)/coriolis_proportion);
//					moved[(ii + 1) % WATER_INDEXES] += tomove * (1.0/coriolis_proportion);
//				}
//

				// move that proportion to that tile.
				for( int ii = 0; ii < WATER_INDEXES; ii++){
					// calculate the amount moved
					float tomove = moved[ii];

					// compute the velocity at the base of the water column
					param->velocities[ii]->values[currindex] = tomove;
					// assign it to the pipe

					param->pipes[ii]->values[indexes[ii]] = tomove;
					totalmoved += tomove;
				}
				left -= totalmoved;

				if(left < 0){
					left = 0;
//					printf( "moved: %f\n", left - totalmoved);
				}
			}


			// remove the moved volume
			map_set(param->toReturn, xx, yy, left);

		}
	}

//	dispDS(pipes[0]);

//	// reduce part of map-reduce
//	for( int yy = param->offset; yy < param->water->height; yy += param->increment){
//		for( int xx = 0; xx < param->water->width; xx++){
//			// get the current value
//			float current = value(param->toReturn, xx, yy);
//			// add the pipe values to it
//			for( int ii = 0; ii < WATER_INDEXES; ii ++){
//				current += value(param->pipes[ii], xx, yy);
//			}
//			// assign it back
//			map_set(param->toReturn, xx, yy, current);
//		}
//	}



	param->done = 1;
}


/**
 * Move the water depending on the height of the surrounding terrain and the water there.
 *
 * Modifies the momentums pointer. clears all of the old maps
 */
map2d * water_movement(map2d * restrict water, map2d * restrict height, map2d ** momentums, map2d ** velocities){

	// make a new map to return.
	map2d * toReturn = new_map2d(water->width, water->height);

	// make an array of eight pointers to each of the pipes.
	map2d ** pipes = water_pipes(water->width, water->width);

	// do the water calculations

#ifdef POOL_THREAD

	// make an array of pool structs.
	struct water_poolpram_s ** pool_data = malloc( NUM_THREADS * sizeof( struct water_poolpram_s *));

	// make the structs and start the threads
	for( int ii = 0; ii < NUM_THREADS; ii++){
		pool_data[ii] = malloc( sizeof( struct water_poolpram_s));
		pool_data[ii]->done = 0;
		pool_data[ii]->water = water;
		pool_data[ii]->height = height;
		pool_data[ii]->pipes = pipes;
		pool_data[ii]->velocities = velocities;
		pool_data[ii]->momentums = momentums;
		pool_data[ii]->toReturn = toReturn;
		pool_data[ii]->offset = ii;
		pool_data[ii]->increment = NUM_THREADS;

		threadpool_add(thread_pool, water_thread, (void *)(pool_data[ii]), 0);
	}

	// wait for all of the threads to finish
	for( int ii = 0; ii < NUM_THREADS; ii++){
		while(pool_data[ii]->done == 0){
			usleep((unsigned int)1);
		}
		free(pool_data[ii]);
	}
	free(pool_data);
#else

	struct water_poolpram_s * pool_data  = malloc( sizeof( struct water_poolpram_s));

	pool_data->done = 0;
	pool_data->water = water;
	pool_data->height = height;
	pool_data->pipes = pipes;
	pool_data->momentums = momentums;
	pool_data->toReturn = toReturn;
	pool_data->offset = ii;
	pool_data->increment = NUM_THREADS;

	threadpool_add(thread_pool, water_thread, (void *)(pool_data), 0);

	while(pool_data->done == 0){
		usleep(1);
	}

	free(pool_data);

#endif

	// reduce part of map-reduce
	for( int yy = 0; yy < water->height; yy++){
		for( int xx = 0; xx < water->width; xx++){
			// get the current value
			float current = value(toReturn, xx, yy);
			// add the pipe values to it
			for( int ii = 0; ii < WATER_INDEXES; ii ++){
				current += value(pipes[ii], xx, yy);
			}
			// assign it back
			map_set(toReturn, xx, yy, current);
		}
	}

	//free everything
	for( int ii = 0; ii < WATER_INDEXES; ii++){
		map2d_delete(momentums[ii]);
		momentums[ii] = pipes[ii];
	}
	free( pipes);

	return toReturn;
}
//#endif








/*
 * Compute the hydraulic erosion
 */
map2d * hydraulic_erosion(map2d * heightmap, map2d ** velocities){

	// make a new map to return.
	map2d * toReturn = new_map2d(heightmap->width, heightmap->height);

	// make an array of eight pointers to each of the pipes.
	map2d ** pipes = malloc( 8 * sizeof( map2d *));

	// make all the pipes
	for( int ii = 0; ii < 8; ii++)
	{
//		check_nan(velocities[ii], __FILE__, __LINE__);
		pipes[ii] = new_map2d(heightmap->width, heightmap->height);
	}

	/* what I need to do :
	 * Find the amount of material moved to the next tile, and then push that much water back the other way.
	 */


	for( int yy = 0; yy < heightmap->height; yy ++){
		for( int xx = 0; xx < heightmap->width; xx++){
			// get the values of all the neighboring cells
			// 0 1 2
			// 7 x 3
			// 6 5 4
			float current = value(heightmap, xx, yy);
			int currindex = m_index(heightmap, xx, yy);
			int indexes[8];
			float values[8];
			calculate_indexes8(indexes, xx, yy, heightmap);

			// count the total amount being moved
			float total = 0;
			for( int ii = 0; ii < 8; ii++){
				float tomove = velocities[ii]->values[currindex];
//				if( currwater < 0.1){
				values[ii] = tomove  * HYDRAULIC_EROSION_CONSTANT;// min (tomove  * 0.5, RAIN_CONSTANT * 0.02); //* (0.5-currwater) * (0.5-currwater)
//				}
//				else{
//					values[ii] = 0;
//				}
				total += values[ii];
			}

			float proportion = min(1.0, current / total);
			float totalmoved = 0;

			// move that proportion to that tile.
			for( int ii = 0; ii < 8; ii++){
				float tomove = 0;
				tomove = total * proportion;
				// assign it to the pipe
				pipes[ii]->values[indexes[ii]] = tomove;
				totalmoved += tomove;
			}

			float left = current - totalmoved;


			// remove the moved volume
			map_set(toReturn, xx, yy, left);

			/* add that much water - there has to have been that much water that moved,
			 * so there will be that much available to take.
			 */
//			watermap->values[currindex] += totalmoved;


		}
	}


	// reduce part of map-reduce
	for( int yy = 0; yy < heightmap->height; yy++){
		for( int xx = 0; xx < heightmap->width; xx++){
			// get the current index
			int currindex = m_index(heightmap, xx, yy);
			// add the pipe values to it
			float total = 0.0;
			for( int ii = 0; ii < 8; ii ++){
				total += pipes[ii]->values[currindex];
			}
			// assign it back
			toReturn->values[currindex] += total;
			// subtract that much water
//			watermap->values[currindex] -= total;
		}
	}

	//free everything
	for( int ii = 0; ii < 8; ii++){
		map2d_delete(pipes[ii]);
	}
	free( pipes);

	return toReturn;

}

void calculate_indexes4(int * indexes, int xx, int yy, map2d * index_into ){
	indexes[0] = m_index(index_into, xx, yy - 1);
	indexes[1] = m_index(index_into, xx + 1, yy);
	indexes[2] = m_index(index_into, xx, yy + 1);
	indexes[3] = m_index(index_into, xx - 1, yy);
}


/**
 * This function does water movement as in the paper
 * "Interactive Terrain Modeling Using Hydraulic Erosion" by Stavo et al. 2008
 *
 * velocities are in m/s
 *
 * pipe xy indexes are for the water coming into the tile.
 *
 * This only works in 4 directions.
 */
void stavo_water_movement(map2d * heightmap, map2d * water, map2d * nextwater, map2d ** oldvelocities, map2d ** newvelocities, map2d ** pipes){

	// base directions for the loop in this function
	//  0
	// 3x1
	//  2

	// water is 1 g/cm^3
	float density = 1.0f;

	float area = squarelen * squarelen;



	for( int yy = 0; yy < heightmap->height; yy ++){
		for( int xx = 0; xx < heightmap->width; xx++){
			// get the current index
			int index = m_index(heightmap, xx, yy);

			// find the indexes of the neighbors
			int indexes[4];
			calculate_indexes4(&indexes, xx, yy, water);

			// find the current amounts of water and geopotential height
			float currwater = water->values[index];
			float volume = currwater * area;
			float currheight = heightmap->values[index];
			float geo = currwater + currheight; // the geopotential height

			float differences[4]; // the pressure difference between the current tile and the neighbor
			float velocity[4];
			float flow[4]; // the proportion of the water that is moving to that neighbor
			float totaltobemoved = 0; // the total amount of water to be moved

			// loop over each direction
			for(int ii = 0; ii < 4; ii++){
				// find the pressure difference
				float n_water = water->values[indexes[ii]];
				float n_height = heightmap->values[indexes[ii]];
				float n_geo = n_water + n_height;
				differences[ii] = gravity * (geo - n_geo); // formula in paper has the density here too, but the acceleration formula cancels it out

				// find the force of friction in the pipe.
				// use skin friction drag (https://en.wikipedia.org/wiki/Skin_friction_drag)
				// Assume pipe is half full, so hydraulic radius is 0.25m. Also assume square pipe.
				velocity[ii] = oldvelocities[ii]->values[index];
				float re = velocity[ii] * 0.25 / kinematic_viscostity_water_20c;

				// use the reynold's number to find the coefficient of friction
				// use the Prandtl approximation
				float coef = (float)(0.074 * pow( re, -0.2));

				// change the velocity
				velocity[ii] -= velocity[ii] * velocity[ii] * squarelen * squarelen * coef;

				// Make sure that the change due to friction does not make the velocity less than zero
				velocity[ii] = max( 0, velocity[ii]);


				// if the differences are zero, skip the acceleration calculations and have the flow be zero.
				if( differences[ii] > 0.0){
					// once friction has been applied, find the acceleration
					float acceleration = differences[ii] / squarelen;

					velocity[ii] += acceleration * timestep;

					// find the final flow.
					flow[ii] = velocity[ii] * squarelen * squarelen;
					totaltobemoved += flow[ii];
				}
				else{
					flow[ii] = 0.0;
				}
			}

			// now that we have figured out the flow in each direction, make sure that we aren't taking too much out and then write to the new velocities
			float proportion = 1.0f;
			if( totaltobemoved > volume){ // square it to find the total volume of water in this tile.
				proportion = volume / totaltobemoved;
			}
			// write to the pipes and to the velocities
			for(int ii = 0; ii < 4; ii++){
				// make sure to move it into the pipe for the correct tile.
				pipes[ii]->values[indexes[ii]] = flow[ii] * proportion;
				newvelocities[ii]->values[index] = velocity[ii] * proportion;
			}

			// write to the next water level
			nextwater->values[index] = min(volume, totaltobemoved) / area;

			// do the next tile
		}
	}

	// once all tiles are done, do the map-reduce
	for( int yy = 0; yy < heightmap->height; yy ++){
		for( int xx = 0; xx < heightmap->width; xx++){
			int index = m_index(heightmap, xx, yy);
			for(int ii = 0; ii < 4; ii++){
				nextwater->values[index] += pipes[ii]->values[index];
			}
		}
	}



}




