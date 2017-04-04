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


#endif /* WEATHER_H_ */
