/* GStreamer
 */

#ifndef __GST_PYRLK_H__
#define __GST_PYRLK_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>
#include "opencv_functions.h"

G_BEGIN_DECLS

#define GST_TYPE_PYRLK \
	(gst_pyrlk_get_type())
#define GST_PYRLK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PYRLK,GstPyrlk))
#define GST_PYRLK_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PYRLK,GstPyrlkClass))
#define GST_IS_PYRLK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PYRLK))
#define GST_IS_PYRLK_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PYRLK))

typedef struct _GstPyrlk GstPyrlk;
typedef struct _GstPyrlkClass GstPyrlkClass;


struct _GstPyrlk {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvRGB, *cvRGBprev, *cvRGBout;
  IplImage            *cvGrey;
  IplImage            *cvGreyPrev;
  IplImage            *cvEdgeImage;
  IplImage            *cvEdgeImage2;

  struct bbox         facepos;
  CvPoint             center;
};

struct _GstPyrlkClass {
  GstVideoFilterClass parent_class;
};

GType gst_pyrlk_get_type(void);

G_END_DECLS

gboolean gst_pyrlk_plugin_init(GstPlugin * plugin);

#endif /* __GST_PYRLK_H__ */
