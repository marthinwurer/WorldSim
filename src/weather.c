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

const float RAIN_CONSTANT = 1.0/1024.0/365.0/24.0;

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

			float flux_ns = flux * ( new_ew->values[west] + new_ew->values[index]);

			// update the change in velocities
			change_ns->values[west] += flux_ns;
			change_ns->values[index]-= flux_ns;


			// north-south contribution
			//       FLUX=DT12*(PV(I,J,L)+PV(IP1,J,L)+PV(I,J+1,L)+PV(IP1,J+1,L))       2066.
			flux = timestep / 12 *
					( new_ns->values[index] + new_ns->values[west] + new_ns->values[north] + new_ns->values[nw]);

			flux_ew = flux * ( new_ns->values[north] + new_ns->values[index]);

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


	// do the rest
	for( int yy = 0; yy < pressure->height - 1; ++yy){
		for( int xx = 0; xx < pressure->width; ++xx){
			int index = m_index(pressure, xx, yy);
			int north = m_index(pressure, xx, yy - 1);
			int south = m_index(pressure, xx, yy + 1);
			int west = m_index(pressure, xx - 1, yy);
			int east = m_index(pressure, xx + 1, yy);

			new_ew->values[index] = old_ew->values[index] + pressure->values[index] - pressure->values[east];
			new_ns->values[index] = old_ns->values[index] + pressure->values[index] - pressure->values[south];
		}
	}

	// do polar ns and ew
	for( int xx = 0; xx < pressure->width; ++xx){
		int yy = height - 1;
		int index = m_index(pressure, xx, yy);
		int north = m_index(pressure, xx, yy - 1);
		int south = m_index(pressure, xx, yy + 1);
		int west = m_index(pressure, xx - 1, yy);
		int east = m_index(pressure, xx + 1, yy);

		new_ew->values[index] = ( old_ew->values[index] + (pressure->values[index] - pressure->values[east]));
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










































