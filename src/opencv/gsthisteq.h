/* GStreamer
 */

#ifndef __GST_HISTEQ_H__
#define __GST_HISTEQ_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_HISTEQ \
	(gst_histeq_get_type())
#define GST_HISTEQ(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_HISTEQ,GstHisteq))
#define GST_HISTEQ_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_HISTEQ,GstHisteqClass))
#define GST_IS_HISTEQ(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_HISTEQ))
#define GST_IS_HISTEQ_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_HISTEQ))

typedef struct _GstHisteq GstHisteq;
typedef struct _GstHisteqClass GstHisteqClass;

struct _GstHisteq {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvRGBA;
  IplImage            *cvYUV;
  IplImage            *cvRGB;

  IplImage* ch1;
  IplImage* ch2;
  IplImage* ch3;
  IplImage* chA;

  IplImage* im_y   ;
  IplImage* im_u   ;
  IplImage* im_v   ;
  IplImage* eq_im_y;
  IplImage* eq_im_u;
  IplImage* eq_im_v;

  gint method;

};

struct _GstHisteqClass {
  GstVideoFilterClass parent_class;
};

GType gst_histeq_get_type(void);

G_END_DECLS

gboolean gst_histeq_plugin_init(GstPlugin * plugin);

#endif /* __GST_HISTEQ_H__ */
