#include "camshift.h"

#define vmin 60
#define vmax 256
#define smin 50
#define region 50

//=========================================
CvRect checkRectBoundary(CvRect A, CvRect B) {
//=========================================
	if (B.x < A.x)
		B.x = A.x;
	if (B.x + B.width > A.width)
		B.width = A.width - B.x;
	if (B.y < A.y)
		B.y = A.y;
	if (B.y + B.height > A.height)
		B.height = A.height - B.y;
	return (B);
}
//=========================================
CvKalman* initKalman(CvKalman* kalman) {
//=========================================
	const float A[] = {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1};
	kalman = cvCreateKalman(4, 2, 0);
	memcpy(kalman->transition_matrix->data.fl, A, sizeof(A));//A
	cvSetIdentity(kalman->measurement_matrix, cvScalarAll(1));//H
	cvSetIdentity(kalman->process_noise_cov, cvScalarAll(1e-5));//Q w ;
	cvSetIdentity(kalman->measurement_noise_cov, cvScalarAll(1e-1));//R v
	cvSetIdentity(kalman->error_cov_post, cvScalarAll(1));//P
	return kalman;
}

//=========================================
void getCurrState(CvKalman* kalman, CvPoint point1, CvPoint point2) {
//=========================================
	float input[4] = {point2.x, point2.y, point2.x - point1.x, point2.y - point1.y};//currentstate
	memcpy(kalman->state_post->data.fl, input, sizeof(input));
}

//=========================================
CvMat* getMeasurement(CvMat* mat, CvPoint point1, CvPoint point2) {
//=========================================
	float input[4] = {point2.x, point2.y, point2.x - point1.x, point2.y - point1.y};
	memcpy(mat->data.fl, input, sizeof(input));
	return mat;
}

//=========================================
void boundaryCheck(CvRect& box, int w, int h)
{
    //=========================================
    if (box.x < 0) box.x = 0;

    if (box.y < 0) box.y = 0;

    if (box.width <= 0)
    {
        box.width = 1;
    }

    if (box.height <= 0)
    {
        box.height = 1;
    }

    if(box.x >= w-1)
    {
        box.x = w - 2;
        box.width = 1;
    }

    if(box.y >= h-1)
    {
        box.y = h - 2;
        box.height = 1;
    }

    if(box.x + box.width >= w)
    {
        box.width = w - box.x;
    }

    if(box.y + box.height >= h)
    {
        box.height = h - box.y;
    }
}


//=========================================
void initCamKalTracker(IplImage* frame, camshift_kalman_tracker& camKalTrk) {
//=========================================
	camKalTrk.kalman = initKalman(camKalTrk.kalman);
	camKalTrk.measurement = cvCreateMat(2, 1, CV_32FC1 );//Z(k)
	camKalTrk.realposition = cvCreateMat(4, 1, CV_32FC1 );//real X(k)

	/* allocate all the buffers */
	camKalTrk.image = cvCreateImage(cvGetSize(frame), 8, 3);
	camKalTrk.image->origin = frame->origin;
	camKalTrk.hsv = cvCreateImage(cvGetSize(frame), 8, 3);
	camKalTrk.hue = cvCreateImage(cvGetSize(frame), 8, 1);
	camKalTrk.mask = cvCreateImage(cvGetSize(frame), 8, 1);

	camKalTrk.backproject = cvCreateImage(cvGetSize(frame), 8, 1);
	camKalTrk.backproject->origin = frame->origin;

	int hdims = 256;
	float hranges_arr[] = {0, 180};
	float* hranges = hranges_arr;
	camKalTrk.hist = cvCreateHist(1, &hdims, CV_HIST_ARRAY, &hranges, 1); //
	camKalTrk.selectObject = 0;
}

