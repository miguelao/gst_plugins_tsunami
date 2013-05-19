#ifndef __PREDATOR_H
#define __PREDATOR_H

#include "wrapper.h"
#include "config.h"
//#include "imAcq.h" SSS
//#include "gui.h"

/* ---------------------------------

Create a class called Predator that includes the class Main.
Route messages sent to Predator to Main.
Class Main has a pointer to class TLD.
You initialize a template with a call to tld->SelectObject(grey, &rect); //This is now done within main->doWork
You process the next frame with a call to tld->processImg(img);//This is now done within main->doWork


------------------------------------- */
namespace tld {

class Predator
{
public:
	Predator();
	~Predator();
	void init(IplImage *img);
	void setTemplate(IplImage *img, Rect boundingBox);
    void resetTemplate(IplImage *img, Rect boundingBox);
	TrackerResult doWork(IplImage *img);
private:
	Wrapper	*wrapper;
	Config	config;
	//ImAcq	*imAcq; SSS
	//Gui		*gui; SSS
};

} //namespace tld 
#endif
