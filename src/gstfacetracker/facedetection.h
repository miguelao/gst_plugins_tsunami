/*
 //
 //                  INTEL CORPORATION PROPRIETARY INFORMATION
 //     This software is supplied under the terms of a license agreement or
 //     nondisclosure agreement with Intel Corporation and may not be copied
 //     or disclosed except in accordance with the terms of that agreement.
 //          Copyright(c) 2011 Intel Corporation. All Rights Reserved.
 //
 */

#ifndef __FACEDETECTION_H__
#define __FACEDETECTION_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

void face_y_normalization(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean);


#ifdef USE_IPP

#include "ipp.h"


#ifndef __IPPIMAGE_H__
#include "ippimage.h"
#endif

#include "defines.h"

#include <sys/time.h>

#include <stdio.h>

/*
 * #ifndef __IPPDEFS_H__
#include "ippdefs.h"
#endif
#ifndef __IPPCORE_H__
#include "ippcore.h"
#endif
#ifndef __IPPS_H__
#include "ipps.h"
#endif
#ifndef __IPPI_H__
#include "ippi.h"
#endif
#ifndef __IPPCV_H__
#include "ippcv.h"
#endif
#include "ippcc.h"
#include "ippcore.h"
*/
class CCluster
{
public:
	int** m_x;
	int** m_y;
	int** m_w;
	int* m_count;
	int m_csize;
	int m_fsize;
	int m_currentclustercount;
	int m_prevclustercount;

	CCluster(void);
	~CCluster(void);

	int Init(int size, int clustercount);
	int DeInit();
};

typedef enum
{
	NoPruning = 0,
	RowPruning = 1,
	ColPruning = 2,
	RowColMixPruning = 3,
	CannyPruning = 4

}pruningType;

typedef struct
{
	int nthreads;
	int minfacew;
	int maxfacew;
	float sfactor;
	pruningType pruning;

}PARAMS_FCDFLT;

struct haar_internalParams {

	int minneighbors;
	float distfactor;
	float distfactorrect;

	IppiHaarClassifier_32f** pHaar;

	int stages;
	int classifiers;
	int features;
	int positive;

	int* nLength;
	int* nClass;
	int* nFeat;
	int* pNum;
	int* nStnum;

	IppiRect* pFeature;

	Ipp32f* pWeight;
	Ipp32f* pThreshold;
	Ipp32f* pVal1;
	Ipp32f* pVal2;
	Ipp32f* sThreshold;
	IppiSize classifierSize;

	IppiSize face;

	// set 2
	int pruningParam;
	int pruningParam2;
	int maxfacecount;
	int maxrectcount;

	Ipp32s bord;

	Ipp32f factor;
	Ipp32f decthresh;
	IppiSize roi;
	IppiSize roi0;
	IppiSize roi1;
	IppiRect rect;
	IppiRect rect0;
	IppStatus status;

	int tmpStep;
	Ipp8u* pTmp;

	Ipp32f* src32f;
	Ipp64f* sqr;
	Ipp32f* norm;
	Ipp8u* mask;

	int src32fStep;
	int sqrStep;
	int normStep;
	int maskStep;
	int firstTime;

};

int init_haar_internalParams(haar_internalParams& iParams);
int facedetection_filter( struct haar_internalParams& iParams, const CIppImage& src, PARAMS_FCDFLT& params, float* p, int* np);
int init_ipp_classifier( struct haar_internalParams& iParams, int width, int height, PARAMS_FCDFLT& params, const char* cascade_name);
int deinit_ipp_classifier(struct haar_internalParams& iParams);
int StartHaar(struct haar_internalParams* iParams, const char* cascade_name);
int FiniHaar(struct haar_internalParams& iParams);
#endif // __FACEDETECTION_H__

#endif