//=========================================
void setFaceCoords(int type, int x, int y, camshift_kalman_tracker& camKalTrk) {
//=========================================
	if (!camKalTrk.image)
		return;

	if (camKalTrk.image->origin)
		y = camKalTrk.image->height - y;

	if (camKalTrk.selectObject) {
		camKalTrk.selection.x = MIN(x,camKalTrk.origin.x);
		camKalTrk.selection.y = MIN(y,camKalTrk.origin.y);
		camKalTrk.selection.width = camKalTrk.selection.x + CV_IABS(x - camKalTrk.origin.x);
		camKalTrk.selection.height = camKalTrk.selection.y + CV_IABS(y - camKalTrk.origin.y);

		camKalTrk.selection.x = MAX( camKalTrk.selection.x, 0 );
		camKalTrk.selection.y = MAX( camKalTrk.selection.y, 0 );
		camKalTrk.selection.width = MIN( camKalTrk.selection.width, camKalTrk.image->width );
		camKalTrk.selection.height = MIN( camKalTrk.selection.height, camKalTrk.image->height );
		camKalTrk.selection.width -= camKalTrk.selection.x;
		camKalTrk.selection.height -= camKalTrk.selection.y;
	}

	switch (type) {
	case 0:
		camKalTrk.origin = cvPoint(x, y);
		camKalTrk.selection = cvRect(x, y, 0, 0);
		camKalTrk.selectObject = 1;
		break;
	case 1:
		camKalTrk.selectObject = 0;
		if (camKalTrk.selection.width > 0 && camKalTrk.selection.height > 0)
			camKalTrk.trackObject = -1;
		camKalTrk.originBox = camKalTrk.selection;

		break;
	}
}

