#ifndef IMAGEFEATURE_H
#define IMAGEFEATURE_H

#include "defines.h"
#include "haarclass.h"
//#include <lib/media/mediacontext.h>


// Blob feature.
typedef struct {
	float 	p[3];
	float 	w, h;
} t_blobfeature;

// vector of features
typedef struct featurelist {
	union {
		t_blobfeature	*blob;
	} feat;									// list of <nfeat> features of <type> type
	unsigned int nfeat;						// number of features in <feat>
	unsigned int nmatches; 					// number of matches found
	int *match;						// <nfeat> indices of found forward matches in other featurelist structure (<0 if no match)
	//unsigned int nmatches; 				// number of matches found
	float *match_weight;				// vector of <nfeat> doubles indicating importance of matches.
	//t_mediacontext *mediacontext;		// media context
	struct featurelist *previous; 	// pointer to previous feature list, if linked
	struct featurelist *next;			// pointer to next feature list, if linked
} t_featurelist;


// INIT
t_featurelist *featurelist_create_custom(unsigned int nfeat);
unsigned int featurelist_init_custom(t_featurelist *fl, unsigned int nfeat);
unsigned int featurelist_destroy_custom(t_featurelist *fl);

// EXTRACTION
t_featurelist *featurelist_haar(t_image *inim, t_image *outim, t_haarclass *hc);

#endif
