#ifndef LIB_IMAGE_SEGMENTATION_COLORBINS_H
#define LIB_IMAGE_SEGMENTATION_COLORBINS_H

#include "defines.h"
#include "image.h"

typedef struct {
	unsigned int *inbins;
	unsigned int outbins[27];
	unsigned int total;
	unsigned char ymin, ymax, umin, umax, vmin, vmax;
	unsigned char ny, nu, nv;
} t_colorbins;

// constructor
t_colorbins* colorbins_create(unsigned char ymin, unsigned char ymax, unsigned char ny, unsigned char umin, unsigned char umax, unsigned char nu, unsigned char vmin, unsigned char vmax, unsigned char nv);
// destructor
void colorbins_destroy(t_colorbins* cb);
// deep copy
t_colorbins* colorbins_deep_copy(t_colorbins *cb);


// set all bins to zero
void colorbins_reset(t_colorbins *cb);
// set counts to volume of each bin
void colorbins_set_weights(t_colorbins* cb);
// set all outliers to zero
void colorbins_reset_outbins(t_colorbins *cb);

// get index of outlier bin
inline unsigned char colorbins_get_indexout(t_colorbins *cb, unsigned char* ii);
// get index of inlier bin
inline unsigned int colorbins_get_indexin(t_colorbins *cb, unsigned char* ii);



// increase count of the corresponding bin for one pixel
void colorbins_addpixel(t_colorbins *cb, unsigned char* ii);
// increase count of the corresponding bin for all pixels within a ROI
void colorbins_addsubimage(t_colorbins *cb, t_image *image, unsigned int x1, unsigned int x2, unsigned int xs, unsigned int y1, unsigned int y2, unsigned int ys);
// add n particles from a (uncorrelated) gaussian distribution
void colorbins_add_gaussian_particles(t_colorbins* cb, unsigned char my, unsigned char mu, unsigned char mv, unsigned char sy, unsigned char su, unsigned char sv, unsigned int n);




// get the count of the bin corresponding to a pixel
unsigned int colorbins_scorepixel(t_colorbins *cb, unsigned char* ii);
// get the mean count/total of all pixels in a ROI
float colorbins_scoresubimage(t_colorbins *cb, t_image *image, unsigned int x1, unsigned int x2, unsigned int xs, unsigned int y1, unsigned int y2, unsigned int ys);
// scale the UV channels of a YUV image to the score of the pixels according to the cb
// make sure the cb is normalized (either by volume or by another cb)
void colorbins_scoreimage(t_colorbins *cb, t_image *in, t_image *out);



// normalize a cb <- fg/(fg+bg), but weighted with their respective totals
// cb, fg and bg should have same dimensions
void colorbins_normalize(t_colorbins *cb, t_colorbins *fg, t_colorbins *bg);
// divide a cb <- fg/div, but scaled with div->total
// cb, fg and div should have same dimensions
void colorbins_divide(t_colorbins *cb, t_colorbins *fg, t_colorbins *div);
// scale all counts in cb, including total
void colorbins_scale(t_colorbins *cb, float s);
// saturate
void colorbins_saturate_inbins(t_colorbins *cb, unsigned int max);

#endif
