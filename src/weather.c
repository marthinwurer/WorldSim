/*
 * weather.c
 *
 *  Created on: Apr 3, 2017
 *      Author: benjamin
 */

#include "weather.h"
#include "utilities.h"


#include <math.h>
#include <stdlib.h>
#include <stdio.h>

const float RAIN_CONSTANT = 1.0/1024.0/365.0/24.0;
extern float gravity;

#define METER 0.0000565
#define MILLIMETER (METER / 1000.0)


float base_temp(float latitude){
	if(fabs(latitude) < .2){
		return 27 + 273.15;
	}
	float val = fabs(latitude) * -67.5 + 40.5 + 273.15; // that kelvin tho
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



void calc_air_velocities(map2d * pressure, map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, map2d * convergence){
	// i is x is U, j is y is V


	int height = pressure->height;
	int width = pressure ->width;


	for( int yy = 1; yy < pressure->height - 1; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);

//			PU(I,J,L)=.25*DYP(J)*SPA(I,J,L)*(P(I,J)+P(IP1,J))
			float spa = (old_ew->values[index] + old_ew->values[south]);
			new_ew->values[index] = .25 * ((spa) * (pressure->values[index] + pressure->values[west]));
			// PV(I,J,L)=.25*DXV(J)*(V(I,J,L)+V(IM1,J,L))*(P(I,J)+P(I,J-1))
			new_ns->values[index] = .25 * ((old_ns->values[index] + old_ns->values[west]) *
					(pressure->values[index] - pressure->values[north]));
		}
	}
	
	// compute ns at south pole
	for( int xx = 0; xx < pressure->width; ++xx){
		int yy = height - 1;
		int index = m_index(pressure, xx, yy);
		int north = m_index(pressure, xx, yy - 1);
		int south = m_index(pressure, xx, yy + 1);
		int west = m_index(pressure, xx - 1, yy);
		int east = m_index(pressure, xx + 1, yy);
		new_ns->values[index] = .25 * ((old_ns->values[index] + old_ns->values[west]) *
				(pressure->values[index] - pressure->values[north]));
	}

	// calculate ew and ns for polar line

	float v_ns_n = 0;
	float v_ns_s = 0;
	float v_ew_n = 0; // PUN
	float v_ew_s = 0; // PUS

	for( int xx = 0; xx < pressure->width; ++xx){
		int index_n = m_index(pressure, xx, 1);
		int index_s = m_index(pressure, xx, pressure->height - 1);

		v_ns_n += old_ns->values[index_n];
		v_ns_s += old_ns->values[index_s];
		v_ew_n += new_ew->values[index_n];
		v_ew_s += new_ew->values[index_s];



//		int west = m_index(pressure, xx - 1, 0);
//
//		new_ew->values[index] = .5 * ((old_ew->values[index] ) +
//				(pressure->values[index] + pressure->values[west]));
//		new_ns->values[index] = 0;
////				.5 * ((old_ns->values[index] + old_ns->values[west]) +
////				(pressure->values[index]));
	}