//=========================================
CvRect camKalTrack(IplImage* frame, camshift_kalman_tracker& camKalTrk) {
//=========================================
	if (!frame)
		printf("Input frame empty!\n");

	cvCopy(frame, camKalTrk.image, 0);
	cvCvtColor(camKalTrk.image, camKalTrk.hsv, CV_BGR2HSV); // BGR to HSV

	if (camKalTrk.trackObject) {
		int _vmin = vmin, _vmax = vmax;
		cvInRangeS(camKalTrk.hsv, cvScalar(0, smin, MIN(_vmin,_vmax), 0), cvScalar(180, 256, MAX(_vmin,_vmax), 0), camKalTrk.mask); // MASK
		cvSplit(camKalTrk.hsv, camKalTrk.hue, 0, 0, 0); //  HUE
		if (camKalTrk.trackObject < 0) {
			float max_val = 0.f;
			boundaryCheck(camKalTrk.originBox, frame->width, frame->height);
			cvSetImageROI(camKalTrk.hue, camKalTrk.originBox); // for ROI
			cvSetImageROI(camKalTrk.mask, camKalTrk.originBox); // for camKalTrk.mask
			cvCalcHist(&camKalTrk.hue, camKalTrk.hist, 0, camKalTrk.mask); //
			cvGetMinMaxHistValue(camKalTrk.hist, 0, &max_val, 0, 0);
			cvConvertScale(camKalTrk.hist->bins, camKalTrk.hist->bins, max_val ? 255. / max_val : 0., 0); //  bin  [0,255]
			cvResetImageROI(camKalTrk.hue); // remove ROI
			cvResetImageROI(camKalTrk.mask);
			camKalTrk.trackWindow = camKalTrk.originBox;
			camKalTrk.trackObject = 1;
			camKalTrk.lastpoint = camKalTrk.predictpoint = cvPoint(camKalTrk.trackWindow.x + camKalTrk.trackWindow.width / 2,
					camKalTrk.trackWindow.y + camKalTrk.trackWindow.height / 2);
			getCurrState(camKalTrk.kalman, camKalTrk.lastpoint, camKalTrk.predictpoint);//input curent state
		}
		//(x,y,vx,vy),
		camKalTrk.prediction = cvKalmanPredict(camKalTrk.kalman, 0);//predicton=kalman->state_post

		camKalTrk.predictpoint = cvPoint(cvRound(camKalTrk.prediction->data.fl[0]), cvRound(camKalTrk.prediction->data.fl[1]));

		camKalTrk.trackWindow = cvRect(camKalTrk.predictpoint.x - camKalTrk.trackWindow.width / 2, camKalTrk.predictpoint.y
				- camKalTrk.trackWindow.height / 2, camKalTrk.trackWindow.width, camKalTrk.trackWindow.height);

		camKalTrk.trackWindow = checkRectBoundary(cvRect(0, 0, frame->width, frame->height), camKalTrk.trackWindow);

		camKalTrk.searchWindow = cvRect(camKalTrk.trackWindow.x - region, camKalTrk.trackWindow.y - region, camKalTrk.trackWindow.width + 2
				* region, camKalTrk.trackWindow.height + 2 * region);

		camKalTrk.searchWindow = checkRectBoundary(cvRect(0, 0, frame->width, frame->height), camKalTrk.searchWindow);

		cvSetImageROI(camKalTrk.hue, camKalTrk.searchWindow);
		cvSetImageROI(camKalTrk.mask, camKalTrk.searchWindow);
		cvSetImageROI(camKalTrk.backproject, camKalTrk.searchWindow);

		cvCalcBackProject( &camKalTrk.hue, camKalTrk.backproject, camKalTrk.hist ); // back project

		cvAnd(camKalTrk.backproject, camKalTrk.mask, camKalTrk.backproject, 0);

		camKalTrk.trackWindow = cvRect(region, region, camKalTrk.trackWindow.width, camKalTrk.trackWindow.height);

		if (camKalTrk.trackWindow.height > 5 && camKalTrk.trackWindow.width > 5) {
			// calling CAMSHIFT
			cvCamShift(camKalTrk.backproject, camKalTrk.trackWindow, cvTermCriteria(CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1),
					&camKalTrk.trackComp, &camKalTrk.trackBox);

			/*cvMeanShift( camKalTrk.backproject, camKalTrk.trackWindow,
			 cvTermCriteria( CV_TERMCRIT_EPS | CV_TERMCRIT_ITER, 10, 1 ),
			 &camKalTrk.trackComp);*/
		}
		else {
			camKalTrk.trackComp.rect.x = 0;
			camKalTrk.trackComp.rect.y = 0;
			camKalTrk.trackComp.rect.width = 0;
			camKalTrk.trackComp.rect.height = 0;
		}

		cvResetImageROI(camKalTrk.hue);
		cvResetImageROI(camKalTrk.mask);
		cvResetImageROI(camKalTrk.backproject);
		camKalTrk.trackWindow = camKalTrk.trackComp.rect;
		camKalTrk.trackWindow = cvRect(camKalTrk.trackWindow.x + camKalTrk.searchWindow.x, camKalTrk.trackWindow.y
				+ camKalTrk.searchWindow.y, camKalTrk.trackWindow.width, camKalTrk.trackWindow.height);

		camKalTrk.measurepoint = cvPoint(camKalTrk.trackWindow.x + camKalTrk.trackWindow.width / 2, camKalTrk.trackWindow.y
				+ camKalTrk.trackWindow.height / 2);
		camKalTrk.realposition->data.fl[0] = camKalTrk.measurepoint.x;
		camKalTrk.realposition->data.fl[1] = camKalTrk.measurepoint.y;
		camKalTrk.realposition->data.fl[2] = camKalTrk.measurepoint.x - camKalTrk.lastpoint.x;
		camKalTrk.realposition->data.fl[3] = camKalTrk.measurepoint.y - camKalTrk.lastpoint.y;
		camKalTrk.lastpoint = camKalTrk.measurepoint;//keep the current real position

		//measurement x,y
		cvMatMulAdd( camKalTrk.kalman->measurement_matrix/*2x4*/, camKalTrk.realposition/*4x1*/,/*measurementstate*/0, camKalTrk.measurement );
		cvKalmanCorrect(camKalTrk.kalman, camKalTrk.measurement);

		cvRectangle(frame, cvPoint(camKalTrk.trackWindow.x, camKalTrk.trackWindow.y), cvPoint(camKalTrk.trackWindow.x
				+ camKalTrk.trackWindow.width, camKalTrk.trackWindow.y + camKalTrk.trackWindow.height), CV_RGB(255,128,0), 4, 8, 0);
	}
	// set new selection if it exists
	if (camKalTrk.selectObject && camKalTrk.selection.width > 0 && camKalTrk.selection.height > 0) {
		cvSetImageROI(camKalTrk.image, camKalTrk.selection);
		cvXorS(camKalTrk.image, cvScalarAll(255), camKalTrk.image, 0);
		cvResetImageROI(camKalTrk.image);
	}

	return camKalTrk.trackWindow;
}
