/*
 //
 //                  INTEL CORPORATION PROPRIETARY INFORMATION
 //     This software is supplied under the terms of a license agreement or
 //     nondisclosure agreement with Intel Corporation and may not be copied
 //     or disclosed except in accordance with the terms of that agreement.
 //          Copyright(c) 2007-2011 Intel Corporation. All Rights Reserved.
 //
 */

#ifndef __FACEDETECTION_H__
#include "facedetection.h"
#endif

#include <opencv/cv.h>
#include <opencv/highgui.h>
/*
 *  Y component Normalization for face tracking
 */
void face_y_normalization(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean) {

	const uint32_t S = W * H;

	uint32_t mean_global = 0;
	for (uint32_t k = 0; k < S; ++k)
		mean_global += data[k*C];
	mean_global /= S;
	if (mean_global == 0)
		return;

	//printf("mean_global:%d\n",mean_global);

#ifdef CUTOUT_CCL
    if (mean_global < 80)
        return;
#endif
	if (y_mean > mean_global) {
		for (uint32_t k = 0; k < S; ++k) {
			uint8_t& val = data[k*C];
			const uint32_t tmp = (y_mean * val) / mean_global;
			if (tmp > 255)
				val = 255;
			else
				val = (uint8_t)tmp;
		}
	} else {
		for (uint32_t k = 0; k < S; ++k) {
			uint8_t& val = data[k*C];
			val = (y_mean * val) / mean_global;
		}
	}
}


#ifdef USE_IPP
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

// set 2
int init_haar_internalParams(haar_internalParams& iParams) {
	memset(&iParams, 0, sizeof(haar_internalParams));
	iParams.minneighbors = 2;
	iParams.distfactor = 15.0f;
	iParams.distfactorrect = 1.3f;
	iParams.pruningParam = 1;
	iParams.pruningParam2 = 1;
	iParams.bord = 1;
	iParams.factor = 1.0f;
	iParams.decthresh = 0.0001f;
	iParams.roi.width = 3;
	iParams.roi.height = 3;
	iParams.firstTime = 1;
	iParams.roi1.width = 10;
	ippSetNumThreads(1);
	return 0;
}

static int is_equal(int x1, int y1, int w1, int x2, int y2, int w2, float distparam, float rectparam) {
	return ((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2) <= distparam * distparam) && w1 <= w2 * rectparam;
}

CCluster::CCluster(void) {
	m_x = 0;
	m_y = 0;
	m_w = 0;
	m_count = 0;
	m_csize = 0;
	m_fsize = 0;
	m_currentclustercount = 0;

	return;
} // ctor


CCluster::~CCluster(void) {
	for (int i = 0; i < m_csize; i++) {
		if (0 != m_x[i])
		ippFree(m_x[i]);

		if (0 != m_y[i])
		ippFree(m_y[i]);

		if (0 != m_w[i])
		ippFree(m_w[i]);
	}

	if (0 != m_x)
	ippFree(m_x);

	if (0 != m_y)
	ippFree(m_y);

	if (0 != m_w)
	ippFree(m_w);

	if (0 != m_count)
	ippFree(m_count);

	return;
} // dtor


int CCluster::Init(int size, int clustercount) {
	m_currentclustercount = 0;

	m_count = (int*)ippMalloc(sizeof(int) * clustercount);
	if (0 == m_count)
	return -1;

	m_x = (int**)ippMalloc(sizeof(int*) * clustercount);
	if (0 == m_x)
	return -1;

	m_y = (int**)ippMalloc(sizeof(int*) * clustercount);
	if (0 == m_y)
	return -1;

	m_w = (int**)ippMalloc(sizeof(int*) * clustercount);
	if (0 == m_w)
	return -1;

	for (int i = 0; i < clustercount; i++) {
		m_x[i] = (int*)ippMalloc(sizeof(int) * size);
		if (0 == m_x[i])
		return -1;

		m_y[i] = (int*)ippMalloc(sizeof(int) * size);
		if (0 == m_y[i])
		return -1;

		m_w[i] = (int*)ippMalloc(sizeof(int) * size);
		if (0 == m_w[i])
		return -1;

		for (int j = 0; j < size; j++) {
			m_x[i][j] = 0;
			m_y[i][j] = 0;
			m_w[i][j] = 0;
		}

		m_count[i] = 0;
	}

	m_csize = clustercount;

	return 0;
} // CCluster::Init()


