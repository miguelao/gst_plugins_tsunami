#ifndef __SSIM_H__
#define __SSIM_H__

#include <cv.h>

int   initialise_ssim( int width, int height, int nchannels );
float calculate_ssim_from_iplimages( IplImage *img1_temp, IplImage *img2_temp);
int   finalise_ssim(void);

#endif /* __SSIM_H__ */
