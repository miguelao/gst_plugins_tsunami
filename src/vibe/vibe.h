#ifndef LIB_IMAGE_SEGMENTATION_VIBE_H
#define LIB_IMAGE_SEGMENTATION_VIBE_H

#include <opencv/cv.h>

#define t_u_int8  unsigned char
#define t_u_int16          int 
#define t_s_int32          int
#define t_u_int32 unsigned int
#define t_double  double


typedef struct {

  t_u_int8 *model;
  t_u_int8 *conf;

  t_u_int32 *random;
  t_u_int32 irandom;

  t_u_int32 width, height;

  t_u_int8 nsamples;
  t_u_int8 cardinality;
  t_u_int8 sigma_y, sigma_u, sigma_v;

  t_double lastscore;

  t_u_int32 lspeed_fg;	// 1 / fg learning speed
  t_u_int32 lspeed_bg; 	// 1 / bg learning speed

  t_u_int8 cdec_fg;	// fg confidence decrease
  t_u_int8 cinc_bg; 	// bg confidence increase

} t_vibe;

// constructor/destructor
t_vibe *vibe_create(t_u_int32 width, t_u_int32 height, t_u_int8 nsamples);
void vibe_destroy(t_vibe *vb);

// mask is on 4th channel
void vibe_initmodel(t_vibe *vb, IplImage *image);
void vibe_segment(t_vibe *vb, IplImage *image);
void vibe_update(t_vibe *vb, IplImage *image);
void vibe_display(t_vibe *vb, IplImage *image);

void vibe_display_model(t_vibe *vb, IplImage *image);

#endif
