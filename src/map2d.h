/*
 * map2d.h
 *
 *  Created on: Sep 7, 2016
 *      Author: benjamin
 */

#ifndef SRC_MAP2D_H_
#define SRC_MAP2D_H_


typedef struct map2d_struct{
	int height;
	int width;
	float * restrict values;

} map2d;

map2d * new_map2d(int width, int height);

void map2d_delete( map2d * toDelete );

float value( map2d * ds, int x, int y);

int m_index( map2d * ds, int x, int y);

void map_set( map2d * ds, int x, int y, float val);

void dispDS( map2d * ds );

double map2d_total( map2d * ds );

double map2d_min( map2d * ds );

void normalize(map2d * ds);

map2d ** make_array(int size, int x, int y);

void swapmap(map2d ** a, map2d ** b);
void swapmap2(map2d *** a, map2d *** b);


#endif /* SRC_MAP2D_H_ */
