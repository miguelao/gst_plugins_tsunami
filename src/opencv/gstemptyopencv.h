/* GStreamer
 */

#ifndef __GST_EMPTYOPENCV_H__
#define __GST_EMPTYOPENCV_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_EMPTYOPENCV \
	(gst_emptyopencv_get_type())
#define GST_EMPTYOPENCV(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EMPTYOPENCV,GstEmptyopencv))
#define GST_EMPTYOPENCV_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EMPTYOPENCV,GstEmptyopencvClass))
#define GST_IS_EMPTYOPENCV(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EMPTYOPENCV))
#define GST_IS_EMPTYOPENCV_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EMPTYOPENCV))

typedef struct _GstEmptyopencv GstEmptyopencv;
typedef struct _GstEmptyopencvClass GstEmptyopencvClass;

struct _GstEmptyopencv {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvRGB;
};

struct _GstEmptyopencvClass {
  GstVideoFilterClass parent_class;
};

GType gst_emptyopencv_get_type(void);

G_END_DECLS

gboolean gst_emptyopencv_plugin_init(GstPlugin * plugin);

#endif /* __GST_EMPTYOPENCV_H__ */
