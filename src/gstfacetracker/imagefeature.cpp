#include "imagefeature.h"
#include <string.h>
#include <math.h>
#include "mtxcore.h"
#include <opencv/cv.h>

//#define __DEBUG_HAAR_TIMING__

#ifdef __DEBUG_HAAR_TIMING__

#include <sys/time.h>

long long timeval_subtract (struct timeval *x, struct timeval *y)
{
	{
		struct timeval y2 = *y;
		/* Perform the carry for the later subtraction by updating y. */
		if (x->tv_usec < y2.tv_usec) {
			int nsec = (y2.tv_usec - x->tv_usec) / 1000000 + 1;
			y2.tv_usec -= 1000000 * nsec;
			y2.tv_sec += nsec;
		}
		if (x->tv_usec - y2.tv_usec > 1000000) {
			int nsec = (y2.tv_usec - x->tv_usec) / 1000000;
			y2.tv_usec += 1000000 * nsec;
			y2.tv_sec -= nsec;
		}

		/* Return -1 if result is negative. */
		if(x->tv_sec < y2.tv_sec) return -1;

		return (long long)(x->tv_sec - y2.tv_sec)*1000000 + x->tv_usec - y2.tv_usec;
	}
}

static inline unsigned long long clock_get_timestamp(void)
{
	struct timeval currtime;
	gettimeofday(&currtime, NULL);
	return (unsigned long long)currtime.tv_sec * 1000000ull + currtime.tv_usec;
}

typedef struct
{
	struct timeval starttime;
	struct timeval stoptime;

	float ms;
	float FPS;
}t_chrono;

inline static void startchrono(t_chrono *chrono)
{
	gettimeofday(&chrono->starttime, NULL);
}

inline static void stopchrono(t_chrono *chrono)
{
	gettimeofday(&chrono->stoptime, NULL);
	unsigned long long diff = timeval_subtract(&chrono->stoptime, &chrono->starttime);
	chrono->ms = diff / 1000.;
	chrono->FPS = 1000000. / diff;
}

inline static unsigned long long stepchrono(t_chrono *chrono)
{
	struct timeval oldstarttime = chrono->starttime;
	gettimeofday(&chrono->starttime, NULL);
	return timeval_subtract(&chrono->starttime, &oldstarttime);
}

inline static unsigned long long watchchrono(t_chrono *chrono)
{
	struct timeval stoptime;
	gettimeofday(&stoptime, NULL);
	return timeval_subtract(&stoptime, &chrono->starttime);
}
#endif

t_featurelist *featurelist_create_custom(unsigned int nfeat) {
	t_featurelist *fl = (t_featurelist *)malloc(sizeof(t_featurelist));
	featurelist_init_custom(fl, nfeat);

	return fl;
}

unsigned int featurelist_init_custom(t_featurelist *fl, unsigned int nfeat) {
	fl->feat.blob = (t_blobfeature*)malloc(nfeat * sizeof(t_blobfeature));
	fl->nfeat = nfeat;
	//fl->mediacontext = NULL; //mediacontext_create_custom();
	fl->previous = NULL;
	fl->next = NULL;

	if (nfeat == 0) {
		fl->match = NULL;
		fl->match_weight = NULL;
	}
	else {
		fl->match = (int*)malloc(nfeat * sizeof(int));
		memset(fl->match, 255, nfeat * sizeof(int));
		fl->nmatches = 0;
		fl->match_weight = (float*)malloc(nfeat * sizeof(float));
		memset(fl->match_weight, 0, nfeat * sizeof(float));
	}

	return 0;
}

unsigned int featurelist_destroy_custom(t_featurelist *fl) {
	if (fl == NULL)
		return 0;

	if (fl->match)
		free(fl->match);
	if (fl->match_weight)
		free(fl->match_weight);

	//if (fl->mediacontext) mediacontext_destroy_custom(fl->mediacontext);

	if (fl->next)
		featurelist_destroy_custom(fl->next);

	free(fl);

	return 0;
}


t_featurelist *featurelist_haar(t_image *inim, t_image *outim, t_haarclass *hc) {
	t_featurelist *fl;
	t_blobfeature *ff;
	float p[100];
	unsigned int np;
	unsigned int i;

	// detect
	if (inim->data_format != PACKED) {
		printf("[featurelist_haar] input image should be PACKED.\n");
		return 0;
	}

#ifdef __DEBUG_HAAR_TIMING__
	t_chrono *chrono = (t_chrono *)malloc(sizeof(t_chrono));
	startchrono(chrono);
#endif

	haarclass_detect(hc, inim, p, &np);

#ifdef __DEBUG_HAAR_TIMING__
	long long delay = stepchrono(chrono);
	printf("%d\n",delay);
	free(chrono);
#endif

	// export
	fl = featurelist_create_custom(np);
	for (i = 0, ff = fl->feat.blob; i < np; i++, ff++) {
		ff->p[0] = p[4 * i];
		ff->p[1] = p[4 * i + 1];
		ff->p[2] = 1.0;
		ff->w = p[4 * i + 2];
		ff->h = p[4 * i + 3];
        }


	// draw
	if (outim) {
		if (outim->data_format != PACKED) {
			printf("[featurelist_haar] output image should be PACKED.\n");
			return 0;
		}
		if (outim != inim)
			memcpy(outim->data[0], inim->data[0], outim->rowbytes * outim->height);

		IplImage *im = cvCreateImageHeader(cvSize(outim->width, outim->height), IPL_DEPTH_8U, HAARCOL_BYTESPIXEL);
		im->widthStep = outim->rowbytes;
		im->imageData = (char*)outim->data[0];

		for (i = 0, ff = fl->feat.blob; i < fl->nfeat; i++, ff++)
			cvCircle(im, cvPoint((int)ff->p[0], (int)ff->p[1]), 0.25 * (ff->w + ff->h), CV_RGB(0, 255, 0), 1, 8, 0);
		//cvRectangle(im, cvPoint((int)(ff->p[0]-ff->w/2),(int)(ff->p[1]-ff->h/2)), cvPoint((int)(ff->p[0]+ff->w/2),(int)(ff->p[1]+ff->h/2)), CV_RGB(0, 255, 0),1,8,0);

		cvReleaseImageHeader(&im);
	}

	return fl;

}
