/* GStreamer
 */

#ifndef __GST_BLOCKANALYSIS_H__
#define __GST_BLOCKANALYSIS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

//#include "../glsl/ShmImageServer.h"
//#include "../bgmotiondetection/bgmd_main.h"

G_BEGIN_DECLS

#define GST_TYPE_BLOCKANALYSIS \
	(gst_blockanalysis_get_type())
#define GST_BLOCKANALYSIS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BLOCKANALYSIS,GstBlockanalysis))
#define GST_BLOCKANALYSIS_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BLOCKANALYSIS,GstBlockanalysisClass))
#define GST_IS_BLOCKANALYSIS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BLOCKANALYSIS))
#define GST_IS_BLOCKANALYSIS_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BLOCKANALYSIS))

typedef struct _GstBlockanalysis GstBlockanalysis;
typedef struct _GstBlockanalysisClass GstBlockanalysisClass;

struct _GstBlockanalysis {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvYUV;
  IplImage            *cvRGB;
  IplImage 			  *cvGRAY;
  IplImage			  *cvSobel_x;
  IplImage			  *cvSobel_y;
  IplImage			  *cvSobelSc;
  CvMat 			  *cvMatDx;
  CvMat 			  *cvMatDy;

  float res;

  //ShmImageServer ShmIS;

  //bgMotionDetection *test;
};

struct _GstBlockanalysisClass {
  GstVideoFilterClass parent_class;
};

GType gst_blockanalysis_get_type(void);

G_END_DECLS

gboolean gst_blockanalysis_plugin_init(GstPlugin * plugin);

#endif /* __GST_BLOCKANALYSIS_H__ */
