/*
 * weather.c
 *
 *  Created on: Apr 3, 2017
 *      Author: benjamin
 */

#include "weather.h"
#include "utilities.h"


#include <math.h>

const float RAIN_CONSTANT = 1.0/1024.0/365.0/24.0;

#define METER 0.0000565
#define MILLIMETER (METER / 1000.0)


float base_temp(float latitude){
	if(fabs(latitude) < .2){
		return 27;
	}
	float val = fabs(latitude) * -67.5 + 40.5;
	return val;
}


map2d * temp_map_from_heightmap(map2d* heightmap, float sealevel, float max_height){
	map2d * temp_map = new_map2d(heightmap->width, heightmap->height);

	/*
	 * wet atmospheric lapse rate is ~5.0C/ 1KM. dry is 9.8/KM
	 *
	 * That means that if my 1.0 is the height of mt everest, 8,848m IRL, and 0.5 is sea level, then
	 * -5C * 8848 /( difference between sea level and max  - in this case 0.5)
	 * is the equation to find the lapse rate in pure my units.
	 * Which is -88.48.
	 *
	 */
	for( int yy = 0; yy < temp_map->height; yy++){
		for( int xx = 0; xx < temp_map->width; xx++){
			int index = m_index(temp_map, xx, yy);

			float height = heightmap->values[index];
			float adjusted = max(height - sealevel, 0.0);
			float lapse = adjusted * -88.48;

			float latitude = temp_map->height / 2;
			latitude = (latitude - yy) / latitude;

			float base = base_temp(latitude);

			temp_map->values[index] = base + lapse;
		}
	}

	return temp_map;
}

/**
 * Do basic evaporation: remove half of the water from the tile.
 *
 * MODIFIES THE INPUT
 */
#define FLAT_EVAP
float evaporate(map2d * water_map, map2d * ground_water, double * removed, map2d * evap_map, map2d * temperature){
	float maxval = 0.0;
	double totalRemoved = 0;
	for( int yy = 0; yy < water_map->height; yy++){
		for( int xx = 0; xx < water_map->width; xx++){
			int ii = m_index(water_map, xx, yy);

#ifndef FLAT_EVAP
			float amount = RAIN_CONSTANT * evap_map->values[ii];
#else
			float amount = RAIN_CONSTANT;

			/*
			 * use the Blaney-Criddle formula:
			 * This formula, based on another empirical model, requires only
			 * mean daily temperatures T (C) over each month. Then:
			 * 		 PE = p.(0.46*T + 8) mm/day
			 * where p is the mean daily percentage (for the month) of total
			 * annual daytime hours.
			 */

			// use .5 as % daylight hours
			// make time step 1 hour, so divide by 24.
			amount = 0.5 * (0.46 * temperature->values[ii] + 8.0) / 24.0 * MILLIMETER; // gets us mm.
#endif

			float tomove = min(amount, water_map->values[ii]);

			water_map->values[ii] -= tomove;

			if( water_map->values[ii] > maxval){
				maxval = water_map->values[ii];
			}
			totalRemoved += tomove;
		}
	}
	*removed = totalRemoved;
	return maxval;
}

/**
 * Do basic rainfall: add a portion of the rain map to the .
 *
 * MODIFIES THE INPUT
 */
void rainfall(map2d *restrict input, map2d *restrict rain_map, double evaporated){

	double rain_per = evaporated / (double)( input->height * input->width);
	for( int yy = 0; yy < input->height; yy++){
		for( int xx = 0; xx < input->width; xx++){
			int ii = m_index(input, xx, yy);
			input->values[ii] += rain_per;
		}
	}
}
