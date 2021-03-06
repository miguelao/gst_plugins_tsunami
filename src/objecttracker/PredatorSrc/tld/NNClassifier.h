/*  Copyright 2011 AIT Austrian Institute of Technology
*
*   This file is part of OpenTLD.
*
*   OpenTLD is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   OpenTLD is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with OpenTLD.  If not, see <http://www.gnu.org/licenses/>.
*
*/
/*
 * NNClassifier.h
 *
 *  Created on: Nov 16, 2011
 *      Author: Georg Nebehay
 */

#ifndef NNCLASSIFIER_H_
#define NNCLASSIFIER_H_

#include <vector>
#include <opencv/cv.h>

#include "../tld/NormalizedPatch.h"
#include "../tld/DetectionResult.h"

using namespace std;
using namespace cv;

namespace tld {

class NNClassifier {
	float ncc(float *f1,float *f2);
public:
	bool enabled;

	int * windows;
	float thetaFP;
	float thetaTP;
	DetectionResult* detectionResult;
	vector<NormalizedPatch>* falsePositives;
	vector<NormalizedPatch>* truePositives;

	NNClassifier();
	virtual ~NNClassifier();

	void release();
	float classifyPatch(NormalizedPatch * patch);
	float classifyBB(Mat img, Rect* bb);
	float classifyWindow(Mat img, int windowIdx);
	void learn(vector<NormalizedPatch> patches);
	bool filter(Mat img, int windowIdx);
};

} /* namespace tld */
#endif /* NNCLASSIFIER_H_ */
