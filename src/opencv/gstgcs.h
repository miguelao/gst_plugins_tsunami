/* GStreamer
 */

#ifndef __GST_GCS_H__
#define __GST_GCS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>
#include "grabcut_wrapper.hpp"

// if we define KMEANS, the torso bbox is somehow re-centered using the largest 
// colour-spatial cluster as found by k-Means algorithm
#undef KMEANS 


G_BEGIN_DECLS

#define GST_TYPE_GCS \
	(gst_gcs_get_type())
#define GST_GCS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GCS,GstGcs))
#define GST_GCS_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GCS,GstGcsClass))
#define GST_IS_GCS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GCS))
#define GST_IS_GCS_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GCS))

typedef struct _GstGcs GstGcs;
typedef struct _GstGcsClass GstGcsClass;

//////////////////////////////////////////////
typedef struct curlMemoryStructGCS {        //
  unsigned char *memory;                    //
  size_t size;                              //
  size_t pos;                               //
} curlMemoryStructGCS;                      //
                                            //
enum PICSRC_COLOR_TYPE_GCS {                //
  COLOR_TYPE_GCS_GRAY = 0,                  //
  COLOR_TYPE_GCS_PALETTE,                   //
  COLOR_TYPE_GCS_RGB, 		            //
  COLOR_TYPE_GCS_RGB_ALPHA,                 //
  COLOR_TYPE_GCS_GRAY_ALPHA,                //
  COLOR_TYPE_GCS_UNKNOWN                    //
};                                          //
                                            //
typedef struct picSrcImageInfoGCS {         //
  unsigned int width;                       //
  unsigned int height;                      //
  int          channels;                    //
  int          bpp;                         //
  PICSRC_COLOR_TYPE_GCS colorspace;         //
} picSrcImageInfoGCS;                       //
//////////////////////////////////////////////


struct _GstGcs {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height, numframes;
  
  bool      display;  
  int       debug;  

  IplImage* pImageRGBA ; // 4channel input

  IplImage* pImgRGB;     // 3channel version of the 4channel input
  IplImage* pImgScratch; // 3channel scratchpad

  IplImage* pImgGRAY;       // Gray of the RGB image
  IplImage* pImgGRAY_copy;  // Gray of the RGB image
  IplImage* pImgGRAY_diff;  // Smoothed difference between GRAY and GRAY_1
  IplImage* pImgGRAY_1;     // storage of previous pImgGRAY
  IplImage* pImgGRAY_1copy; // Gray of the RGB image

  IplImage* pImgChA;     // Alpha channel of the input
  IplImage* pImgCh1;
  IplImage* pImgCh2;
  IplImage* pImgCh3;
  IplImage* pImgChX;     // Alpha channel of the incoming input

#ifdef KMEANS
  IplImage* pImgRGB_kmeans;  // Copy of input with backpropagated colour clusters
  CvMat*    kmeans_points;   // K-Means points ( rows of (r,g,b,x,y)
  CvMat*    kmeans_clusters; // K-Means clusters
  int       num_clusters;
  int       num_samples;     // amount of points of training sample
#endif// KMEANS

  // GCS stuff
  IplImage*  pImg_skin;       // Skin colour pixels as {255} or {0}

  CvMat*     grabcut_mask; // mask created by graphcut
  CvRect     bbox_prev;
  CvRect     bbox_now;
  CvRect     facepos;
  gboolean   facefound;

  struct grabcut_params GC;

  //////////////////////////////////////////////////////////////////////////////
  // files related stuff
  gchar* ghostfilename;
  IplImage            *cvGhost;
  IplImage            *cvGhostBw;
  IplImage            *cvGhostBwResized;
  IplImage            *cvGhostBwAffined;
  // png load stuff
  unsigned char *raw_image;

  picSrcImageInfoGCS info;

  // point arrays for the affine transform
  CvPoint2D32f srcTri[3], dstTri[3];
  CvMat* warp_mat;

};

struct _GstGcsClass {
  GstVideoFilterClass parent_class;
};

GType gst_gcs_get_type(void);

G_END_DECLS

gboolean gst_gcs_plugin_init(GstPlugin * plugin);

#endif /* __GST_GCS_H__ */