void ClusterFaces(struct haar_internalParams& iParams, Ipp8u* mask8u, IppiSize maskRoi, int maskStepint, CCluster* clusters,
		IppiSize faceSize, Ipp32f factorf) {
	int i;
	int j;
	int ii;
	int jj;
	int xcenter;
	int ycenter;
	int wrect;
	bool equalrect = false;

	for (ii = 0; ii < maskRoi.height; ii++) {
		for (jj = 0; jj < maskRoi.width; jj++) {
			if (!mask8u[maskStepint * ii + jj])
			continue;

			xcenter = (int)((jj + faceSize.width / 2) * factorf);
			ycenter = (int)((ii + faceSize.width / 2) * factorf);

			wrect = (int)(faceSize.width * factorf);

			if (clusters->m_currentclustercount) {
				for (i = 0; i < clusters->m_currentclustercount; i++) {
					for (j = 0; j < clusters->m_count[i]; j++) {
						if (is_equal(xcenter, ycenter, wrect, clusters->m_x[i][j], clusters->m_y[i][j], clusters->m_w[i][j],
										iParams.distfactor, iParams.distfactorrect)) {
							clusters->m_x[i][clusters->m_count[i]] = xcenter;
							clusters->m_y[i][clusters->m_count[i]] = ycenter;
							clusters->m_w[i][clusters->m_count[i]] = wrect;
							clusters->m_count[i]++;
							equalrect = true;
							break;
						}
					}

					if (equalrect)
					break;
				}

				if (!equalrect) {
					clusters->m_x[clusters->m_currentclustercount][0] = xcenter;
					clusters->m_y[clusters->m_currentclustercount][0] = ycenter;
					clusters->m_w[clusters->m_currentclustercount][0] = wrect;
					clusters->m_count[clusters->m_currentclustercount] = 1;
					clusters->m_currentclustercount++;
				}

				equalrect = false;
			}
			else {
				clusters->m_x[0][0] = xcenter;
				clusters->m_y[0][0] = ycenter;
				clusters->m_w[0][0] = wrect;
				clusters->m_count[0] = 1;
				clusters->m_currentclustercount++;
			}
		}
	}

	return;
} // ClusterFaces()


