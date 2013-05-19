/* GStreamer
 */

#ifndef __GST_SNAKES_H__
#define __GST_SNAKES_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_SNAKES \
	(gst_snakes_get_type())
#define GST_SNAKES(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SNAKES,GstSnakes))
#define GST_SNAKES_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SNAKES,GstSnakesClass))
#define GST_IS_SNAKES(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SNAKES))
#define GST_IS_SNAKES_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SNAKES))

typedef struct _GstSnakes GstSnakes;
typedef struct _GstSnakesClass GstSnakesClass;

struct _GstSnakes {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvRGBA;
  IplImage            *cvRGB;
  IplImage            *cvGRAY;
  float x,y,w;

  bool showH, showS, showV, display;
  gint      method;

  IplImage* ch1;
  IplImage* ch2;
  IplImage* ch3;
  IplImage* chA;

  // snake algo parameters
  CvPoint*       points;  // array of snake starting points 
  int            length;  // amount of points per frame side, i.e. above, below, left, right
                          // Array points would have 4*length points
  float          alpha;
  float          beta;
  float          gamma;
  CvTermCriteria criteria;
  int            calcGradient;
  CvSize         size;

  int            nframe;
};

struct _GstSnakesClass {
  GstVideoFilterClass parent_class;
};

GType gst_snakes_get_type(void);

G_END_DECLS

gboolean gst_snakes_plugin_init(GstPlugin * plugin);

#endif /* __GST_SNAKES_H__ */
