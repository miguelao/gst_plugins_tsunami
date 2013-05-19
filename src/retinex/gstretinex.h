/* GStreamer
 */

#ifndef __GST_RETINEX_H__
#define __GST_RETINEX_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_RETINEX \
	(gst_retinex_get_type())
#define GST_RETINEX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_RETINEX,GstRetinex))
#define GST_RETINEX_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_RETINEX,GstRetinexClass))
#define GST_IS_RETINEX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_RETINEX))
#define GST_IS_RETINEX_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_RETINEX))

typedef struct _GstRetinex GstRetinex;
typedef struct _GstRetinexClass GstRetinexClass;

struct _GstRetinex {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  bool      display;  

  IplImage* pFrame ; // 4channel input
  IplImage* pFrame2; // 3channel version of the 4channel input
  IplImage* pFrameA; // Alpha channel of the input
  IplImage* ch1;
  IplImage* ch2;
  IplImage* ch3;

};

struct _GstRetinexClass {
  GstVideoFilterClass parent_class;
};

GType gst_retinex_get_type(void);

G_END_DECLS

gboolean gst_retinex_plugin_init(GstPlugin * plugin);

#endif /* __GST_RETINEX_H__ */
