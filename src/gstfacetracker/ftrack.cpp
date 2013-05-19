#include "ftrack.h"

//=========================================
ftrack::ftrack()
//=========================================
{
	nrFrames = 0;
	faceSeen = false;
}

//=========================================
void ftrack::init()
//=========================================
{
	nrFrames = 0;
	faceSeen = false;
}

//=========================================
void ftrack::reset()
//=========================================
{
	faceSeen = false;
	init();
}

//=========================================
ftrack::~ftrack()
//=========================================
{
  ;
}

//=========================================
bool ftrack::updateFaceCoords(CvRect in, CvRect& out, bool isFace)
//=========================================
{
	// don't add data to history until face is seen atleast once
	if(!faceSeen && isFace)
	{
		faceSeen = true;
	}
	else if(!faceSeen)
		{
			out = in;
			return false;
		}

	face_x[nrFrames%FTRACK_HISTORY_FRAMES] = in.x;
	face_y[nrFrames%FTRACK_HISTORY_FRAMES] = in.y;
	face_w[nrFrames%(FTRACK_HISTORY_FRAMES_RAD)] = in.width;
	face_h[nrFrames%(FTRACK_HISTORY_FRAMES_RAD)] = in.height;

	int avg_x = 0;
	int avg_y = 0;
	int avg_w = 0;
	int avg_h = 0;

	for(int i=0; i < FTRACK_HISTORY_FRAMES; i++)
	{
		avg_x += face_x[(nrFrames-i)%FTRACK_HISTORY_FRAMES];
		avg_y += face_y[(nrFrames-i)%FTRACK_HISTORY_FRAMES];
		avg_w += face_w[(nrFrames-i)%(FTRACK_HISTORY_FRAMES_RAD)];
		avg_h += face_h[(nrFrames-i)%(FTRACK_HISTORY_FRAMES_RAD)];
	}
	for(int i=FTRACK_HISTORY_FRAMES;  i < FTRACK_HISTORY_FRAMES_RAD; i++)
	{
		avg_w += face_w[(nrFrames-i)%(FTRACK_HISTORY_FRAMES_RAD)];
		avg_h += face_h[(nrFrames-i)%(FTRACK_HISTORY_FRAMES_RAD)];
	}

	if(nrFrames < (FTRACK_HISTORY_FRAMES))
		{
			out	= in;
			avg_x = avg_x/(nrFrames+1);
			avg_y = avg_y/(nrFrames+1);
		}
	else if(nrFrames < FTRACK_HISTORY_FRAMES_RAD)
		{
			out = in;
			out.x = avg_x/FTRACK_HISTORY_FRAMES;
			out.y = avg_y/FTRACK_HISTORY_FRAMES;
		}
	else
	{
		avg_x = avg_x/FTRACK_HISTORY_FRAMES;
		avg_y = avg_y/FTRACK_HISTORY_FRAMES;
		avg_w = avg_w/(FTRACK_HISTORY_FRAMES_RAD);
		avg_h = avg_h/(FTRACK_HISTORY_FRAMES_RAD);

		out.x = avg_x;
		out.y = avg_y;
		out.width = avg_w;
		out.height = avg_h;
	}

	nrFrames++;
	return true;
}