int SetFacesToSeq(struct haar_internalParams& iParams, CCluster* clusters, float* p, int* np) {
	int i;
	int j;
	int xsum;
	int ysum;
	int wsum;
	bool equalrect = false;

	CCluster* mergedclusters = new CCluster;
	if (0 == mergedclusters)
	return -1;

	i = mergedclusters->Init(clusters->m_currentclustercount, clusters->m_currentclustercount);
	if (i != 0)
	{
		delete mergedclusters;
		return -1;
	}

	for (i = 0, xsum = 0, ysum = 0, wsum = 0; i < clusters->m_currentclustercount; i++, xsum = 0, ysum = 0, wsum = 0) {
		if (clusters->m_count[i] >= iParams.minneighbors) {
			int xface;
			int yface;
			int wface;

			for (j = 0; j < clusters->m_count[i]; j++) {
				xsum += clusters->m_x[i][j];
				ysum += clusters->m_y[i][j];
				wsum += clusters->m_w[i][j];
			}

			wface = wsum / clusters->m_count[i];
			xface = xsum / clusters->m_count[i];
			yface = ysum / clusters->m_count[i];

			if (mergedclusters->m_currentclustercount) {
				for (int k = 0; k < mergedclusters->m_currentclustercount; k++) {
					for (int l = 0; l < mergedclusters->m_count[k]; l++) {
						if (is_equal(xface, yface, wface, mergedclusters->m_x[k][l], mergedclusters->m_y[k][l], mergedclusters->m_w[k][l],
										iParams.distfactor, iParams.distfactorrect)) {
							mergedclusters->m_x[k][mergedclusters->m_count[k]] = xface;
							mergedclusters->m_y[k][mergedclusters->m_count[k]] = yface;
							mergedclusters->m_w[k][mergedclusters->m_count[k]] = wface;
							mergedclusters->m_count[k]++;
							equalrect = true;
							break;
						}
					}

					if (equalrect)
					break;
				}

				if (!equalrect) {
					mergedclusters->m_x[mergedclusters->m_currentclustercount][0] = xface;
					mergedclusters->m_y[mergedclusters->m_currentclustercount][0] = yface;
					mergedclusters->m_w[mergedclusters->m_currentclustercount][0] = wface;
					mergedclusters->m_count[mergedclusters->m_currentclustercount] = 1;
					mergedclusters->m_currentclustercount++;
				}

				equalrect = false;
			}
			else {
				mergedclusters->m_w[0][0] = wface;
				mergedclusters->m_x[0][0] = xface;
				mergedclusters->m_y[0][0] = yface;
				mergedclusters->m_count[0] = 1;
				mergedclusters->m_currentclustercount++;
			}
		}
	}

	(*np) = 0;
	for (i = 0, xsum = 0, ysum = 0, wsum = 0; i < mergedclusters->m_currentclustercount; i++, xsum = 0, ysum = 0, wsum = 0) {

		for (j = 0; j < mergedclusters->m_count[i]; j++) {
			xsum += mergedclusters->m_x[i][j];
			ysum += mergedclusters->m_y[i][j];
			wsum += mergedclusters->m_w[i][j];
		}

		int m_count = mergedclusters->m_count[i];
		p[4 * i + 0] = xsum / m_count;
		p[4 * i + 1] = ysum / m_count;
		p[4 * i + 2] = (wsum / m_count);
		p[4 * i + 3] = (wsum / m_count);

		(*np) = (*np) + 1;
	}

	if (0 != mergedclusters)
	delete mergedclusters;

	return 0;
} // SetFaces()

IppStatus FreeHaarClassifier(struct haar_internalParams& iParams) {
	int ii;

	for (ii = 0; ii < iParams.stages; ii++) {
		iParams.status = ippiHaarClassifierFree_32f(iParams.pHaar[ii]);
		if (ippStsNoErr != iParams.status)
		return iParams.status;
	}

	if (0 != iParams.pHaar) {
		delete iParams.pHaar;
		iParams.pHaar = 0;
	}

	if (0 != iParams.sThreshold) {
		delete iParams.sThreshold;
		iParams.sThreshold = 0;
	}

	if (0 != iParams.nLength) {
		delete iParams.nLength;
		iParams.nLength = 0;
	}

	if (0 != iParams.nClass) {
		delete iParams.nClass;
		iParams.nClass = 0;
	}

	if (0 != iParams.nFeat) {
		delete iParams.nFeat;
		iParams.nFeat = 0;
	}

	if (0 != iParams.pNum) {
		delete iParams.pNum;
		iParams.pNum = 0;
	}

	if (0 != iParams.pThreshold) {
		delete iParams.pThreshold;
		iParams.pThreshold = 0;
	}

	if (0 != iParams.pVal1) {
		delete iParams.pVal1;
		iParams.pVal1 = 0;
	}

	if (0 != iParams.pVal2) {
		delete iParams.pVal2;
		iParams.pVal2 = 0;
	}

	if (0 != iParams.pFeature) {
		delete iParams.pFeature;
		iParams.pFeature = 0;
	}

	if (0 != iParams.pWeight) {
		delete iParams.pWeight;
		iParams.pWeight = 0;
	}

	if (0 != iParams.nStnum) {
		delete iParams.nStnum;
		iParams.nStnum = 0;
	}

	return ippStsNoErr;
} // FreeHaarClassifier()


