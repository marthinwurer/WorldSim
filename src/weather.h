/*
 * weather.h
 *
 *  Created on: Apr 3, 2017
 *      Author: benjamin
 */

#ifndef WEATHER_H_
#define WEATHER_H_

#include "map2d.h"


#define GAS_CONSTANT_DRY_AIR		287.1
#define SPECIFIC_HEAT_DRY_AIR		1004.0
#define GRAVITY						9.81
#define KAPPA						0.286

float base_temp(float latitude);
map2d * temp_map_from_heightmap(map2d* heightmap, float sealevel, float max);

float evaporate(map2d * water_map, map2d * ground_water, double * removed, map2d * evap_map, map2d * temperature);

void rainfall(map2d * input, map2d * rain_map, double evaporated);

void calc_air_velocities(map2d * pressure, map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, map2d * convergence);

void calc_new_pressure(map2d * pressure, map2d * convergence, float timestep);

void my_air_velocities(map2d * pressure, map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, map2d * convergence);

void advect_velocities(map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, float timestep);

void advect_tracer(map2d * ew_velocity, map2d * ns_velocity, map2d * tracer, float timestep);

void advect_momentum(map2d * ew_velocity, map2d * ns_velocity, map2d * tracer, float timestep);

void calc_real_height(map2d * heightmap, map2d * water,  map2d * real_height, float sealevel);

void geopotential(map2d * heightmap, map2d * pressure,  map2d * ew_velocity, map2d * ns_velocity, float timestep);

void temperature_pressure(map2d * temperature, map2d * pressure,  map2d * ew_velocity, map2d * ns_velocity, float timestep);

void set_initial_pressures( map2d * height, map2d * water, map2d * virtual_temperature, map2d * surface_pressure, float sea_level);

void aflux(
		map2d * u_corner, map2d * v_corner,
		map2d * u_edge, map2d * v_edge,
		map2d * pressure,
		map2d * pressure_tendency,
		map2d * convergence,
		float timestep,
		float dx,
		float dy);

void advectm(
		map2d * pressure_tendency,
		map2d * pressure,
		map2d * new_pressure,
		float dt,
		float dx,
		float dy);

void advectv(
		map2d * u_corner, map2d * v_corner, // velocity
		map2d * dut, map2d * dvt, // change in velocity over time
		map2d * u_edge, map2d * v_edge, // momentum
		map2d * pressure,
		float dt,
		float dx,
		float dy);

void pgf(
		map2d * u_edge, map2d * v_edge,
		map2d * pressure,
		map2d * temperature, // this is the potential temperature
		map2d * geopotential,
		map2d * spa,
		float dt,
		float dx,
		float dy);




#endif /* WEATHER_H_ */