//    PUS=.25*DYP(2)*PUS*P(1,1)/FIM
	int southwest = m_index(pressure, 0, pressure->height - 1);
	v_ew_n = .25 * v_ew_n * pressure->values[0] / pressure->width;
	v_ew_s = .25 * v_ew_s * pressure->values[southwest] / pressure->width;
	v_ns_n /= width;
	v_ns_s /= width;

	// dummy array time
	float * dummy_n = calloc(width, sizeof(float));
	float * dummy_s = calloc(width, sizeof(float));
	float pbs = 0;
	float pbn = 0;
	for( int xx = 1; xx < pressure->width; ++xx){
		int index_n = m_index(pressure, xx, 1);
		int index_s = m_index(pressure, xx, pressure->height - 1);

		// I think i might have a different idea of north and south... no matter, as long as I'm consistent
//	      DUMMYS(I)=DUMMYS(I-1)+(PV(I,2,L)-PVS)
		dummy_s[xx] = dummy_s[xx -1] + new_ew->values[index_s] - v_ns_s;
		dummy_n[xx] = dummy_n[xx -1] + new_ew->values[index_n] - v_ns_n;
		pbs += dummy_s[xx];
		pbn += dummy_n[xx];
	}

	pbs /= width;
	pbn /= width;

	// do the final calculations
	for( int xx = 1; xx < pressure->width; ++xx){
		int index_n = m_index(pressure, xx, 0);
		int index_s = m_index(pressure, xx, pressure->height - 1);

//	      PU(I,1,L)=3.*(PBS-DUMMYS(I)+PUS) // south
		// I do not get these at all.
		new_ew->values[index_s] = 3.0*(pbs - dummy_s[xx] + v_ns_s);
		new_ew->values[index_n] = 3.0*(dummy_s[xx] - pbn + v_ns_n);
	}

	free( dummy_s );
	free( dummy_n );









	// do the convergence

	for( int yy = 1; yy < pressure->height - 1; ++yy){

		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);

			// CONV(I,J,L)=(PU(IM1,J,L)-PU(I,J,L)+PV(I,J,L)-PV(I,J+1,L))*DSIG(L)

			convergence->values[index] = (new_ew->values[west] - new_ew->values[index] +
					new_ns->values[index] - new_ns->values[south]);
		}
	}

	for( int xx = 0; xx < pressure->width; ++xx){
		int index_n = m_index(pressure, xx, 0);
		int index_s = m_index(pressure, xx, pressure->height - 1);

		convergence->values[index_n] = v_ns_n;
		convergence->values[index_s] = -v_ns_s;
	}

}

void calc_new_pressure(map2d * pressure, map2d * convergence, float timestep){

	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);

			//       PA(I,J)=P(I,J)+(DT1*PIT(I,J)/DXYP(J))
			pressure->values[index] += convergence->values[index] * timestep;


		}
	}
}

/*
 * basically advecv
 */
void move_air_velocities(
		map2d * pressure,
		map2d * old_ew,
		map2d * new_ew,
		map2d * old_ns,
		map2d * new_ns,
		map2d * convergence,
		float timestep){

	map2d * change_ns = new_map2d(pressure->width, pressure->height);
	map2d * change_ew = new_map2d(pressure->width, pressure->height);

	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int west = m_index(pressure, xx - 1, yy);
			int nw = m_index(pressure, xx - 1, yy - 1);


			// west-east contribution
			//       FLUX=DT12*(PU(IP1,J,L)+PU(IP1,J-1,L)+PU(I,J,L)+PU(I,J-1,L))
			float flux = timestep / 12 *
					( new_ew->values[west] + new_ew->values[nw] + new_ew->values[index] + new_ew->values[north]);
			// FLUX*(U(IP1,J,L)+U(I,J,L))
			float flux_ew = flux * ( new_ew->values[west] + new_ew->values[index]);

			// update the change in velocities
			change_ew->values[west] += flux_ew;
			change_ew->values[index]-= flux_ew;

			float flux_ns = flux * ( new_ns->values[west] + new_ns->values[index]);

			// update the change in velocities
			change_ns->values[west] += flux_ns;
			change_ns->values[index]-= flux_ns;


			// north-south contribution
			//       FLUX=DT12*(PV(I,J,L)+PV(IP1,J,L)+PV(I,J+1,L)+PV(IP1,J+1,L))       2066.
			flux = timestep / 12 *
					( new_ns->values[index] + new_ns->values[west] + new_ns->values[north] + new_ns->values[nw]);

			flux_ew = flux * ( new_ew->values[north] + new_ew->values[index]);

			// update the change in velocities
			change_ew->values[north] += flux_ew;
			change_ew->values[index]-= flux_ew;

			flux_ns = flux * ( new_ns->values[north] + new_ns->values[index]);

			// update the change in velocities
			change_ns->values[north] += flux_ns;
			change_ns->values[index]-= flux_ns;



		}
	}

}