const char* ReadLineFromMem(const char* ptr, char* buf) {
	const char* endptr = strchr((char*)ptr, '\n');
	if (!endptr)
	endptr = ptr + strlen(ptr);
	strncpy(buf, ptr, (int)(endptr - ptr));
	buf[endptr - ptr] = '\0';
	for (ptr = endptr; *ptr == '\n' || *ptr == '\r'; ptr++)
	;
	return ptr;
}

IppStatus ReadHaarClassifier(struct haar_internalParams& iParams, const char* cascade_path) {
	int ii;
	int jj;
	int kk;
	int jjj = 0;
	int kkk = 0;

	fflush(stdout);

	FILE* pFile = fopen(cascade_path, "r");
	if (!pFile)
	{
		printf("ERROR loading %s\n", cascade_path);
		return ippStsBadArgErr;
	}
	fseek(pFile, 0, SEEK_END);
	const int size = (int)ftell(pFile);
	fseek(pFile, 0, SEEK_SET);
	char* haardata = (char*)malloc(size+1);
	const int nRead = fread(haardata, 1, size, pFile);
	fclose(pFile);
	if (nRead != size)
	{
		free(haardata);
		return ippStsBadArgErr;
	}
	haardata[size] = 0;

	const int N = 1024;
	char line[N + 2];

	const char *ptr = haardata;

	ptr = ReadLineFromMem(ptr, line);
	sscanf(line, "%d %d %d %d %d", &(iParams.face.width), &(iParams.face.height), &iParams.stages, &iParams.classifiers,
			&iParams.features);

	iParams.pHaar = new IppiHaarClassifier_32f*[iParams.stages];
	if (0 == iParams.pHaar)
	return ippStsErr;

	iParams.sThreshold = new Ipp32f[iParams.stages];
	if (0 == iParams.sThreshold)
	return ippStsErr;

	iParams.nLength = new int[iParams.stages];
	if (0 == iParams.nLength)
	return ippStsErr;

	iParams.nStnum = new int[iParams.stages];
	if (0 == iParams.nStnum)
	return ippStsErr;

	iParams.nClass = new int[iParams.stages];
	if (0 == iParams.nClass)
	return ippStsErr;

	iParams.nFeat = new int[iParams.stages];
	if (0 == iParams.nFeat)
	return ippStsErr;

	iParams.pNum = new int[iParams.classifiers];
	if (0 == iParams.pNum)
	return ippStsErr;

	iParams.pThreshold = new Ipp32f[iParams.classifiers];
	if (0 == iParams.pThreshold)
	return ippStsErr;

	iParams.pVal1 = new Ipp32f[iParams.classifiers];
	if (0 == iParams.pVal1)
	return ippStsErr;

	iParams.pVal2 = new Ipp32f[iParams.classifiers];
	if (0 == iParams.pVal2)
	return ippStsErr;

	iParams.pFeature = new IppiRect[iParams.features];
	if (0 == iParams.pFeature)
	return ippStsErr;

	iParams.pWeight = new Ipp32f[iParams.features];
	if (0 == iParams.pWeight)
	return ippStsErr;

	for (ii = 0; ii < iParams.stages; ii++) {
		ptr = ReadLineFromMem(ptr, line);
		sscanf(line, "%d %g", iParams.nLength + ii, iParams.sThreshold + ii);
		iParams.nStnum[ii] = 0;
		iParams.nClass[ii] = jjj;
		iParams.nFeat[ii] = kkk;

		for (jj = 0; jj < iParams.nLength[ii]; jjj++, jj++) {
			int nread = 0;
			char* lptr = line;
			ptr = ReadLineFromMem(ptr, line);
			sscanf(lptr, "%d%n", iParams.pNum + jjj, &nread);
			lptr += nread;
			iParams.nStnum[ii] += iParams.pNum[jjj];

			for (kk = 0; kk < iParams.pNum[jjj]; kkk++, kk++) {
				nread = 0;
				sscanf(lptr, "%d %d %d %d %g%n", &((iParams.pFeature + kkk)->x), &((iParams.pFeature + kkk)->y), &((iParams.pFeature
										+ kkk)->width), &((iParams.pFeature + kkk)->height), iParams.pWeight + kkk, &nread);
				lptr += nread;
			}

			ptr = ReadLineFromMem(ptr, line);
			sscanf(line, "%g %g %g", iParams.pThreshold + jjj, iParams.pVal1 + jjj, iParams.pVal2 + jjj);
		}
	}

	//fclose(file);
	free(haardata);

	if ((jjj != iParams.classifiers) || (kkk != iParams.features))
	return ippStsBadArgErr;

	return ippStsNoErr;
} // ReadHaarClassifier()


