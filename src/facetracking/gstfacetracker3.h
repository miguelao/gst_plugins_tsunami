/* GStreamer
 */

#ifndef __GST_FACETRACKER3_H__
#define __GST_FACETRACKER3_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include "haarwrapper/haarwrapper.h"
#include "kalmantracking.h"

G_BEGIN_DECLS

#define GST_TYPE_FACETRACKER3 \
	(gst_facetracker3_get_type())
#define GST_FACETRACKER3(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FACETRACKER3,GstFacetracker3))
#define GST_FACETRACKER3_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FACETRACKER3,GstFacetracker3Class))
#define GST_IS_FACETRACKER3(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FACETRACKER3))
#define GST_IS_FACETRACKER3_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FACETRACKER3))

typedef struct _GstFacetracker3 GstFacetracker3;
typedef struct _GstFacetracker3Class GstFacetracker3Class;

struct _GstFacetracker3 {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  
  gint setfps;
  guint64 setfps_delay;
  guint64 setfps_time;
  
  gboolean display;
  gboolean tracker;
  long nframes, nframes_with_face_detected;
  
  gchar *profile;
  gchar *profile2;
  gchar *profile3;
  gdouble scale_factor;
  gint min_neighbors;
  gint min_size_width;
  gint min_size_height;
  
  t_haarwrapper_image *image_rgb;
  t_haarwrapper_image *image_rgb2;
  IplImage            *cvBGR_input;
  IplImage            *cvBGR;

  t_haarwrapper       *hc;
  t_haarwrapper       *hc2;
  t_haarwrapper       *hc3;

  struct bbox_int   *face, *torso, *side;
  struct kernel_internal_state *facek, *torsok, *sidek;

  guint32 timer;  
  int faceAppearanceCnt;
  int binsInitialized;
  
  //t_statslog* statslog;
};

struct _GstFacetracker3Class {
  GstVideoFilterClass parent_class;
};

GType gst_facetracker3_get_type(void);

G_END_DECLS

gboolean gst_facetracker3_plugin_init(GstPlugin * plugin);

#endif /* __GST_FACETRACKER3_H__ */