void my_air_velocities(map2d * pressure, map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, map2d * convergence){

	// assume that ew is velocity towards the east and located to the east (positive goes east, negative goes west)
	// assume that ns goes south abd is located to the south

	int height = pressure->height;
	int width =  pressure->width;

	float friction = 0.9;


	// do velocity
	for( int yy = 0; yy < pressure->height - 1; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);

			float ew_val = old_ew->values[index];
			float ns_val = old_ns->values[index];

			// apply friction
			ew_val = signof(ew_val) * max( 0.0, fabs(ew_val) * friction);
			ns_val = signof(ns_val) * max( 0.0, fabs(ns_val) * friction);

			new_ew->values[index] = ew_val + pressure->values[index] - pressure->values[east];
			new_ns->values[index] = ns_val + pressure->values[index] - pressure->values[south];

		}
	}

	// do polar ns and ew
	for( int xx = 0; xx < pressure->width; ++xx){
		int yy = height - 1;
		int index = m_index(pressure, xx, yy);
		int east = m_index(pressure, xx + 1, yy);

		float ew_val = old_ew->values[index] + pressure->values[index] - pressure->values[east];

		// apply friction
		new_ew->values[index] = signof(ew_val) * max( 0.0, fabs(ew_val) - friction);

		new_ns->values[index] = 0.0;
	}


	// do convergence
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int west = m_index(pressure, xx - 1, yy);

			convergence->values[index] =
					- new_ew->values[index] - new_ns->values[index] +
					new_ew->values[west] + new_ns->values[north];
		}
	}


}

void advect_velocities(map2d * old_ew, map2d * new_ew, map2d * old_ns, map2d * new_ns, float timestep){

	map2d * change_ns = new_map2d(old_ew->width, old_ew->height);
	map2d * change_ew = new_map2d(old_ew->width, old_ew->height);

	for( int yy = 0; yy < old_ew->height; ++yy){
		for( int xx = 0; xx < old_ew->width; ++xx){
			int index = m_index(old_ew, xx, yy);
			int north = m_index(old_ew, xx, yy - 1);
			int south = m_index(old_ew, xx, yy + 1);
			int west = m_index(old_ew, xx - 1, yy);
			int southwest = m_index(old_ew, xx - 1, yy + 1);

			// do east-west advection first
			float ew_val = .5 * (old_ew->values[west] + old_ew->values[index]);
			float ns_val = .5 * (old_ew->values[west] + old_ew->values[southwest]);

			ew_val = (ew_val + .5 * (new_ew->values[west] + new_ew->values[index])) * timestep;

			change_ew->values[index] -= ew_val;
			change_ew->values[west] += ew_val;

//			ns_val = (ns_val + .5 * (old_ns->values[west] + old_ns->values[index])) * timestep;
//
//			change_ns->values[index] += ns_val;
//			change_ns->values[west] -= ns_val;


			// now do north-south advection
//			ew_val = .5 * (old_ns->values[west] + old_ns->values[index]);
//			ns_val = .5 * (old_ns->values[north] + old_ns->values[index]);
//
//			ns_val = (ns_val + ns_val) * timestep;
//
//			change_ns->values[index] += ns_val;
//			change_ns->values[west] -= ns_val;

//			ew_val = (ew_val + .5 * (old_ew->values[west] + old_ew->values[southwest])) * timestep;
//
//			change_ew->values[southwest] += ew_val;
//			change_ew->values[west] -= ew_val;

		}
	}

	// make the changes!
	for( int yy = 0; yy < old_ew->height; ++yy){
		for( int xx = 0; xx < old_ew->width; ++xx){
			int index = m_index(old_ew, xx, yy);

			new_ew->values[index] += change_ew->values[index];
			new_ns->values[index] += change_ns->values[index];

		}
	}

	map2d_delete(change_ew);
	map2d_delete(change_ns);


}