IppStatus AdjustHaarClassifier(struct haar_internalParams& iParams, int border, float decstage) {
	int ii;
	int jj;
	IppiSize stageClassifierSize;

	stageClassifierSize.width = 0;
	stageClassifierSize.height = 0;

	float scale = 1.0f / (float)((iParams.face.width - border - border) * (iParams.face.height - border - border));

	for (jj = 0; jj < iParams.features; jj++)
	iParams.pWeight[jj] *= scale;

	for (ii = 0; ii < iParams.stages; ii++)
	iParams.sThreshold[ii] -= decstage;

	for (jj = 0; jj < iParams.features; jj++)
	iParams.pFeature[jj].y = iParams.face.height - iParams.pFeature[jj].y - iParams.pFeature[jj].height;

	for (ii = 0; ii < iParams.stages; ii++) {
		iParams.status = ippiHaarClassifierInitAlloc_32f(iParams.pHaar + ii, iParams.pFeature + iParams.nFeat[ii], iParams.pWeight
				+ iParams.nFeat[ii], iParams.pThreshold + iParams.nClass[ii], iParams.pVal1 + iParams.nClass[ii], iParams.pVal2
				+ iParams.nClass[ii], iParams.pNum + iParams.nClass[ii], iParams.nLength[ii]);
		if (iParams.status != ippStsOk)
		return iParams.status;

		iParams.status = ippiGetHaarClassifierSize_32f(iParams.pHaar[ii], &stageClassifierSize);
		if (iParams.status != ippStsOk)
		return iParams.status;

		if (stageClassifierSize.width > iParams.classifierSize.width)
		iParams.classifierSize.width = stageClassifierSize.width;

		if (stageClassifierSize.height > iParams.classifierSize.height)
		iParams.classifierSize.height = stageClassifierSize.height;
	}

	return ippStsNoErr;
} // AdjustHaarClassifier()


IppStatus PruningSetRow(Ipp8u* pMask, int maskStepint, IppiSize roi, int nh) {
	int i;
	IppiSize pruningRoi;
	IppStatus status = ippStsNoErr;

	if (nh <= 1)
	nh = 1;

	pruningRoi.height = nh;
	pruningRoi.width = roi.width;

	if (roi.height > nh) {
		for (i = 0; i < roi.height; i += nh + 1) {
			if (i + nh > roi.height)
			pruningRoi.height = roi.height - 1 - i;

			if (pruningRoi.height > 0)
			status = ippiSet_8u_C1R(0, pMask + maskStepint, maskStepint, pruningRoi);

			if (ippStsNoErr != status) {
				return status;
			}

			pMask += maskStepint * (nh + 1);
		}
	}

	return ippStsNoErr;
} // PruningSetRow()


IppStatus PruningSetCol(Ipp8u* pMask, int maskStepint, IppiSize roi, int nw) {
	int i;
	int j;
	int k;
	int m;
	IppiSize pruningRoi;

	if (nw <= 1)
	nw = 1;

	pruningRoi.height = roi.height;
	pruningRoi.width = nw;

	if (roi.width > nw) {
		for (i = 0; i < roi.height; i++) {
			for (j = 0; j < roi.width; j += nw + 1) {
				m = nw;
				if (j + nw > roi.width - 1)
				m = roi.width - 1 - j;

				for (k = 0; k < m; k++)
				pMask[j + 1 + k] = 0;
			}

			pMask += maskStepint;
		}
	}

	return ippStsNoErr;
} // PruningSetCol()


