#ifndef __HAARWRAPPER_H__
#define __HAARWRAPPER_H__

#include "bbox.h"
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <gst/gst.h>  // guint32 etc

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define RED    CV_RGB(255.0, 0.0, 0.0)
#define GREEN  CV_RGB(0.0, 255.0, 0.0)
#define BLUE   CV_RGB(0.0, 0.0, 255.0)
#define CYAN   CV_RGB(0.0, 127.0, 127.0)
#define PURPLE CV_RGB(127.0, 0.0, 127.0)
#define YELLOW CV_RGB(127.0, 127.0, 0.0)

// image type definition
typedef struct {
  guint32  height;
  guint32  width;
  guint32  channels;
  guint32  bpp;
  guint32  rowbytes;
  guint32  rowsize[4];
  guint8*  data[4];
} t_haarwrapper_image;

typedef struct {
  char cascade_name[512];
  CvMemStorage* storage;
  CvHaarClassifierCascade* cascade;
} t_haarwrapper;

t_haarwrapper* haarwrapper_create(const char* filename);
void           haarwrapper_destroy(t_haarwrapper* hc);
guint32        haarwrapper_detect(t_haarwrapper *hc, t_haarwrapper_image* im, 
                                  struct bbox_double* p, guint32 *np);
void           haarwrapper_drawbox(IplImage *frame, struct bbox_int* bbox, CvScalar colour);
void           haarwrapper_drawtext(IplImage *frame, struct bbox_int* pos, CvScalar colour, char* text);



t_haarwrapper_image* haarwrapper_create_image(void) ;
void                 haarwrapper_alloc_image(t_haarwrapper_image* image, guint32 width, guint32 height) ;
void                 haarwrapper_free_image(t_haarwrapper_image* img) ;
void                 haarwrapper_setdata_image(t_haarwrapper_image* image, guint8* data) ;
void                 haarwrapper_destroy_image(t_haarwrapper_image* img);
void                 haarwrapper_resize_image(t_haarwrapper_image* image, guint32 width, guint32 height) ;
gboolean             haarwrapper_copy_image(t_haarwrapper_image* dst, const t_haarwrapper_image* src) ;
guint32 haarwrapper_flip(t_haarwrapper *hc, t_haarwrapper_image* im, t_haarwrapper_image* im_out) ;


#endif //__HAARWRAPPER_H__
