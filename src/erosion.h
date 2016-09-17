/*
 * erosion.h
 *
 *  Created on: Sep 16, 2016
 *      Author: benjamin
 */

#ifndef SRC_EROSION_H_
#define SRC_EROSION_H_

map2d * thermal_erosion(map2d * input);

float evaporate(map2d * input);

void rainfall(map2d * input, map2d * rain_map);

map2d * water_movement(map2d * water, map2d * height);

#endif /* SRC_EROSION_H_ */
