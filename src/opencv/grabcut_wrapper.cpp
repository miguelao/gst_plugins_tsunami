#include "grabcut_wrapper.hpp"

using namespace cv;

int initialise_grabcut(struct grabcut_params *GC, IplImage* image_c, CvMat* mask_c)
{
  GC->image = new Mat(image_c, false); // "true" refers to copydata
  GC->mask  = new Mat(mask_c,  false);
  GC->bgdModel = new Mat(); // "true" refers to copydata
  GC->fgdModel = new Mat();

  return(0);
}

int run_graphcut_iteration(struct grabcut_params *GC, IplImage* image_c, CvMat* mask_c, CvRect* bbox)
{  
  GC->image->datastart = (uchar*)image_c->imageData;
  GC->mask->datastart  = mask_c->data.ptr;

  if( cvCountNonZero(mask_c) )
    grabCut( *(GC->image), *(GC->mask), Rect(), *(GC->bgdModel), *(GC->fgdModel), 1,  GC_INIT_WITH_MASK );

  return(0);
}

int run_graphcut_iteration2(struct grabcut_params *GC, IplImage* image_c, CvMat* mask_c, CvRect* bbox)
{  
  GC->image->datastart = (uchar*)image_c->imageData;
  GC->mask->datastart  = mask_c->data.ptr;

  grabCut( *(GC->image), *(GC->mask), *(bbox), *(GC->bgdModel), *(GC->fgdModel), 1,  GC_INIT_WITH_RECT );

  return(0);
}

int finalise_grabcut(struct grabcut_params *GC)
{
  delete GC->image;
  delete GC->mask;
  delete GC->bgdModel;
  delete GC->fgdModel;

  return(0);
}
