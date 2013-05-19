#ifndef __FACEDETECT_H
#define __FACEDETECT_H

#define _HAS_EXCEPTIONS 0  

#include "cv.h"
#include "highgui.h"
#include "SmoothingFilter.h"
#include "history.h"
#include <stdlib.h>
#include <vector>
#include <string>


struct FDBuf
{
	CvMemStorage			*storage;
	CvHaarClassifierCascade *cascade;
	int						FD_MinFaceSize;
	SmoothingFilter			*smoothingFilter;
	History					h;

};

std::vector<CvRect> DetectFaces( IplImage* img, FDBuf *fdBuf);
std::string PrintFaces(std::vector<CvRect> faces);
bool isLarger(const CvRect &rect1, const CvRect &rect2); //Returns true is rect1 is larger than rect2

#endif
