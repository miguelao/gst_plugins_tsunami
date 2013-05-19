/* GStreamer
 */

#ifndef __GST_SKIN_H__
#define __GST_SKIN_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_SKIN \
	(gst_skin_get_type())
#define GST_SKIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SKIN,GstSkin))
#define GST_SKIN_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SKIN,GstSkinClass))
#define GST_IS_SKIN(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SKIN))
#define GST_IS_SKIN_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SKIN))

typedef struct _GstSkin GstSkin;
typedef struct _GstSkinClass GstSkinClass;

struct _GstSkin {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvRGBA;
  IplImage            *cvRGB;
  float x,y,w;

  bool showH, showS, showV, enableskin, display;
  gint      method;

  IplImage* ch1;
  IplImage* ch2;
  IplImage* ch3;
  IplImage* chA;

};

struct _GstSkinClass {
  GstVideoFilterClass parent_class;
};

GType gst_skin_get_type(void);

G_END_DECLS

gboolean gst_skin_plugin_init(GstPlugin * plugin);

#endif /* __GST_SKIN_H__ */
