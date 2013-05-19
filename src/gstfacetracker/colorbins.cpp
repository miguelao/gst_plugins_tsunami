#include "colorbins.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// constructor
t_colorbins* colorbins_create(unsigned char ymin, unsigned char ymax, unsigned char ny, unsigned char umin, unsigned char umax, unsigned char nu, unsigned char vmin, unsigned char vmax, unsigned char nv)
{
	t_colorbins *cb = (t_colorbins*)malloc(sizeof(t_colorbins));

	cb->ymin = ymin;	cb->ymax = ymax;	cb->ny = ny;
	cb->umin = umin;	cb->umax = umax;	cb->nu = nu;
	cb->vmin = vmin;	cb->vmax = vmax;	cb->nv = nv;

	cb->inbins = (unsigned int*)malloc(ny*nu*nv*sizeof(unsigned int));

	colorbins_reset(cb);

	return cb;
}

// destructor
void colorbins_destroy(t_colorbins* cb)
{
	free(cb->inbins);
	free(cb);
}

// deep copy
t_colorbins* colorbins_deep_copy(t_colorbins *cb)
{
	t_colorbins *cp = (t_colorbins*)malloc(sizeof(t_colorbins));

	memcpy(cp,cb,sizeof(t_colorbins));

	cp->inbins = (unsigned int*)malloc(cp->ny*cp->nu*cp->nv*sizeof(unsigned int));
	memcpy(cp->inbins,cb->inbins,cp->ny*cp->nu*cp->nv*sizeof(unsigned int));

	return cp;

}

// set all bins to zero
void colorbins_reset(t_colorbins *cb) {
	memset(cb->inbins,0,cb->ny*cb->nu*cb->nv*sizeof(unsigned int));
	memset(cb->outbins,0,27*sizeof(unsigned int));
	cb->total = 0;
}
// set all outliers to zero
void colorbins_reset_outbins(t_colorbins *cb) {
	memset(cb->outbins,0,27*sizeof(unsigned int));
}

// set counts to volume of each bin
void colorbins_set_weights(t_colorbins* cb)
{
	unsigned int i,j;
	for (i=0; i<27; i++) cb->outbins[i]=1;
	for (i=0; i<27; i+=3) { cb->outbins[i]*=cb->vmin; cb->outbins[i+1]*=(cb->vmax-cb->vmin); cb->outbins[i+2]*=(256-cb->vmax); }
	for (i=0; i<27; i+=9) for (j=0; j<3; j++) { cb->outbins[i+j]*=cb->umin; cb->outbins[i+j+3]*=(cb->umax-cb->umin); cb->outbins[i+j+6]*=(256-cb->umax); }
	for (i=0; i<6; i++) { cb->outbins[i]*=cb->ymin; cb->outbins[i+6]*=(cb->ymax-cb->ymin); cb->outbins[i+12]*=(256-cb->ymax); }
	cb->inbins[0] = ((cb->ymax-cb->ymin)*(cb->vmax-cb->vmin)*(cb->umax-cb->umin))/(cb->ny*cb->nu*cb->nv);
	const unsigned int n = cb->ny*cb->nu*cb->nv;
	for (i=0; i<n; i++) cb->inbins[i]=cb->inbins[0];
}

// get index of outlier bin
inline unsigned char colorbins_get_indexout(t_colorbins *cb, unsigned char* ii) {
	unsigned char nout = 0;
	if (ii[0]>=cb->ymax) nout+=18; else if (ii[0]>=cb->ymin) nout+=9;
	if (ii[1]>=cb->umax) nout+=6; else if (ii[1]>=cb->umin) nout+=3;
	if (ii[2]>=cb->vmax) nout+=2; else if (ii[2]>=cb->vmin) nout+=1;
	return nout;
}
// get index of inlier bin
inline unsigned int colorbins_get_indexin(t_colorbins *cb, unsigned char* ii) {
	unsigned int nin = ((ii[2]-cb->vmin)*cb->nv)/(cb->vmax-cb->vmin);
	nin += (((ii[1]-cb->umin)*cb->nu)/(cb->umax-cb->umin))*cb->nv;
	nin += (((ii[0]-cb->ymin)*cb->ny)/(cb->ymax-cb->ymin))*cb->nv*cb->nu;
	return nin;
}

// increase count of the corresponding bin for one pixel
void colorbins_addpixel(t_colorbins *cb, unsigned char* ii) {
	unsigned char nout = colorbins_get_indexout(cb, ii);
	cb->outbins[nout]++;
	if (nout==13) {
		unsigned int nin = colorbins_get_indexin(cb, ii);
		cb->inbins[nin]++;
	}
	cb->total++;
}

// increase count of the corresponding bin for all pixels within a ROI
void colorbins_addsubimage(t_colorbins *cb, t_image *image, unsigned int x1, unsigned int x2, unsigned int xs, unsigned int y1, unsigned int y2, unsigned int ys)
{
	if (xs<1) xs=1;
	if (ys<1) ys=1;

	if (x2>image->width) x2 = image->width;

	if (y2>image->height) y2 = image->height;

	unsigned int i, j;
	for (i=y1; i<y2; i+=ys) {
		unsigned char *ii = image->data[0] + image->rowbytes*i +x1*HAARCOL_BYTESPIXEL;
		for (j=x1; j<x2; j+=xs, ii+=HAARCOL_BYTESPIXEL*xs)
			colorbins_addpixel(cb, ii);

	}

}

