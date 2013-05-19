#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <opencv/cxcore.h>


struct camshift_kalman_tracker {
	CvMat* measurement;
	CvMat* realposition;
	const CvMat* prediction;
	IplImage *image, *hsv, *hue, *mask, *backproject;
	CvHistogram *hist;
	CvPoint lastpoint, predictpoint, measurepoint;
	int selectObject, trackObject;
	CvPoint origin;

	CvRect selection, originBox;

	CvRect trackWindow, searchWindow;
	CvBox2D trackBox;
	CvKalman* kalman;
	CvConnectedComp trackComp;
};

void initCamKalTracker(IplImage* frame, camshift_kalman_tracker& camKalTrk );
void setFaceCoords( int type, int x, int y, camshift_kalman_tracker& camKalTrk);
CvRect camKalTrack( IplImage* frame, camshift_kalman_tracker& camKalTrk);
void boundaryCheck(CvRect& box, int w, int h);
