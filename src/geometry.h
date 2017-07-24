/*
 * geometry.h
 *
 *  Created on: Jul 10, 2017
 *      Author: benjamin
 */

#ifndef SRC_GEOMETRY_H_
#define SRC_GEOMETRY_H_

typedef struct geo_units_s{
	float deg;
	float rad;
	float meter;
} geo_units;

typedef struct geom_s{
	float radius;
	float circumfrence;
	float diameter;
	geo_units north;
	geo_units south;
	geo_units east;
	geo_units west;
	geo_units ns_dist;



};



#endif /* SRC_GEOMETRY_H_ */
