/* GStreamer
 */

#ifndef __GST_CONTOURS_H__
#define __GST_CONTOURS_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_CONTOURS \
	(gst_contours_get_type())
#define GST_CONTOURS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CONTOURS,GstContours))
#define GST_CONTOURS_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CONTOURS,GstContoursClass))
#define GST_IS_CONTOURS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CONTOURS))
#define GST_IS_CONTOURS_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CONTOURS))

typedef struct _GstContours GstContours;
typedef struct _GstContoursClass GstContoursClass;

struct _GstContours {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvGRAY;
  IplImage            *cvGRAY_copy;
  gboolean             display; 
  gboolean             histeq; 

  CvMemStorage        *storage; 
  CvSeq               *contour;
  int                  mode;
  int                  minarea;
};

struct _GstContoursClass {
  GstVideoFilterClass parent_class;
};

GType gst_contours_get_type(void);

G_END_DECLS

gboolean gst_contours_plugin_init(GstPlugin * plugin);

#endif /* __GST_CONTOURS_H__ */