void advect_tracer(map2d * ew_velocity, map2d * ns_velocity, map2d * tracer, float timestep){

	map2d * change = new_map2d(ew_velocity->width, ew_velocity->height);

	for( int yy = 0; yy < ew_velocity->height - 1; ++yy){
		for( int xx = 0; xx < ew_velocity->width; ++xx){
			int index = m_index(ew_velocity, xx, yy);
			int south = m_index(ew_velocity, xx, yy + 1);
			int east = m_index(ew_velocity, xx + 1, yy);

			// do east-west advection first
			float ew_val =  (ew_velocity->values[index]);// + ew_velocity->values[index]);

			ew_val = (ew_val * .5 * (tracer->values[east] + tracer->values[index])) * timestep;

			 // the maximum change can be the amount of tracer that exists in the tile /4
			ew_val = max(-(tracer->values[east]/4), ew_val);
			// check the other tile too
			ew_val = min((tracer->values[index]/4), ew_val);


			change->values[east] += ew_val;
			change->values[index] -= ew_val;

			float ns_val =  (ns_velocity->values[index]);// + ew_velocity->values[index]);

			ns_val = (ns_val * .5 * (tracer->values[index] + tracer->values[south])) * timestep;

			 // the maximum change can be the amount of tracer that exists in the tile /4
			ns_val = max(-(tracer->values[south]/4), ns_val);
			// check the other tile too
			ns_val = min((tracer->values[index]/4), ns_val);


			change->values[south] += ns_val;
			change->values[index] -= ns_val;

		}

		//      QT(I,J,L)=QT(I,J,L)+(FLUXQ(IM1)-FLUXQ(I))
	}

	// do the poles
	for( int xx = 0; xx < ew_velocity->width; ++xx){

		int yy = ew_velocity->height - 1;

		int index = m_index(ew_velocity, xx, yy);
		int south = m_index(ew_velocity, xx, yy + 1);
		int east = m_index(ew_velocity, xx + 1, yy);

		// do east-west advection first
		float ew_val =  (ew_velocity->values[index]);// + ew_velocity->values[index]);

		ew_val = (ew_val * .5 * (tracer->values[east] + tracer->values[index])) * timestep;

		 // the maximum change can be the amount of tracer that exists in the tile /4
		ew_val = max(-(tracer->values[east]/4), ew_val);
		// check the other tile too
		ew_val = min((tracer->values[index]/4), ew_val);


		change->values[east] += ew_val;
		change->values[index] -= ew_val;

		// don't do north-south

	}



	// make the changes!
	for( int yy = 0; yy < ew_velocity->height; ++yy){
		for( int xx = 0; xx < ew_velocity->width; ++xx){
			int index = m_index(ew_velocity, xx, yy);

			tracer->values[index] += change->values[index];

		}
	}

	map2d_delete(change);
}



void advect_momentum(map2d * ew_velocity, map2d * ns_velocity, map2d * tracer, float timestep){

	map2d * change = new_map2d(ew_velocity->width, ew_velocity->height);

	for( int yy = 0; yy < ew_velocity->height - 1; ++yy){
		for( int xx = 0; xx < ew_velocity->width; ++xx){
			int index = m_index(ew_velocity, xx, yy);
			int north = m_index(ew_velocity, xx, yy - 1);
			int south = m_index(ew_velocity, xx, yy + 1);
			int west = m_index(ew_velocity, xx - 1, yy);
			int east = m_index(ew_velocity, xx + 1, yy);
			int southwest = m_index(ew_velocity, xx - 1, yy + 1);

			// do east-west advection first
			float ew_val =  .5 * (ew_velocity->values[index] + ew_velocity->values[east]);

			ew_val = (ew_val + .5 * (tracer->values[east] +	tracer->values[index])) * timestep * .5;


			change->values[index] += ew_val;
			change->values[east] -= ew_val;



			float ns_val = (ns_velocity->values[index]);

			ns_val = (ns_val + .5 * (tracer->values[south] + tracer->values[index])) * timestep  * .5;

			change->values[index] += ns_val;
			change->values[south] -= ns_val;

		}

		//      QT(I,J,L)=QT(I,J,L)+(FLUXQ(IM1)-FLUXQ(I))
	}

	// poles
	for( int xx = 0; xx < ew_velocity->width; ++xx){

		int yy = ew_velocity->height - 1;

		int index = m_index(ew_velocity, xx, yy);
		int north = m_index(ew_velocity, xx, yy - 1);
		int south = m_index(ew_velocity, xx, yy + 1);
		int west = m_index(ew_velocity, xx - 1, yy);
		int east = m_index(ew_velocity, xx + 1, yy);
		int southwest = m_index(ew_velocity, xx - 1, yy + 1);

		// do east-west advection first
		float ew_val =  .5 * (ew_velocity->values[index] + ew_velocity->values[east]);

		ew_val = (ew_val + .5 * (tracer->values[east] +	tracer->values[index])) * timestep * .5;


		change->values[index] += ew_val;
		change->values[east] -= ew_val;

	}




	// make the changes!
	for( int yy = 0; yy < ew_velocity->height; ++yy){
		for( int xx = 0; xx < ew_velocity->width; ++xx){
			int index = m_index(ew_velocity, xx, yy);

			tracer->values[index] += change->values[index];

		}
	}

	map2d_delete(change);
}


