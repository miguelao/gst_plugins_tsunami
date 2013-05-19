
#include "common.h"

#include "cv.h"
#include "highgui.h"
//#include "history.h"
//#include "SmoothingFilter.h"
#include "FaceDetect.h"
#include <vector>
#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>

//static CvMemStorage* storage = 0;
//static CvHaarClassifierCascade* cascade = 0;
//const char* cascade_name = "dane.xml";
//static int FD_MinFaceSize = 80;

//static SmoothingFilter *smoothingFilter;



//Outputs a character string describing the faces found.
//Important: The caller of this function must free the character string.
// char *str = DetectFacesV2(image); DoSomething(str); free(str);
//This function simply call DetectFaces() and converts the output into a character string.
extern "C" char *DetectFacesV2(IplImage *imgIn, void *buf)
{
	FDBuf *fdBuf = (FDBuf *) buf;
	std::vector<CvRect> faces = DetectFaces(imgIn, fdBuf);
	std::string str = PrintFaces(faces);
	char *c_str = static_cast<char *>(malloc(str.length()+1));
	strcpy(c_str, str.c_str());
	return c_str;
}


std::vector<CvRect> DetectFaces(IplImage* img, FDBuf *fdBuf)
{
	
  /*
	static CvScalar colors[] = 
	{
		{{0,0,255}},
		{{0,128,255}},
		{{0,255,255}},
		{{0,255,0}},
		{{255,128,0}},
		{{255,255,0}},
		{{255,0,0}},
		{{255,0,255}}
	};
  */

	/*
	static IplImage *img = cvCreateImage( cvSize(imgIn->width,imgIn->height), IPL_DEPTH_8U, imgIn->nChannels );		
	if( imgIn->origin == IPL_ORIGIN_TL )
		cvCopy( imgIn, img, 0 );
	else
		cvFlip( imgIn, img, 0 );
	*/
	

	Int i;
	IplImage *gray;
	
	int MinSize = fdBuf->FD_MinFaceSize;

	//static History h;
	MyRect mr;
	bool valid;

	gray = cvCreateImage( cvSize(img->width,img->height), 8, 1 );
	cvCvtColor( img, gray, CV_BGR2GRAY );
	cvEqualizeHist( gray, gray );
	cvClearMemStorage(fdBuf->storage);

	vector<CvRect> faceVec;

	if( fdBuf->cascade )
	{
		Double t = ( Double ) cvGetTickCount();
		CvSeq* faces;

		faces = cvHaarDetectObjects( gray, fdBuf->cascade, fdBuf->storage,
			1.1, 2, 0/*CV_HAAR_DO_CANNY_PRUNING*/, cvSize(MinSize, MinSize) );

		t = ( Double )cvGetTickCount() - t;
		//printf( "detection time = %gms\n", t/(( Double )cvGetTickFrequency()*1000.) );

		for( i = 0; i < (faces ? faces->total : 0); i++ )
		{
			CvRect* r = (CvRect*)cvGetSeqElem( faces, i );
			
			UInt faceX = cvRound(r->x);
			UInt faceY = cvRound(r->y);
			UInt faceW = cvRound(r->width);
			UInt faceH = cvRound(r->height);

			mr.x = faceX; mr.y = faceY; mr.w = faceW; mr.h = faceH;
			valid = fdBuf->h.CheckContinuity(mr); //Caution: mr might be overwritten by design.
			if(valid)
			{
				if((int)faceW > MinSize)
				{
					faceVec.push_back(*r);
				}
				else
				{
					//printf("ccccccccccccccccccccccccc\n");
				}
			}
			else
			{
				//printf("@@@@@@@@@@@@@@@@@\n");
			}			
		}

	}
	
	fdBuf->smoothingFilter->Smooth(faceVec);

	/*
	for(unsigned int i=0; i<faceVec.size(); ++i)
	{
		CvRect *r = &(faceVec[i]);
		cvRectangle(img, cvPoint( cvRound(r->x), cvRound(r->y) ), cvPoint( cvRound(r->x + r->width), cvRound(r->y + r->height) ), 
					 colors[0], 3, 8, 0);
	}
	*/
	
	cvReleaseImage( &gray );
	fdBuf->h.UpdateTime();
	//cvWaitKey(1);

	const std::vector<CvRect>::iterator  b = faceVec.begin();
        const std::vector<CvRect>::iterator  e = faceVec.end();

	std::sort(faceVec.begin(), faceVec.end(), isLarger);
	

	return faceVec;
}





extern "C" void *InitFaceDetector2(char *cascade_name, int minfacesize)
{

  FDBuf *fdBuf = new FDBuf;
  
  fdBuf->FD_MinFaceSize = minfacesize;
  
  fdBuf->cascade = (CvHaarClassifierCascade*)cvLoad( cascade_name, 0, 0, 0 );
  
  if( !fdBuf->cascade )
  {
    //fprintf( stderr, "ERROR: Could not load classifier cascade\n" );
    //MessageBox(NULL, "Error: Could not load classifier cascade", "", MB_OK);
    fprintf(stderr, "ERROR: Could not load classifier cascade [%s]\n", cascade_name );
    exit(-1);
  }
  else
  {
    fprintf( stderr, "Classifier cascade loaded successfully\n" );
  }
  
  fdBuf->storage = cvCreateMemStorage(0);
  
  fdBuf->smoothingFilter = new SmoothingFilter();
  
  return (void *)fdBuf;
}

extern "C" void ReleaseFaceDetector(void *buf)
{
	FDBuf *fdBuf = (FDBuf *) buf;

        if(fdBuf->smoothingFilter)
          delete fdBuf->smoothingFilter;
	cvReleaseMemStorage(&(fdBuf->storage));

	delete fdBuf;
}


string PrintFaces(vector<CvRect> faces)
{
		
	/* Old code - prints to cout
	cout << "# of faces = " << faces.size() << endl;
		
	for(int i=0; i<faces.size(); ++i)
	{
		CvRect rect = faces[i];
		cout << "Face " << i+1 << ": " << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;
	}
	*/

	//new code: prints to ostringstream 

	ostringstream s;

	s << "# of faces = " << faces.size() << ";";
		
	for(unsigned int i=0; i<faces.size(); ++i)
	{
		CvRect rect = faces[i];
		//s << "Face " << i+1 << ": " << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;
		s << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << ";";
	}

	return s.str();

}

//Returns true is rect1 is larger than (or the same size as)rect2
bool isLarger(const CvRect &rect1, const CvRect &rect2)
{

	return rect1.width >= rect2.width ? true : false;

}
