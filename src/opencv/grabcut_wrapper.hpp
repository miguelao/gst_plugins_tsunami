
#ifndef __GRABCUT_WRAPPER_HPP__
#define __GRABCUT_WRAPPER_HPP__

#include <opencv/cv.h>

using namespace cv;
struct grabcut_params{
  Mat* bgdModel;
  Mat* fgdModel;
  Mat* image;
  Mat* mask;  
};

int initialise_grabcut(struct grabcut_params *GC, IplImage* image_c, CvMat* mask_c);
int run_graphcut_iteration(struct grabcut_params *GC, IplImage* image_c, CvMat* mask_c, CvRect* bbox);
int run_graphcut_iteration2(struct grabcut_params *GC, IplImage* image_c, CvMat* mask_c, CvRect* bbox);
int finalise_grabcut(struct grabcut_params *GC);




#endif //#define __GRABCUT_WRAPPER_HPP__
