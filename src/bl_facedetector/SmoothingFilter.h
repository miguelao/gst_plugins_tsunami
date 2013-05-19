#ifndef __SMOOTHINGFILTER_H
#define __SMOOTHINGFILTER_H

#define _HAS_EXCEPTIONS 0 

#include "cv.h"
//#include "FaceDetect.h"
#include <vector>

class SmoothingFilter
{
public:
	SmoothingFilter(double smoothingFactor = 0.35);
	void Smooth(std::vector<CvRect> &faceRects); //Smooths each rect in the vector.
	double GetAlpha(){return alpha;}
	void SetAlpha(double alfa){alpha = alfa;}
private:
	double alpha; //The smoothing factor.
	std::vector<CvRect> currentFaceRects;
	CvRect FindClosestRect(const CvRect &rect); //Finds the rectangle in currentFaceRects that is closest (within 30 pixels) to rect. If there is no such rect, the returned rectangle has its width set to -1. 
};


#endif