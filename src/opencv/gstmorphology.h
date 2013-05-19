/* GStreamer
 */

#ifndef __GST_MORPHOLOGY_H__
#define __GST_MORPHOLOGY_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_MORPHOLOGY \
	(gst_morphology_get_type())
#define GST_MORPHOLOGY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MORPHOLOGY,GstMorphology))
#define GST_MORPHOLOGY_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MORPHOLOGY,GstMorphologyClass))
#define GST_IS_MORPHOLOGY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MORPHOLOGY))
#define GST_IS_MORPHOLOGY_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MORPHOLOGY))

typedef struct _GstMorphology GstMorphology;
typedef struct _GstMorphologyClass GstMorphologyClass;

struct _GstMorphology {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvBGR;
  IplImage            *cvBGR_out;
  gint                 iterations; 
  gint                 op; 
};

struct _GstMorphologyClass {
  GstVideoFilterClass parent_class;
};

GType gst_morphology_get_type(void);

G_END_DECLS

gboolean gst_morphology_plugin_init(GstPlugin * plugin);

#endif /* __GST_MORPHOLOGY_H__ */
