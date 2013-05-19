
#include "haarwrapper.h"
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

t_haarwrapper* haarwrapper_create(const char* filename) 
{

  t_haarwrapper* hc = (t_haarwrapper*)malloc(sizeof(t_haarwrapper));
  strcpy(hc->cascade_name, filename);
  
  hc->storage = NULL;
  hc->cascade = NULL;
  
  hc->cascade = (CvHaarClassifierCascade*)cvLoad(hc->cascade_name, 0, 0, 0);
  hc->storage = cvCreateMemStorage(0);
  
  if (!hc->cascade)
  {
    printf("[haarwrapper_create] Could not load classifier cascade\n" );
    haarwrapper_destroy(hc);
    return NULL;
  }
  
  cvClearMemStorage(hc->storage);

  return hc;
}

void haarwrapper_destroy(t_haarwrapper* hc) {
  if (hc == NULL)
    return;

  cvReleaseMemStorage(&hc->storage);
  cvRelease((void**)&hc->cascade);

  free(hc);
}


guint32 haarwrapper_detect(t_haarwrapper *hc, t_haarwrapper_image* im, struct bbox_double* p, guint32 *np) 
{
 
  IplImage *img = cvCreateImageHeader(cvSize(im->width,im->height), IPL_DEPTH_8U, 3);

  img->widthStep = im->rowbytes;
  img->imageData = (char*)im->data[0];
  // opencv 2.2 expects 7 parameters
  CvSeq* obj = cvHaarDetectObjects( img, hc->cascade, hc->storage, 
                                    1.25, 2, CV_HAAR_DO_CANNY_PRUNING,
                                    cvSize(im->width/16,im->height/16),
                                    cvSize(0, 0));
  
  *np = (obj ? obj->total : 0);
  if(*np){ 
    CvRect* r = (CvRect*)cvGetSeqElem(obj, 0);
    p->x = r->x;
    p->y = r->y;
    p->w = r->width;
    p->h = r->height;

  }
/*
  guint32 i;
  for (i = 0; i < *np; i++, p += 4) {
    CvRect* r = (CvRect*)cvGetSeqElem(obj, i);
    p[0] = r->x;
    p[1] = r->y;
    p[2] = r->width;
    p[3] = r->height;
  }
*/

  cvReleaseImageHeader(&img);

  return *np;
}


void haarwrapper_drawbox(IplImage *frame, struct bbox_int* bbox, CvScalar colour)
{
  cvRectangle(frame, 
              cvPoint( bbox->x        , bbox->y ), 
              cvPoint( bbox->x+bbox->w, bbox->y+bbox->h ), 
              colour, 1, 8, 0);
}

void haarwrapper_drawtext(IplImage *frame, struct bbox_int* pos, CvScalar colour, char* text)
{
  CvFont font1;
  cvInitFont( &font1, CV_FONT_HERSHEY_SIMPLEX, 0.4f, 0.4f, 0.0f, 1, 8 );
  cvPutText( frame, text , cvPoint( pos->x, pos->y ), &font1, colour );
}

t_haarwrapper_image* haarwrapper_create_image(void) 
{
  t_haarwrapper_image* const img = (t_haarwrapper_image *)malloc(sizeof(t_haarwrapper_image));
  memset(img, 0, sizeof(t_haarwrapper_image));
  img->channels = 3;
  img->bpp = 24;
  return img;
}

void haarwrapper_alloc_image(t_haarwrapper_image* image, guint32 width, guint32 height) 
{
  haarwrapper_resize_image(image, width, height);
  haarwrapper_setdata_image(image, (guint8*)malloc(height * image->rowbytes));
}

void haarwrapper_free_image(t_haarwrapper_image* img) 
{
  if (img->data[0] != NULL)
    free(img->data[0]);
  memset(img->rowsize, 0, 4*sizeof(guint32));
  memset(img->data, 0, 4*sizeof(guint8*));
}

void haarwrapper_setdata_image(t_haarwrapper_image* image, guint8* data) 
{  
  image->data[0] = data;
  if (image->rowsize[1]) { image->data[1] = (data += image->height*image->rowsize[0]);
    if (image->rowsize[2]) { image->data[2] = (data += image->height*image->rowsize[1]);
      if (image->rowsize[3]) { image->data[3] = (data += image->height*image->rowsize[2]); }}}
}

void haarwrapper_destroy_image(t_haarwrapper_image* img) 
{
  free(img);
  img = NULL;
}


void haarwrapper_resize_image(t_haarwrapper_image* image, guint32 width, guint32 height) 
{
  image->width = width;
  image->height = height;
  image->rowbytes = width*3;
  image->rowsize[0] = image->rowbytes;
}

gboolean haarwrapper_copy_image(t_haarwrapper_image* dst, const t_haarwrapper_image* src) 
{
  for (guint32 i=0; i<4; ++i) {
    memcpy(dst->data[i], src->data[i], src->height*src->rowsize[i]);
  }
  return(TRUE);
}

guint32 haarwrapper_flip(t_haarwrapper *hc, t_haarwrapper_image* im, t_haarwrapper_image* im2) 
{
 
  IplImage *img = cvCreateImageHeader(cvSize(im->width,im->height), IPL_DEPTH_8U, 3);
  img->widthStep = im->rowbytes;
  img->imageData = (char*)im->data[0];

  IplImage *img2 = cvCreateImageHeader(cvSize(im2->width,im2->height), IPL_DEPTH_8U, 3);
  img2->widthStep = im2->rowbytes;
  img2->imageData = (char*)im2->data[0];

  cvFlip(img, img2, 1);
  return(0);
}

// EOF

