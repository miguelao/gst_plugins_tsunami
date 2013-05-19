/* GStreamer
 */

#ifndef __GST_DILATE_H__
#define __GST_DILATE_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_DILATE \
	(gst_dilate_get_type())
#define GST_DILATE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DILATE,GstDilate))
#define GST_DILATE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DILATE,GstDilateClass))
#define GST_IS_DILATE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DILATE))
#define GST_IS_DILATE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DILATE))

typedef struct _GstDilate GstDilate;
typedef struct _GstDilateClass GstDilateClass;

struct _GstDilate {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvBGR;
  IplImage            *cvBGR_out;
  gboolean             iterations; 
};

struct _GstDilateClass {
  GstVideoFilterClass parent_class;
};

GType gst_dilate_get_type(void);

G_END_DECLS

gboolean gst_dilate_plugin_init(GstPlugin * plugin);

#endif /* __GST_DILATE_H__ */
