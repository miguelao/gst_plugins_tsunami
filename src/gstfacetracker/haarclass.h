#ifndef LIB_IMAGE_FEATURES_HAAR_HAARCLASS_H
#define LIB_IMAGE_FEATURES_HAAR_HAARCLASS_H

#include <opencv/cv.h>
#include <opencv/highgui.h>
#include "defines.h"
#include "image.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef USE_IPP
#include "facedetection.h"
#endif

typedef struct {
#ifdef USE_IPP
	struct haar_internalParams iParams;
	char cascade_name[512];
#else
	char cascade_name[512];
	CvMemStorage* storage;
	CvHaarClassifierCascade* cascade;
#endif
} t_haarclass;

t_haarclass* haarclass_create(const char* filename);
void haarclass_destroy(t_haarclass* hc);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// wrapper around opencv's haarclassifier
//
// p: np x 4 matrix of (cx,cy,w,h) defining rectangles around detected objects (must be allocated beforehand)
// im must be PACKED
//
unsigned int haarclass_detect(t_haarclass *hc, t_image* im, float *p, unsigned int *np);

#endif