IppStatus PruningSetRowColMix(Ipp8u* pMask, int maskStepint, IppiSize roi, int nh, int nw) {
	int i;
	int j;
	int k;
	int m;
	IppiSize pruningRoi;
	IppStatus status = ippStsNoErr;

	if (nh <= 1)
	nh = 1;

	if (nw <= nh)
	nw = 1;

	pruningRoi.height = nh;
	pruningRoi.width = roi.width;

	if (roi.height > nh) {
		for (i = 0; i < roi.height; i += nh + 1) {
			for (j = 0; j < roi.width; j += nw + 1) {
				m = nw;
				if (j + nw > roi.width - 1)
				m = roi.width - 1 - j;

				for (k = 0; k < m; k++)
				pMask[j + 1 + k] = 0;
			}

			if (i + nh > roi.height)
			pruningRoi.height = roi.height - 1 - i;

			if (pruningRoi.height > 0)
			status = ippiSet_8u_C1R(0, pMask + maskStepint, maskStepint, pruningRoi);

			if (ippStsNoErr != status) {
				return status;
			}

			pMask += maskStepint * (nh + 1);
		}
	}

	return ippStsNoErr;
} // PruningSetRowColMix()


int StartHaar(struct haar_internalParams& iParams, const char* cascade_name) {

	iParams.status = ReadHaarClassifier(iParams, cascade_name);

	if (ippStsNoErr != iParams.status) {
		return -1;
	}

	iParams.status = AdjustHaarClassifier(iParams, iParams.bord, iParams.decthresh);
	if (ippStsNoErr != iParams.status)
	return -1;

	return 0;
}

int FiniHaar(struct haar_internalParams& iParams) {
	return FreeHaarClassifier(iParams);
}

int init_ipp_classifier(struct haar_internalParams& iParams, int width, int height, PARAMS_FCDFLT& params, const char* cascade_name) {

	if (iParams.firstTime) {
		StartHaar(iParams, cascade_name);

		iParams.firstTime = 0;

		iParams.roi0.width = width;
		iParams.roi0.height = height;

		iParams.rect0.x = 0;
		iParams.rect0.y = 0;
		iParams.rect0.width = iParams.roi0.width;
		iParams.rect0.height = iParams.roi0.height;

		iParams.rect.x = iParams.bord;
		iParams.rect.y = iParams.bord;

		iParams.pTmp = ippiMalloc_8u_C1(iParams.roi0.width, iParams.roi0.height, &iParams.tmpStep);
		if (0 == iParams.pTmp)
		return -1;

		if (params.maxfacew <= 0)
		params.maxfacew = iParams.roi0.width;

		if (params.minfacew <= 0)
                  params.minfacew = iParams.face.width;

		if (iParams.face.width > 0)
		iParams.factor = ((float)params.minfacew) / ((float)iParams.face.width);

		iParams.maxfacecount = 0;
                iParams.face.width = 20;
                iParams.face.height= 20;
		iParams.maxfacecount = ((iParams.roi0.width / iParams.face.width) * (iParams.roi0.height / iParams.face.height) / 3);
		iParams.maxrectcount = (int)(iParams.distfactor * iParams.distfactor * params.maxfacew / params.minfacew);
		iParams.maxfacecount = IPP_MIN(iParams.maxfacecount, 100);
		iParams.maxrectcount = IPP_MIN(iParams.maxrectcount, 1000);

		iParams.src32f = ippiMalloc_32f_C1((int)(iParams.roi0.width / iParams.factor + 2), (int)(iParams.roi0.height / iParams.factor
						+ 2), &iParams.src32fStep);
		if (0 == iParams.src32f)
		return -1;

		iParams.sqr = (Ipp64f*)ippiMalloc_32fc_C1((int)(iParams.roi0.width / iParams.factor + 2), (int)(iParams.roi0.height
						/ iParams.factor + 2), &iParams.sqrStep);
		if (0 == iParams.sqr)
		return -1;

		iParams.norm = ippiMalloc_32f_C1((int)(iParams.roi0.width / iParams.factor), (int)(iParams.roi0.height / iParams.factor),
				&iParams.normStep);
		if (0 == iParams.norm)
		return -1;

		iParams.mask = ippiMalloc_8u_C1((int)(iParams.roi0.width / iParams.factor), (int)(iParams.roi0.height / iParams.factor),
				&iParams.maskStep);
		if (0 == iParams.mask)
		return -1;

	}
	return 0;
}

