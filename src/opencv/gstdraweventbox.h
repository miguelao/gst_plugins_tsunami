/* GStreamer
 */

#ifndef __GST_DRAWEVENTBOX_H__
#define __GST_DRAWEVENTBOX_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_DRAWEVENTBOX \
	(gst_draweventbox_get_type())
#define GST_DRAWEVENTBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DRAWEVENTBOX,GstDraweventbox))
#define GST_DRAWEVENTBOX_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DRAWEVENTBOX,GstDraweventboxClass))
#define GST_IS_DRAWEVENTBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DRAWEVENTBOX))
#define GST_IS_DRAWEVENTBOX_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DRAWEVENTBOX))

typedef struct _GstDraweventbox GstDraweventbox;
typedef struct _GstDraweventboxClass GstDraweventboxClass;

struct _GstDraweventbox {
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

  CvRect    bboxpos;
  gboolean  bboxvalid;
};

struct _GstDraweventboxClass {
  GstVideoFilterClass parent_class;
};

GType gst_draweventbox_get_type(void);

G_END_DECLS

gboolean gst_draweventbox_plugin_init(GstPlugin * plugin);

#endif /* __GST_DRAWEVENTBOX_H__ */
