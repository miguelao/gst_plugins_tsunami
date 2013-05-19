/* GStreamer
 */

#ifndef __GST_GC_H__
#define __GST_GC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>
#include "grabcut_wrapper.hpp"

G_BEGIN_DECLS

#define GST_TYPE_GC \
	(gst_gc_get_type())
#define GST_GC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GC,GstGc))
#define GST_GC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GC,GstGcClass))
#define GST_IS_GC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GC))
#define GST_IS_GC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GC))

typedef struct _GstGc GstGc;
typedef struct _GstGcClass GstGcClass;


struct _GstGc {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height, numframes;
  
  bool      display;  
  int       debug;  

  IplImage* pImageRGBA ; // 4channel input

  IplImage* pImgRGB;     // 3channel version of the 4channel input

  IplImage* pImgChA;     // Alpha channel of the input
  IplImage* pImgCh1;
  IplImage* pImgCh2;
  IplImage* pImgCh3;
  IplImage* pImgChX;     // Alpha channel of the incoming input

  // GC stuff

  CvMat*     grabcut_mask; // mask created by graphcut

  struct grabcut_params GC;
  CvRect     facepos;
  
  float      growfactor; // grow multiplier to apply to input bbox

};

struct _GstGcClass {
  GstVideoFilterClass parent_class;
};

GType gst_gc_get_type(void);

G_END_DECLS

gboolean gst_gc_plugin_init(GstPlugin * plugin);

#endif /* __GST_GC_H__ */
