#include "haarclass.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

t_haarclass* haarclass_create(const char* filename) {

	t_haarclass* hc = (t_haarclass*)malloc(sizeof(t_haarclass));
	strcpy(hc->cascade_name, filename);

	#ifdef USE_IPP
	init_haar_internalParams(hc->iParams);
	printf("Using cascade file:%s\n", hc->cascade_name);
	#else
	hc->storage = NULL;
	hc->cascade = NULL;

	hc->cascade = (CvHaarClassifierCascade*)cvLoad(hc->cascade_name, 0, 0, 0);
	hc->storage = cvCreateMemStorage(0);

	if (!hc->cascade)
	{
		printf("[haarclass_create] Could not load classifier cascade\n" );
		haarclass_destroy(hc);
		return NULL;
	}

	cvClearMemStorage(hc->storage);
	#endif
	return hc;
}

void haarclass_destroy(t_haarclass* hc) {
	if (hc == NULL)
		return;
	#ifdef USE_IPP
	deinit_ipp_classifier(hc->iParams);
	FiniHaar(hc->iParams);
	#else
	cvReleaseMemStorage(&hc->storage);
	cvRelease((void**)&hc->cascade);
	#endif
	free(hc);
}

// p: np x 4 matrix of (cx,cy,w,h) defining rectangles around detected objects (must be allocated beforehand)
// im must be PACKED


unsigned int haarclass_detect(t_haarclass *hc, t_image* im, float* p, unsigned int *np) {

#ifdef USE_IPP

	*np = 0;
	PARAMS_FCDFLT params;

	CIppImage src;

	params.nthreads = 1;
	params.minfacew = (int)(im->width/8);
	params.maxfacew = (int)(im->width);
	params.sfactor = 1.1;
	params.pruning = RowColMixPruning;

	src.Attach(im->width, im->height, HAARCOL_BYTESPIXEL, 8, im->data[0], (im->width)*HAARCOL_BYTESPIXEL);

	init_ipp_classifier(hc->iParams, im->width, im->height, params, hc->cascade_name);

	facedetection_filter(hc->iParams,src, params, p, (int*)np);

#else

	unsigned int i;

	IplImage *img = cvCreateImageHeader(cvSize(im->width,im->height),IPL_DEPTH_8U,HAARCOL_BYTESPIXEL);

	img->widthStep = im->rowbytes;
	img->imageData = (char*)im->data[0];
	// opencv 2.2 expects 7 parameters
	CvSeq* obj = cvHaarDetectObjects( img, hc->cascade, hc->storage, 1.2, 2, 0, cvSize(im->width/8,im->height/8) );
	// opencv 2.1 expects 8 parameters
	//CvSeq* obj = cvHaarDetectObjects( img, hc->cascade, hc->storage, 1.2, 2, 0, cvSize(40, 40), cvSize(0, 0) );

	*np = (obj ? obj->total : 0);
	for (i = 0; i < *np; i++, p += 4) {
		CvRect* r = (CvRect*)cvGetSeqElem(obj, i);
		p[0] = r->x + r->width / 2.0;
		p[1] = r->y + r->height / 2.0;
		p[2] = r->width;
		p[3] = r->height;
	}

	cvReleaseImageHeader(&img);
#endif

	return *np;
}
