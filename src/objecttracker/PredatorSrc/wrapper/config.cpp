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
#include "wrapper.h"
#include "config.h"
//#include "qtconfiggui.h" //SSS

using namespace std;

namespace tld {

static char help_text[] =
		"usage: tld [option arguments] [arguments]\n"
		"option arguments:\n"
		"[-a <startFrameNumber>] video starts at the frameNumber <startFrameNumber>\n"
		"[-b <x,y,w,h>] Initial bounding box\n"
		"[-c] shows color images instead of greyscale\n"
		"[-d <device>] select input device: <device>=(IMGS|CAM|VID)\n"
		"    IMGS: capture from images\n"
		"    CAM: capture from connected camera\n"
		"    VID: capture from a video\n"
		"[-e <path>] export model after run to <path>\n"
		"[-f] shows foreground\n"
		"[-i <path>] <path> to the images or to the video\n"
		"[-h] shows help\n"
		"[-j <number>] specifies the <number> of the last frames which are considered by the trajectory; 0 disables the trajectory\n"
		"[-m <path>] if specified load a model from <path>. An initialBoundingBox must be specified or selectManually must be true.\n"
		"[-n <number>] specifies which camera device to use.\n"
		"[-p <path>] prints results into the file <path>\n"
		"[-q] open QT-Config GUI\n"
		"[-s] if set, user can select initial bounding box\n"
		"[-t <theta>] threshold for determining positive results\n"
		"[-z <lastFrameNumber>] video ends at the frameNumber <lastFrameNumber>.\n"
		"    If <lastFrameNumber> is 0 or the option argument isn't specified means\n"
		"    take all frames.\n"
		"arguments:\n"
		"[<path>] <path> to the config file\n";

Config::Config() :
		m_qtConfigGui(false),
		m_selectManuallySet(false),
		m_methodSet(false),
		m_startFrameSet(false),
		m_lastFrameSet(false),
		m_trajectorySet(false),
		m_showDetectionsSet(false),
		m_showForegroundSet(false),
		m_thetaSet(false),
		m_printResultsSet(false),
		m_camNoSet(false),
		m_imagePathSet(false),
		m_modelPathSet(false),
		m_initialBBSet(false),
		m_showOutputSet(false),
		m_exportModelAfterRunSet(false){
}

Config::~Config() {
}

int Config::init(int argc, char ** argv) {
	// check cli arguments
	int c;

	while((c = getopt(argc, argv, "a:b:d:e:fhi:j:m:n:Op:qst:z:")) != -1) {
		switch(c) {
		case 'a':
			m_settings.m_startFrame = atoi(optarg);
			m_startFrameSet = true;
			break;
		case 'b':
			char * pch;
			pch = strtok(optarg, ",");
			while (pch != NULL) {
				m_settings.m_initialBoundingBox.push_back(atoi(pch));
				pch = strtok(NULL, ",");
			}
			break;
		/* SSS
		case 'd':
			if(!strcmp(optarg, "CAM")) {
				m_settings.m_method = IMACQ_CAM;
				m_methodSet = true;
			} else if(!strcmp(optarg, "VID")) {
				m_settings.m_method = IMACQ_VID;
				m_methodSet = true;
			} else if(!strcmp(optarg, "IMGS")) {
				m_settings.m_method = IMACQ_IMGS;
				m_methodSet = true;
			}
			break;
		*/ //SSS
		case 'e':
			m_settings.m_exportModelAfterRun = true;
			m_settings.m_modelExportFile = optarg;
			break;
		case 'f':
			m_settings.m_showForeground = true;
			m_showForegroundSet = true;
			break;
		case 'h':
			cout << help_text;
			return PROGRAM_EXIT;
			break;
		case 'i':
			m_settings.m_imagePath = optarg;
			m_imagePathSet = true;
			break;
		case 'j':
			m_settings.m_trajectory = atoi(optarg);
			m_trajectorySet = true;
			break;
		case 'm':
			m_settings.m_loadModel = true;
			m_settings.m_modelPath = optarg;
			m_modelPathSet = true;
			break;
		case 'n':
			m_settings.m_camNo = atoi(optarg);
			m_camNoSet = true;
			break;
		case 'p':
			m_settings.m_printResults = optarg;
			m_printResultsSet = true;
			break;
		case 'O':
			m_settings.m_showOutput = false;
			m_showOutputSet = true;
			break;
		case 'q':
#ifndef WITH_QT
			cerr << "Program was build without QT-support!" << endl;
			return PROGRAM_EXIT;
#endif
			//m_qtConfigGui = true; //SSS
			break;
		case 's':
			m_settings.m_selectManually = true;
			m_selectManuallySet = true;
			break;
		case 't':
			m_settings.m_threshold = atof(optarg);
			m_thetaSet = true;
			break;
		case 'z':
			m_settings.m_lastFrame = atoi(optarg);
			m_lastFrameSet = true;
			break;
		}
	}

	#ifdef WITH_QT
		if(m_qtConfigGui || (argc == 1)) {
			bool correctClosed;
					Settings settings;
					if(getSettingsFromQtConfigGUI(argc, argv, &settings)) {
							m_settings = settings;
				return SUCCESS;
			}
		}
	#endif

	/* ----- SSS
	if(!m_imagePathSet && m_methodSet && (m_settings.m_method == IMACQ_VID || m_settings.m_method == IMACQ_IMGS)) {
		cerr <<  "Error: Must set imagePath and method if capturing from images or a video." << endl;
		return PROGRAM_EXIT;
	}
	------- */ //SSS
	if(argc > optind)
		m_configPath = argv[optind];


	return SUCCESS;
}

int Config::configure(Wrapper* wrapper) {
	
	/* SSS
	ImAcq* imAcq = wrapper->imAcq;

	// imAcq
	imAcq->method = m_settings.m_method;
	imAcq->imgPath = (m_settings.m_imagePath.empty()) ? NULL : m_settings.m_imagePath.c_str();
	imAcq->lastFrame = m_settings.m_lastFrame;
	imAcq->currentFrame = m_settings.m_startFrame;
	imAcq->camNo = m_settings.m_camNo;
	imAcq->fps = m_settings.m_fps;
	*/ //SSS

	// main
	wrapper->tld->trackerEnabled = m_settings.m_trackerEnabled;
	wrapper->showOutput = m_settings.m_showOutput;
	wrapper->printResults = (m_settings.m_printResults.empty()) ? NULL : m_settings.m_printResults.c_str();
	wrapper->saveDir = (m_settings.m_outputDir.empty()) ? NULL : m_settings.m_outputDir.c_str();
	wrapper->threshold = m_settings.m_threshold;
	wrapper->showForeground = m_settings.m_showForeground;
	wrapper->showNotConfident = m_settings.m_showNotConfident;
	wrapper->tld->alternating = m_settings.m_alternating;
	wrapper->tld->learningEnabled = m_settings.m_learningEnabled;
	wrapper->selectManually = m_settings.m_selectManually;
	wrapper->exportModelAfterRun = m_settings.m_exportModelAfterRun;
	wrapper->modelExportFile = m_settings.m_modelExportFile.c_str();
	wrapper->loadModel = m_settings.m_loadModel;
	wrapper->modelPath = (m_settings.m_modelPath.empty()) ? NULL : m_settings.m_modelPath.c_str();
	wrapper->seed = m_settings.m_seed;
	if(m_settings.m_initialBoundingBox.size() > 0) {
		wrapper->initialBB = new int[4];
		for(int i = 0; i < 4; i++) {
			wrapper->initialBB[i] = m_settings.m_initialBoundingBox[i];
		}
	}

	DetectorCascade* detectorCascade = wrapper->tld->detectorCascade;
	detectorCascade->varianceFilter->enabled = m_settings.m_varianceFilterEnabled;
	detectorCascade->ensembleClassifier->enabled = m_settings.m_ensembleClassifierEnabled;
	detectorCascade->nnClassifier->enabled = m_settings.m_nnClassifierEnabled;

	// classifier
	detectorCascade->useShift = m_settings.m_useProportionalShift;
	detectorCascade->shift = m_settings.m_proportionalShift;
	detectorCascade->minScale = m_settings.m_minScale;
	detectorCascade->maxScale = m_settings.m_maxScale;
	detectorCascade->minSize = m_settings.m_minSize;
	detectorCascade->numTrees = m_settings.m_numTrees;
	detectorCascade->numFeatures = m_settings.m_numFeatures;
	detectorCascade->nnClassifier->thetaTP = m_settings.m_thetaP;
	detectorCascade->nnClassifier->thetaFP = m_settings.m_thetaN;

	return SUCCESS;
}

/*
 POSIX getopt for Windows

 AT&T Public License

 Code given out at the 1985 UNIFORUM conference in Dallas.
 */
#ifndef __GNUC__

#define NULL	0
#define EOF	(-1)
#define ERR(s, c)	if(opterr){\
	char errbuf[2];\
	errbuf[0] = c; errbuf[1] = '\n';\
	fputs(argv[0], stderr);\
	fputs(s, stderr);\
	fputc(c, stderr);}

int opterr = 1;
int optind = 1;
int optopt;
char *optarg;

int getopt(int argc, char **argv, char *opts) {
	static int sp = 1;
	register int c;
	register char *cp;

	if (sp == 1)
		if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
			return (EOF);
		else if (strcmp(argv[optind], "--") == NULL) {
			optind++;
			return (EOF);
		}
	optopt = c = argv[optind][sp];
	if (c == ':' || (cp = strchr(opts, c)) == NULL) {
		ERR(": illegal option -- ", c);
		if (argv[optind][++sp] == '\0') {
			optind++;
			sp = 1;
		}
		return ('?');
	}
	if (*++cp == ':') {
		if (argv[optind][sp + 1] != '\0')
			optarg = &argv[optind++][sp + 1];
		else if (++optind >= argc) {
			ERR(": option requires an argument -- ", c);
			sp = 1;
			return ('?');
		} else
			optarg = argv[optind++];
		sp = 1;
	} else {
		if (argv[optind][++sp] == '\0') {
			sp = 1;
			optind++;
		}
		optarg = NULL;
	}
	return (c);
}

#endif  /* __GNUC__ */

}
