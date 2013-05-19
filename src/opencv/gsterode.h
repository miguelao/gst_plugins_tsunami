/* GStreamer
 */

#ifndef __GST_ERODE_H__
#define __GST_ERODE_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_ERODE \
	(gst_erode_get_type())
#define GST_ERODE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ERODE,GstErode))
#define GST_ERODE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_ERODE,GstErodeClass))
#define GST_IS_ERODE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ERODE))
#define GST_IS_ERODE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_ERODE))

typedef struct _GstErode GstErode;
typedef struct _GstErodeClass GstErodeClass;

struct _GstErode {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvBGR;
  IplImage            *cvBGR_out;
  gboolean             iterations; 
};

struct _GstErodeClass {
  GstVideoFilterClass parent_class;
};

GType gst_erode_get_type(void);

G_END_DECLS

gboolean gst_erode_plugin_init(GstPlugin * plugin);

#endif /* __GST_ERODE_H__ */
