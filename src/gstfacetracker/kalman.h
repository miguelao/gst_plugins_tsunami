#ifndef LIB_UTILS_MATH_KALMAN_H
#define LIB_UTILS_MATH_KALMAN_H

#include "defines.h"


#define KALMAN_TYPE_COV 0
#define KALMAN_TYPE_INFO 1
#define KALMAN_TYPE_DIAG 2

typedef struct {
	float *mean; // mean or information vector
	float *cov;  // covariance of information matrix
	unsigned int nx;   // number of elements
	unsigned char type;  // 0: covariance filter, 1: information filter
} t_kalman_state;

t_kalman_state *ks_create_shared(unsigned int nx);
t_kalman_state *ks_create_header_shared(unsigned int nx);
unsigned int ks_destroy_shared(t_kalman_state *ks);
t_kalman_state *ks_deep_copy(t_kalman_state *ks);

// info <-> cov
void kalman_convert_info(t_kalman_state *ks);
void kalman_convert_cov(t_kalman_state *ks);
void kalman_convert_diag(t_kalman_state *ks);

// update phase
void kalman_update_info(t_kalman_state *s, t_kalman_state *o, float *x, float *hx, float *Ht);
void kalman_update_cov(t_kalman_state *s, t_kalman_state *o, float *hx, float *Ht);
void kalman_update_diag(t_kalman_state *s, t_kalman_state *o);

// kalman score
float kalman_score(t_kalman_state *s, t_kalman_state *o, float *hx, float *Ht);


// x: current/new state mean (1 x nx)
// P: current/new state covariance (nx x nx)
// z: observation mean (1 x nz)
// R: observation covariance (nz x nz)
// hx: current state transferred to observation domain (1 x nz)
// Ht: Jacobian of observation function transposed (nx x nz)

void kalman_update_old(float *x, float *P, float *z, float *R, float *hx, float *Ht, unsigned int nx, unsigned int nz);

#endif
