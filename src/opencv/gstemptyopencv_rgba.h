/* GStreamer
 */

#ifndef __GST_EMPTYOPENCV_RGBA_H__
#define __GST_EMPTYOPENCV_RGBA_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_EMPTYOPENCV_RGBA \
	(gst_emptyopencv_rgba_get_type())
#define GST_EMPTYOPENCV_RGBA(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_EMPTYOPENCV_RGBA,GstEmptyopencv_Rgba))
#define GST_EMPTYOPENCV_RGBA_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_EMPTYOPENCV_RGBA,GstEmptyopencv_RgbaClass))
#define GST_IS_EMPTYOPENCV_RGBA(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_EMPTYOPENCV_RGBA))
#define GST_IS_EMPTYOPENCV_RGBA_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_EMPTYOPENCV_RGBA))

typedef struct _GstEmptyopencv_Rgba GstEmptyopencv_Rgba;
typedef struct _GstEmptyopencv_RgbaClass GstEmptyopencv_RgbaClass;

struct _GstEmptyopencv_Rgba {
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

struct _GstEmptyopencv_RgbaClass {
  GstVideoFilterClass parent_class;
};

GType gst_emptyopencv_rgba_get_type(void);

G_END_DECLS

gboolean gst_emptyopencv_rgba_plugin_init(GstPlugin * plugin);

#endif /* __GST_EMPTYOPENCV_RGBA_H__ */
