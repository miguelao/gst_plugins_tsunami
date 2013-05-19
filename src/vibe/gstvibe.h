/* GStreamer
 */

#ifndef __GST_VIBE_H__
#define __GST_VIBE_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
#include "vibe.h"

G_BEGIN_DECLS

#define GST_TYPE_VIBE \
	(gst_vibe_get_type())
#define GST_VIBE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIBE,GstVibe))
#define GST_VIBE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIBE,GstVibeClass))
#define GST_IS_VIBE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIBE))
#define GST_IS_VIBE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIBE))

typedef struct _GstVibe GstVibe;
typedef struct _GstVibeClass GstVibeClass;

struct _GstVibe {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage            *cvBGRA;
  IplImage            *cvBGR;
  IplImage            *cvYUV, *cvYUVA;
  IplImage            *chA, *ch1, *ch2, *ch3;

  bool      display;  
  bool      norm;  

  int        vibe_nsamples;
  t_vibe*    pvibe;

  int nframes;
};

struct _GstVibeClass {
  GstVideoFilterClass parent_class;
};

GType gst_vibe_get_type(void);

G_END_DECLS

gboolean gst_vibe_plugin_init(GstPlugin * plugin);

#endif /* __GST_VIBE_H__ */
