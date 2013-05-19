#include "predator.h"

/* ---------------------------------

Create a class called Predator that includes the class Main.
Route messages sent to Predator to Main.
Class Main has a pointer to class TLD.
You initialize a template with a call to tld->SelectObject(grey, &rect); //This is now done within main->doWork
You process the next frame with a call to tld->processImg(img);//This is now done within main->doWork
------------------------------------- */
namespace tld {


Predator::Predator()
{
	wrapper = new Wrapper();
	//imAcq = imAcqAlloc(); SSS
	//gui = new Gui(); SSS

	//wrapper->gui = gui; SSS
	//wrapper->imAcq = imAcq; SSS

	config.configure(wrapper);

	srand(wrapper->seed);

	//imAcqInit(imAcq); SSS

	//gui->init(); SSS
}

Predator::~Predator()
{
	delete wrapper; 
}

void Predator::init(IplImage *img)
{
	wrapper->init(img);
}

void Predator::setTemplate(IplImage *img, Rect boundingBox)
{
	wrapper->setTemplate(img, boundingBox);
}

TrackerResult Predator::doWork(IplImage *img)
{
	return wrapper->doWork(img); //wrapper->doWork() return "TrackerResult". Just return it from this funcition
}


void Predator::resetTemplate(IplImage *img, Rect boundingBox)
{
	delete wrapper;
	wrapper = new Wrapper();
	config.configure(wrapper);
	srand(wrapper->seed);
	wrapper->init(img);
	wrapper->setTemplate(img, boundingBox);
}



} //namespace tld