void calc_real_height(map2d * heightmap, map2d * water,  map2d * real_height, float sealevel){
	for( int yy = 0; yy < heightmap->height; ++yy){
		for( int xx = 0; xx < heightmap->width; ++xx){
			int index = m_index(heightmap, xx, yy);

			real_height->values[index] = (heightmap->values[index] + water->values[index] - sealevel);
		}
	}
}


void geopotential(map2d * heightmap, map2d * pressure,  map2d * ew_velocity, map2d * ns_velocity, float timestep){

	for( int yy = 0; yy < heightmap->height - 1; ++yy){
		for( int xx = 0; xx < heightmap->width; ++xx){
			int index = m_index(heightmap, xx, yy);
			int south = m_index(heightmap, xx, yy + 1);
			int east = m_index(heightmap, xx + 1, yy);

			float ew_val =  .5 * (pressure->values[index] + pressure->values[east]);

			ew_val = (ew_val * .5 * (heightmap->values[index] - heightmap->values[east])) * timestep * .5;


			ew_velocity->values[index] += ew_val;

			float ns_val =  .5 * (pressure->values[index] + pressure->values[south]);

			ns_val = (ns_val * .5 * (heightmap->values[index] -	heightmap->values[south])) * timestep * .5;


			ns_velocity->values[index] += ns_val;



		}
	}

	// do pole transition

	for( int xx = 0; xx < heightmap->width; ++xx){

		int yy = heightmap->height - 1;

		int index = m_index(heightmap, xx, yy);
		int south = m_index(heightmap, xx, yy + 1);
		int east = m_index(heightmap, xx + 1, yy);

		float ew_val =  .5 * (pressure->values[index] + pressure->values[east]);

		ew_val = (ew_val * .5 * (heightmap->values[index] - heightmap->values[east])) * timestep * .5;


		ew_velocity->values[index] += ew_val;

		// do nothing for north-south
//		ns_velocity->values[index] = 0;
	}


}



