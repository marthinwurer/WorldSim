/*
 * weather.c
 *
 *  Created on: Apr 3, 2017
 *      Author: benjamin
 */

#include "weather.h"
#include "utilities.h"


#include <math.h>

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


