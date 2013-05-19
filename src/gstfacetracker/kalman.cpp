#include "kalman.h"
#include "mtxcore.h"
#include <string.h>
#include "stdlib.h"

t_kalman_state *ks_create_shared(unsigned int nx)
{
	t_kalman_state *ks = ks_create_header_shared(nx);
	ks->mean = (float*)malloc(nx*sizeof(float));
	ks->cov = (float*)malloc(nx*nx*sizeof(float));
	memset(ks->mean,0,nx*sizeof(float));
	mat_eye(ks->cov,1e20,nx);

	return ks;
}
t_kalman_state *ks_create_header_shared(unsigned int nx)
{
	t_kalman_state *ks = (t_kalman_state*)malloc(sizeof(t_kalman_state));
	ks->nx = nx;
	ks->type = KALMAN_TYPE_COV;

	return ks;
}
unsigned int ks_destroy_shared(t_kalman_state *ks)
{
	free(ks->mean);
	free(ks->cov);
	free(ks);

	return 0;
}

t_kalman_state *ks_deep_copy(t_kalman_state *ks) {

	t_kalman_state *cp = ks_create_shared(ks->nx);
	cp->type = ks->type;
	memcpy(cp->mean,ks->mean,cp->nx*sizeof(float));
	memcpy(cp->cov,ks->cov,cp->nx*cp->nx*sizeof(float));

	return cp;
}

void kalman_convert_diag(t_kalman_state *ks)
{
	if (ks->type==KALMAN_TYPE_DIAG) return;

	if (ks->type==KALMAN_TYPE_INFO) kalman_convert_cov(ks);

	unsigned int i;
	for (i=0; i<ks->nx; i++) ks->cov[i] = ks->cov[i*(ks->nx+1)];

	ks->type = KALMAN_TYPE_DIAG;
}

void kalman_convert_info(t_kalman_state *ks)
{
	if (ks->type==KALMAN_TYPE_INFO) return;

	float Y[ks->nx*ks->nx];
	float y[ks->nx*ks->nx];

	if (ks->type==KALMAN_TYPE_DIAG) {
		unsigned int i;
		memset(Y,0,ks->nx*ks->nx*sizeof(float));
		for (i=0;i<ks->nx;i++) {
			Y[i*(ks->nx+1)] = 1.0/ks->cov[i];
			y[i] = ks->mean[i]/ks->cov[i];
		}
	}

	if (ks->type==KALMAN_TYPE_COV) {
		mat_invert(Y,ks->cov,ks->nx);
		mat_multiply(y,Y,ks->mean,ks->nx,ks->nx,1);
	}

	memcpy(ks->cov,Y,ks->nx*ks->nx*sizeof(float));
	memcpy(ks->mean,y,ks->nx*sizeof(float));

	ks->type = KALMAN_TYPE_INFO;
}
void kalman_convert_cov(t_kalman_state *ks)
{
	if (ks->type==KALMAN_TYPE_COV) return;

	float P[ks->nx*ks->nx];
	float x[ks->nx*ks->nx];

	if (ks->type==KALMAN_TYPE_DIAG) {
		unsigned int i;
		memset(P,0,ks->nx*ks->nx*sizeof(float));
		for (i=0;i<ks->nx;i++) {
			P[i*(ks->nx+1)] = ks->cov[i];
			x[i] = ks->mean[i];
		}
	}
	if (ks->type==KALMAN_TYPE_INFO) {
		mat_invert(P,ks->cov,ks->nx);
		mat_multiply(x,P,ks->mean,ks->nx,ks->nx,1);
	}

	memcpy(ks->cov,P,ks->nx*ks->nx*sizeof(float));
	memcpy(ks->mean,x,ks->nx*sizeof(float));

	ks->type = KALMAN_TYPE_COV;
}

void kalman_update_info(t_kalman_state *s, t_kalman_state *o, float *x, float *hx, float *Ht)
{
	float HtRi[s->nx*o->nx];
	float I[s->nx*s->nx];
	float i[s->nx];
	float tmp[s->nx];
	unsigned char ts = s->type;
	unsigned char to = o->type;

	kalman_convert_info(s);
	kalman_convert_info(o);

	// Y' = Y + Ht.inv(R).H
	mat_multiply(HtRi, Ht, o->cov, s->nx, o->nx, o->nx);
	mat_multiply_transpose(I, HtRi, Ht, s->nx, o->nx, s->nx, MAT_TRANSPOSE_C);
	//printf("[Kalman] Y(%f) += I(%f) \n", mat_norm(s->cov,9), mat_norm(I,9));
	mat_add(s->cov, s->cov, I, s->nx*s->nx);

	// y' = y + Ht.inv(R).(z-h(x)+H.x)
	mat_multiply(i, Ht, o->mean, s->nx, o->nx, 1);	// Ht.inv(R).z
	mat_multiply(tmp, HtRi, hx, s->nx, o->nx, 1);
	mat_sub(i,i,tmp,s->nx);				// - Ht.inv(R).hx
	mat_multiply(tmp, I, x, s->nx, s->nx, 1);
	mat_add(i,i,tmp,s->nx);				// + Ht.inv(R).H.x
	mat_add(s->mean, s->mean, i, s->nx);

	if (ts==KALMAN_TYPE_COV) kalman_convert_cov(s);
	if (to==KALMAN_TYPE_COV) kalman_convert_cov(o);
	if (ts==KALMAN_TYPE_DIAG) kalman_convert_diag(s);
	if (to==KALMAN_TYPE_DIAG) kalman_convert_diag(o);

}

