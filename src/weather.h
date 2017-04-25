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

void calc_air_velocities(map2d * pressure, map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, map2d * convergence);

void calc_new_pressure(map2d * pressure, map2d * convergence, float timestep);

void my_air_velocities(map2d * pressure, map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, map2d * convergence);

#endif /* WEATHER_H_ */