void temperature_pressure(map2d * temperature, map2d * pressure,  map2d * ew_velocity, map2d * ns_velocity, float timestep){

	// calculate the change in pressure
	//       SPA(I,J,L)=SIG(L)*SP*RGAS*T(I,J,L)*PKDN/PDN

	map2d * change = new_map2d(temperature->width, temperature->height);

	for( int yy = 0; yy < temperature->height - 1; ++yy){
		for( int xx = 0; xx < temperature->width; ++xx){
			int index = m_index(temperature, xx, yy);

			change->values[index] = pressure->values[index] * GAS_CONSTANT_DRY_AIR * temperature->values[index]
															* pow(pressure->values[index], .286);
		}
	}

	// calculate the change in velocity due to this.
	// (SPA(I,J,L)+SPA(I,J-1,L))*(P(I,J)-P(I,J-1)))*DXV(J)

	for( int yy = 0; yy < temperature->height - 1; ++yy){
		for( int xx = 0; xx < temperature->width; ++xx){
			int index = m_index(temperature, xx, yy);
			int south = m_index(temperature, xx, yy + 1);
			int east = m_index(temperature, xx + 1, yy);

			float ew_val =  .5 * (change->values[index] + change->values[east]);

			ew_val = (ew_val + .5 * (pressure->values[east] + pressure->values[index])) * timestep * .5;


			ew_velocity->values[index] += ew_val;

			float ns_val =  .5 * (change->values[index] + change->values[south]);

			ns_val = (ns_val + .5 * (pressure->values[south] + pressure->values[index])) * timestep * .5;


			ns_velocity->values[index] += ns_val;



		}
	}

//	for( int xx = 0; xx < temperature->width; ++xx){
//
//		int yy = temperature->height - 1;
//
//		int index = m_index(temperature, xx, yy);
//		int south = m_index(temperature, xx, yy + 1);
//		int east = m_index(temperature, xx + 1, yy);
//
//		float ew_val =  .5 * (change->values[index] + change->values[east]);
//
//		ew_val = (ew_val + .5 * (pressure->values[index] - pressure->values[east])) * timestep * .5;
//
//
//		ew_velocity->values[index] += ew_val;
//	}


	map2d_delete(change);


}


/**
 * The goal of this function is to set the surface pressure so that the sea level pressure is 1013.25 mbar.
 */
void set_initial_pressures( map2d * height, map2d * water, map2d * virtual_temperature, map2d * surface_pressure, float sea_level){

	for( int yy = 0; yy < height->height; ++yy){
		for( int xx = 0; xx < height->width; ++xx){
			int index = m_index(height, xx, yy);

			surface_pressure->values[index] = 1013.25 / exp(gravity * (height->values[index] + water->values[index] - sea_level) / ( GAS_CONSTANT_DRY_AIR * virtual_temperature->values[index]));
		}
	}
}



/**
 * Do my own weather according to velocity computed at southeast of the cell's center.
 * My own scheme:
 * A B
 *  +
 * C D
 *
 * U = horizontal ( eastward )
 * V = vertical ( northward )
 *
 * the values of U and V are located at the southeast corner of the map tile.
 *
 * dU = ((A + C) - (B + D)) * .5
 * dV = ((C + D) - (A + B)) * .5
 *
 * or something like that
 *
 */
// OR JUST PORT GCM II
void aflux(
		map2d * u_corner, map2d * v_corner,
		map2d * u_edge, map2d * v_edge,
		map2d * pressure,
		map2d * pressure_tendency,
		map2d * convergence,
		float timestep,
		float dx,
		float dy){
//	printf("%f, %f, %f\n", timestep, dx, dy);
//	dispDS(u_corner);

	// compute the edge flux from the corners
	// lines 1826-1841
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);

			// TODO: no more toroids
			u_edge->values[index] = 0.25f *//dx *
					(u_corner->values[north] + u_corner->values[index]) *
					(pressure->values[index] + pressure->values[east]);
			v_edge->values[index] = 0.25f * //dy *
					(v_corner->values[west] + v_corner->values[index]) *
					(pressure->values[index] + pressure->values[south]);

//			printf("%f, %f, %f\n", u_edge->values[index], v_edge->values[index], 0.0);

		}
	}
	check_nan(u_edge, __FILE__, __LINE__);
	check_nan(v_edge, __FILE__, __LINE__);

	// compute the convergence (and pressure tendency)
	// 1873-1885
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);
			int southwest = m_index(pressure, xx - 1, yy + 1);

			convergence->values[index] =
					(u_edge->values[west] - u_edge->values[index] +
					v_edge->values[north] - v_edge->values[index]) ; // * DSIG

			// line 1880 multiplies it by the difference in sigma, which does ...
			//
			// something.

			// also apply the convergence to the pressure tendency.
			// lines 1886-1897
			pressure_tendency->values[index] = convergence->values[index];
		}
	}
	check_nan(convergence, __FILE__, __LINE__);
	check_nan(pressure_tendency, __FILE__, __LINE__);




	// not doing sigma dot yet




}



