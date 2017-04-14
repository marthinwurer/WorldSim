/*
 * weather.h
 *
 *  Created on: Apr 3, 2017
 *      Author: benjamin
 */

#ifndef WEATHER_H_
#define WEATHER_H_

#include "map2d.h"

float base_temp(float latitude);
map2d * temp_map_from_heightmap(map2d* heightmap, float sealevel, float max);

float evaporate(map2d * water_map, map2d * ground_water, double * removed, map2d * evap_map, map2d * temperature);

void rainfall(map2d * input, map2d * rain_map, double evaporated);



#endif /* WEATHER_H_ */