int deinit_ipp_classifier(struct haar_internalParams& iParams) {

	if (0 != iParams.src32f)
	ippiFree(iParams.src32f);

	if (0 != iParams.sqr)
	ippiFree(iParams.sqr);

	if (0 != iParams.norm)
	ippiFree(iParams.norm);

	if (0 != iParams.mask)
	ippiFree(iParams.mask);

	if (0 != iParams.pTmp)
	ippiFree(iParams.pTmp);

	return 0;
}

int facedetection_filter(struct haar_internalParams& iParams, const CIppImage& src, PARAMS_FCDFLT& params, float* p, int* np) {

#if FACETRK_FORMAT == FACETRK_FORMAT_YUV
	ippiCopy_8u_C3C1R((const Ipp8u*)src, src.Step(), iParams.pTmp, iParams.tmpStep, iParams.roi0);
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_YUVA
	ippiCopy_8u_C4C1R((const Ipp8u*)src, src.Step(), iParams.pTmp, iParams.tmpStep, iParams.roi0);
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
	ippiCopy_8u_C3C1R((const Ipp8u*)src, src.Step(), iParams.pTmp, iParams.tmpStep, iParams.roi0);
#endif

	face_y_normalization(iParams.pTmp, src.Width(), src.Height(), 1, 200);

	int i;

	iParams.status = ippiMirror_8u_C1IR(iParams.pTmp, iParams.tmpStep, iParams.roi0, ippAxsHorizontal);
	if (ippStsNoErr != iParams.status)
	return -1;

	if (iParams.maxfacecount > 0 && iParams.maxrectcount > 0) {

		CCluster* clusters = new CCluster;
		if (0 == clusters)
		{
			delete clusters;
			return -1;
		}

		i = clusters->Init(iParams.maxrectcount, iParams.maxfacecount);
		if (i != 0)
		{
			delete clusters;
			return -1;
		}

		for (; iParams.roi0.width / iParams.factor > iParams.face.width + 5 && iParams.roi0.height / iParams.factor
				> iParams.face.height + 5 && iParams.face.width * iParams.factor < params.maxfacew; iParams.factor *= params.sfactor) {
			int src8uStep;

			iParams.roi.width = (int)(iParams.roi0.width / iParams.factor);
			iParams.roi.height = (int)(iParams.roi0.height / iParams.factor);

			Ipp8u* src8u = ippiMalloc_8u_C1(iParams.roi.width, iParams.roi.height, &src8uStep);
			if (0 == src8u)
			{
				delete clusters;
				return -1;
			}

			IppiRect dstRoi = {0,0,iParams.roi.width, iParams.roi.height};
			int bufsize = 0;
			/* calculation of work buffer size */
			ippiResizeGetBufSize( iParams.rect0, dstRoi , 1, IPPI_INTER_NN, &bufsize );
			Ipp8u* pBuffer = ippsMalloc_8u( bufsize );
			iParams.status = ippiResizeSqrPixel_8u_C1R(iParams.pTmp, iParams.roi0, iParams.tmpStep, iParams.rect0, src8u, src8uStep,
					dstRoi, 1.0 / iParams.factor, 1.0 / iParams.factor, /*IPPI_INTER_LANCZOS*/
					0, 0, IPPI_INTER_NN, pBuffer);
			ippiFree(pBuffer);
			if (ippStsNoErr != iParams.status)
			{
				delete clusters;
				if (0 != src8u) {
					ippiFree(src8u);
					src8u = 0;
				}
				return -1;
			}

			iParams.status = ippsSet_8u(0, iParams.mask, iParams.maskStep * iParams.roi.height);
			if (ippStsNoErr != iParams.status)
			{
				delete clusters;
				if (0 != src8u) {
					ippiFree(src8u);
					src8u = 0;
				}
				return -1;
			}

			iParams.roi1.width = iParams.roi.width - iParams.classifierSize.width + 1;
			iParams.roi1.height = iParams.roi.height - iParams.classifierSize.height + 1;

			iParams.rect.width = iParams.face.width - iParams.bord - iParams.bord;
			iParams.rect.height = iParams.face.height - iParams.bord - iParams.bord;

			iParams.status = ippiSqrIntegral_8u32f64f_C1R(src8u, src8uStep, iParams.src32f, iParams.src32fStep, iParams.sqr,
					iParams.sqrStep, iParams.roi, (Ipp32f)(-(1 << 24)), 0.0);
			if (ippStsNoErr != iParams.status)
			{
				delete clusters;
				if (0 != src8u) {
					ippiFree(src8u);
					src8u = 0;
				}
				return -1;
			}

			iParams.status = ippiRectStdDev_32f_C1R(iParams.src32f, iParams.src32fStep, iParams.sqr, iParams.sqrStep, iParams.norm,
					iParams.normStep, iParams.roi1, iParams.rect);
			if (ippStsNoErr != iParams.status)
			{
				delete clusters;
				if (0 != src8u) {
					ippiFree(src8u);
					src8u = 0;
				}
				return -1;
			}

			iParams.status = ippiSet_8u_C1R(1, iParams.mask, iParams.maskStep, iParams.roi1);
			if (ippStsNoErr != iParams.status)
			{
				delete clusters;
				if (0 != src8u) {
					ippiFree(src8u);
					src8u = 0;
				}
				return -1;
			}

			switch (params.pruning) {
				case RowPruning:
				PruningSetRow(iParams.mask, iParams.maskStep, iParams.roi1, iParams.pruningParam);
				break;

				case ColPruning:
				PruningSetCol(iParams.mask, iParams.maskStep, iParams.roi1, iParams.pruningParam2);
				break;

				case RowColMixPruning:
				PruningSetRowColMix(iParams.mask, iParams.maskStep, iParams.roi1, iParams.pruningParam, iParams.pruningParam2);
				break;

				default:
				break;

			}

			iParams.positive = iParams.roi1.width * iParams.roi1.height;

			for (i = 0; i < iParams.stages; i++) {
				iParams.status = ippiApplyHaarClassifier_32f_C1R(iParams.src32f, iParams.src32fStep, iParams.norm, iParams.normStep,
						iParams.mask, iParams.maskStep, iParams.roi1, &iParams.positive, iParams.sThreshold[i], iParams.pHaar[i]);
				if (ippStsNoErr != iParams.status)
				return -1;

				if (!iParams.positive)
				break;
			}

			ClusterFaces(iParams, iParams.mask, iParams.roi, iParams.maskStep, clusters, iParams.face, iParams.factor);

			if (0 != src8u) {
				ippiFree(src8u);
				src8u = 0;
			}

		}

		SetFacesToSeq(iParams, clusters, p, np);

		for (int i = 0; i < *np; i++) {
			p[4 * i + 1] = src.Size().height - p[4 * i + 1];
		}
		if (0 != clusters) {
			delete clusters;
			clusters = 0;
		}

	}

	if (iParams.face.width > 0)
	iParams.factor = ((float)params.minfacew) / ((float)iParams.face.width);

	return 0;
} // facedetection_filter()

#endif
