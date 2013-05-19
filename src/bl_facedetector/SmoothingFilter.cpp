
#include "SmoothingFilter.h"
#include <iostream>



SmoothingFilter::SmoothingFilter(double smoothingFactor) : alpha(smoothingFactor)
{
	currentFaceRects.clear();
}

//Finds the rectangle in currentFaceRects that is closest (within 30 pixels) to rect. 
//If there is no such rect, the returned rectangle has its width set to -1. 
CvRect SmoothingFilter::FindClosestRect(const CvRect &rect)
{

	CvRect closest = cvRect(-1, -1, -1, -1);
	int x = rect.x, y = rect.y;

	double minDistSq = 1000000.0; //Some large number
	double distThresholdSq = 900; //30 * 30
	for(unsigned int i=0; i<currentFaceRects.size(); ++i)
	{
		CvRect r = currentFaceRects[i];
		int dx = x - r.x;
		int dy = y - r.y;
		double distSq = dx*dx+dy*dy;
		//dist = static_cast<int> (sqrt(static_cast<double>(dist)));
		if((distSq < distThresholdSq) && (distSq < minDistSq))
		{
			minDistSq = distSq;
			closest = r;
		}
	}

	return closest;
}

//Smooths each rect in the vector.
void SmoothingFilter::Smooth(std::vector<CvRect> &faceRects)
{

	for(unsigned int i=0; i<faceRects.size(); ++i)
	{
		CvRect r = faceRects[i];
		CvRect c = FindClosestRect(r);
		if(c.width > -1)
		{
			c.x += static_cast<int> (alpha*(r.x - c.x) + 0.5);
			c.y += static_cast<int> (alpha*(r.y - c.y) + 0.5);
			double alpha2 = alpha - 0.2; if(alpha2 < 0.0) alpha2 = 0.0;
			c.width += static_cast<int> (alpha2*(r.width - c.width) + 0.5);
			c.height += static_cast<int> (alpha2*(r.height - c.height) + 0.5);
			
			faceRects[i] = c;
		}
	}
	
	currentFaceRects = faceRects;
}