void advectm(
		map2d * pressure_tendency,
		map2d * pressure,
		map2d * new_pressure,
		float dt,
		float dx,
		float dy){

	// I think this is actually right.
	// need to double check when I do geometry
	float dxyp = dx * dy;

	// lines 1948-1950
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);

			// there's a special version of dx/dy, dxyp being used. need to take a look
			float change = (dt * pressure_tendency->values[index] / dxyp);
			new_pressure->values[index] = pressure->values[index]
					+ change
					;

		}
	}
}



// I want this to convert everything back to corner velocity and do the
// friction and coriolis force too. Maybe do aflux -> advec[mtq] -> pgf -> advecv?
void advectv(
		map2d * u_corner, map2d * v_corner, // velocity
		map2d * dut, map2d * dvt, // change in velocity over time
		map2d * u_edge, map2d * v_edge, // momentum
		map2d * pressure,
		float dt,
		float dx,
		float dy){


	// starts with some scaling, don't think I need this yet


	// zero the changes  dut and dvt
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			dut->values[index] = 0;
			dvt->values[index] = 0;
		}
	}


	// advect momentum.
	// 2016-2057
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);
			int southeast = m_index(pressure, xx + 1, yy + 1);

			// west-east mass flux
			float flux = dt/ 12 *
					(u_edge->values[index] + u_edge->values[east] +
							u_edge->values[south] + u_edge->values[southeast]);
			float fluxu = flux * (u_corner->values[index] + u_corner->values[east]);
			float fluxv = flux * (v_corner->values[index] + v_corner->values[east]);
			dut->values[index] -= fluxu;
			dut->values[east] += fluxu;
			dvt->values[index] -= fluxv;
			dvt->values[east] += fluxv;

		}
	}


	// turn the momentum back into the velocity
	// my own code
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);
			int southeast = m_index(pressure, xx + 1, yy + 1);

			u_corner->values[index] = (
					((((u_edge->values[index] + u_edge->values[south]) / 2)
//							+ dut->values[index]
										  ) /
							((pressure->values[index] + pressure->values[east] +
									pressure->values[south] + pressure->values[southeast]) / 4))
					);
//			if(isnan(u_corner->values[index])){
//				printf("NaN (%d, %d): %f %f %f %f %f %f\n", xx, yy,
//						u_edge->values[index], pressure->values[index],
//						pressure->values[east], u_edge->values[south], pressure->values[south], pressure->values[southeast]);
//			}
			v_corner->values[index] =
					(v_edge->values[index]/(pressure->values[index] + pressure->values[south]) +
					v_edge->values[east]/(pressure->values[east] + pressure->values[southeast]));
		}
	}

}


void pgf(
		map2d * u_edge, map2d * v_edge,
		map2d * pressure,
		map2d * temperature, // this is the potential temperature
		map2d * geopotential,
		map2d * spa,
		float dt,
		float dx,
		float dy){

	// TODO: for 3d real model, need to adjust geopotential for first layer
	// see lines 489 of DB11pdC9.f

	// calculate the SPA, whatever that means
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);

			// TODO: will need to be changed with layers for pi and pdn
			spa->values[index] = pow(pressure->values[index], KAPPA) *
					temperature->values[index] * GAS_CONSTANT_DRY_AIR;

		}
	}



	// now do the pressure gradient and geopotential gradient
	// lines 2188-2214
	for( int yy = 0; yy < pressure->height; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);

			//TODO
			// just add it to the edge momentums for the moment.
			// eastward
			float flux;
			flux = dt / dx * .25 * (
					((pressure->values[index] + pressure->values[east]) *
							(geopotential->values[index] - geopotential->values[east])) +
					((spa->values[index] + spa->values[east]) *
							(pressure->values[index] - pressure->values[east])));
			u_edge->values[index] += flux;

			flux = dt / dy * .25 * (
					((pressure->values[index] + pressure->values[south]) *
							(geopotential->values[index] - geopotential->values[south])) +
					((spa->values[index] + spa->values[south]) *
							(pressure->values[index] - pressure->values[south])));
			v_edge->values[index] += flux;
		}
	}

}





















