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
 * wrapper.h
 *
 *  Created on: Nov 18, 2011
 *      Author: Georg Nebehay
 */

#ifndef WRAPPER_H_
#define WRAPPER_H_

#include "../tld/TLD.h"
//#include "imAcq.h" SSS
//#include "gui.h" SSS

using namespace tld;

enum Retval {
	PROGRAM_EXIT = 0,
	SUCCESS = 1
};

class Wrapper {
public:
	TLD * tld;
	//ImAcq * imAcq; SSS
	//Gui * gui; SSS
	bool showOutput;
	const char * printResults;
	const char * saveDir;
	double threshold;
	bool showForeground;
	bool showNotConfident;
	bool selectManually;
	int * initialBB;
	bool reinit;
	bool exportModelAfterRun;
	bool loadModel;
	const char * modelPath;
	const char * modelExportFile;
	int seed;

	Wrapper() {
		tld = new TLD();
		showOutput = 1;
		printResults = NULL;
		saveDir = ".";
		threshold = 0.5;
		showForeground = 0;

		selectManually = 0;

		initialBB = NULL;
		showNotConfident = true;

		reinit = 0;

		loadModel = false;

		exportModelAfterRun = false;
		modelExportFile = "model";
		seed = 0;
	}

	~Wrapper() {
		delete tld;
		//imAcqFree(imAcq); SSS
	}

	void init(IplImage *img);
	void setTemplate(IplImage *img, Rect boundingBox);
	TrackerResult doWork(IplImage *img);
};

#endif /* WRAPPER_H_ */