// add n particles from a (uncorrelated) gaussian distribution
void colorbins_add_gaussian_particles(t_colorbins* cb, unsigned char my, unsigned char mu, unsigned char mv, unsigned char sy, unsigned char su, unsigned char sv, unsigned int n)
{
	unsigned int i;
	for (i=0; i<n; i++) {
		unsigned char ii[3];
		int v;
		v = sy*cos(6.283*drand48())*sqrt(fabs(2.0*log(drand48())))+my; ii[0]=(v>0)?((v<255)?v:255):0;   // Box-Muller
		v = su*cos(6.283*drand48())*sqrt(fabs(2.0*log(drand48())))+mu; ii[1]=(v>0)?((v<255)?v:255):0;
		v = sv*cos(6.283*drand48())*sqrt(fabs(2.0*log(drand48())))+mv; ii[2]=(v>0)?((v<255)?v:255):0;
		colorbins_addpixel(cb, ii);
	}
}


// get the count of the bin corresponding to a pixel
unsigned int colorbins_scorepixel(t_colorbins *cb, unsigned char* ii) {
	unsigned char nout = colorbins_get_indexout(cb, ii);
	if (nout!=13) return cb->outbins[nout];
	unsigned int nin = colorbins_get_indexin(cb, ii);
	return cb->inbins[nin];
}

// get the mean count/total of all pixels in a ROI
float colorbins_scoresubimage(t_colorbins *cb, t_image *image, unsigned int x1, unsigned int x2, unsigned int xs, unsigned int y1, unsigned int y2, unsigned int ys)
{
	if (xs<1) xs=1;
	if (ys<1) ys=1;

	if (x2>image->width) x2 = image->width;

	if (y2>image->height) y2 = image->height;

	unsigned int i, j;
	unsigned int ss = 0;
	unsigned int sn = 0;
	for (i=y1; i<y2; i+=ys) {
		unsigned char *ii = image->data[0] + image->rowbytes*i + HAARCOL_BYTESPIXEL*x1;
		for (j=x1; j<x2; j+=xs, ii+=HAARCOL_BYTESPIXEL*xs, sn++)
			ss += colorbins_scorepixel(cb, ii);
	}

	if (sn<2) return 0;

	return ((float)ss/sn)/cb->total;
}

// scale the UV channels of a YUV image to the score of the pixels according to the cb
// make sure the cb is normalized (either by volume or by another cb)
void colorbins_scoreimage(t_colorbins *cb, t_image *in, t_image *out)
{
	unsigned int i, j;
	for (i=0; i<in->height; i++) {
		unsigned char *ii = in->data[0] + in->rowbytes*i;
		unsigned char *oo = out->data[0] + out->rowbytes*i;
		for (j=0; j<in->width; j++, ii+=HAARCOL_BYTESPIXEL, oo+=HAARCOL_BYTESPIXEL) {
			unsigned int score = colorbins_scorepixel(cb, ii);
			if (score>255) score=255;
			oo[1] = (oo[1]*score)/255; oo[2] = (oo[2]*score)/255;
		}
	}

}

// normalize a cb <- fg/(fg+bg), but weighted with their respective totals
// cb, fg and bg should have same dimensions
void colorbins_normalize(t_colorbins *cb, t_colorbins *fg, t_colorbins *bg) {
	unsigned int i;
	unsigned int *cc = cb->inbins;
	unsigned int *ff = fg->inbins;
	unsigned int *bb = bg->inbins;
	const unsigned int n = cb->ny*cb->nu*cb->nv;
	for (i=0; i<n; i++, cc++, ff++, bb++) {
		if (*ff) *cc = 255*((float)(bg->total*(*ff))/((float)(fg->total*(*bb)+bg->total*(*ff)))); else *cc = 0;
		if (*cc>255) *cc=255;
	}
	cc = cb->outbins;
	ff = fg->outbins;
	bb = bg->outbins;
	for (i=0; i<27; i++, cc++, ff++, bb++) {
		if (*ff) *cc = 255*((float)(bg->total*(*ff))/((float)(fg->total*(*bb)+bg->total*(*ff)))); else *cc = 0;
		if (*cc>255) *cc=255;
	}
	cb->total = 255;
}

// divide a cb <- fg/div, but scaled with div->total
// cb, fg and div should have same dimensions
void colorbins_divide(t_colorbins *cb, t_colorbins *fg, t_colorbins *div) {
	unsigned int i;
	unsigned int *cc = cb->inbins;
	unsigned int *ff = fg->inbins;
	unsigned int *dd = div->inbins;
	const unsigned int n = cb->ny*cb->nu*cb->nv;
	for (i=0; i<n; i++, cc++, ff++, dd++) {
		if (*dd) *cc = (div->total*(*ff))/(*dd); else (*cc)=255;
		if (*cc>255) *cc=255;
	}
	cc = cb->outbins;
	ff = fg->outbins;
	dd = div->outbins;
	for (i=0; i<27; i++, cc++, ff++, dd++) {
		if (*dd) *cc = (div->total*(*ff))/(*dd); else (*cc)=255;
		if (*cc>255) *cc=255;
	}
	cb->total = 255;
}

// divide all counts in cb, including total
void colorbins_scale(t_colorbins *cb, float s) {
	unsigned int i;
	unsigned int *cc = cb->inbins;
	const unsigned int n = cb->ny*cb->nu*cb->nv;
	for (i=0; i<n; i++, cc++) *cc*=s;
	cc = cb->outbins;
	for (i=0; i<27; i++, cc++) *cc*=s;
	cb->total*=s;
}

// saturate
void colorbins_saturate_inbins(t_colorbins *cb, unsigned int max) {
	unsigned int i;
	unsigned int *cc = cb->inbins;
	const unsigned int n = cb->ny*cb->nu*cb->nv;
	for (i=0; i<n; i++, cc++)
		if (*cc>max) {*cc=max; cb->total-=(*cc-max);}
}