void kalman_update_cov(t_kalman_state *s, t_kalman_state *o, float *hx, float *Ht)
{
	unsigned char ts = s->type;
	unsigned char to = o->type;

	kalman_convert_cov(s);
	kalman_convert_cov(o);

	float y[o->nx];
	float PHt[s->nx*o->nx];
	float invS[o->nx*o->nx];
	float S[o->nx*o->nx];
	float K[s->nx*o->nx];
	float dx[s->nx];
	float dP[s->nx*s->nx];

	// y = z - h(x)
	mat_sub(y,o->mean,hx,o->nx);

	// S = HPH' + R
	// K = PH'inv(S)
	mat_multiply(PHt,s->cov,Ht,s->nx,s->nx,o->nx);
	mat_multiply_transpose(invS,Ht,PHt,o->nx,s->nx,o->nx,MAT_TRANSPOSE_B);
	mat_add(S,invS,o->cov,o->nx*o->nx);
	mat_invert(invS,S,o->nx);
	mat_multiply(K,PHt,invS,s->nx,o->nx,o->nx);

	// x = x + K.y
	// P = P - KHP
	mat_multiply(dx,K,y,s->nx,o->nx,1);
	mat_add(s->mean,s->mean,dx,s->nx);
	mat_multiply_transpose(dP,K,PHt,s->nx,o->nx,s->nx,MAT_TRANSPOSE_C);
	mat_sub(s->cov,s->cov,dP,s->nx*s->nx);

	if (ts==KALMAN_TYPE_INFO) kalman_convert_info(s);
	if (to==KALMAN_TYPE_INFO) kalman_convert_info(o);
	if (ts==KALMAN_TYPE_DIAG) kalman_convert_diag(s);
	if (to==KALMAN_TYPE_DIAG) kalman_convert_diag(o);
}


void kalman_update_diag(t_kalman_state *s, t_kalman_state *o)
{
	unsigned char ts = s->type;
	unsigned char to = o->type;
	unsigned int i;

	kalman_convert_diag(s);
	kalman_convert_diag(o);

	for (i=0;i<s->nx;i++) {
		float K = s->cov[i]/(s->cov[i]+o->cov[i]);
		s->mean[i] += K*(o->mean[i]-s->mean[i]);
		s->cov[i] *= (1.0-K);
	}

	if (ts==KALMAN_TYPE_INFO) kalman_convert_info(s);
	if (to==KALMAN_TYPE_INFO) kalman_convert_info(o);
}


void kalman_update_old(float *x, float *P, float *z, float *R, float *hx, float *Ht, unsigned int nx, unsigned int nz)
{
	t_kalman_state *s = ks_create_header_shared(nx);
	t_kalman_state *o = ks_create_header_shared(nz);
	float xc[nx];

	memcpy(xc,x,nx*sizeof(float));

	s->mean = x; s->cov = P;
	o->mean = z; o->cov = R;

	kalman_update_info(s,o,xc,hx,Ht);

	free(s);
	free(o);
}


void kalman_score_init(t_kalman_state *s, t_kalman_state *besto, float *hx, float *Ht, float *HPHt)
{

}

float kalman_score(t_kalman_state *s, t_kalman_state *o, float *hx, float *Ht)
{
	float PHt[s->nx*o->nx];
	float invS[o->nx*o->nx];
	float S[o->nx*o->nx];
	float y[o->nx];
	float invSy[o->nx];
	float e;

	// S = HPH' + R
	mat_multiply(PHt,s->cov,Ht,s->nx,s->nx,o->nx);
	mat_multiply_transpose(S,Ht,PHt,o->nx,s->nx,o->nx,MAT_TRANSPOSE_B);
	mat_add(S,S,o->cov,o->nx*o->nx);
	mat_invert(invS,S,o->nx);

	// y = z - h(x)
	mat_sub(y,o->mean,hx,o->nx);

	// e = y'.inv(S).y
	mat_multiply(invSy,invS,y,o->nx,o->nx,1);
	e = mat_dot(y,invSy,o->nx);

	return e;

}
