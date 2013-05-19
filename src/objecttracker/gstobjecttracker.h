/* GStreamer
 */

#ifndef __GST_OBJECTTRACKER_H__
#define __GST_OBJECTTRACKER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
#include "PredatorSrc/wrapper/predator.h"

G_BEGIN_DECLS

#define GST_TYPE_OBJECTTRACKER \
	(gst_objecttracker_get_type())
#define GST_OBJECTTRACKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OBJECTTRACKER,GstObjecttracker))
#define GST_OBJECTTRACKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OBJECTTRACKER,GstObjecttrackerClass))
#define GST_IS_OBJECTTRACKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OBJECTTRACKER))
#define GST_IS_OBJECTTRACKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OBJECTTRACKER))

typedef struct _GstObjecttracker GstObjecttracker;
typedef struct _GstObjecttrackerClass GstObjecttrackerClass;

struct _GstObjecttracker {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  IplImage *cvRGB;
  gboolean initDone, displayBB;
  Predator *predator;
  gint bb_width, bb_height, bb_x, bb_y;

  // Statistics
  gint nframes, objectCount;

  gchar* eventname; // by default is "objectlocation" but can be tuned to anything, ("facelocation" fi)
  gchar* eventresultname;  // By default is "objectfound" or "facefound" if set to something else
};

struct _GstObjecttrackerClass {
  GstVideoFilterClass parent_class;
};

GType gst_objecttracker_get_type(void);

G_END_DECLS

gboolean gst_objecttracker_plugin_init(GstPlugin * plugin);

#endif /* __GST_OBJECTTRACKER_H__ */
