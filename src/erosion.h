/*
 * erosion.h
 *
 *  Created on: Sep 16, 2016
 *      Author: benjamin
 */

#ifndef SRC_EROSION_H_
#define SRC_EROSION_H_

map2d * thermal_erosion(map2d * input, map2d * water);

map2d ** water_pipes(int x_dim, int y_dim);

map2d * water_movement(map2d * water, map2d * height, map2d ** momentums, map2d ** velocities);

map2d * hydraulic_erosion(map2d * input, map2d ** velocities);


#endif /* SRC_EROSION_H_ */
