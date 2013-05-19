/* GStreamer
 */

#ifndef __GST_FACETRACKER_H__
#define __GST_FACETRACKER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include "defines.h"
#include "face.h"
#include "haarclass.h"
#include "helper.h"
#include "camshift.h"
#include "ftrack.h"

#ifdef USE_IPP
//#include "facedetection.h"
#endif

G_BEGIN_DECLS

#define GST_TYPE_FACETRACKER \
	(gst_facetracker_get_type())
#define GST_FACETRACKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FACETRACKER,GstFacetracker))
#define GST_FACETRACKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FACETRACKER,GstFacetrackerClass))
#define GST_IS_FACETRACKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FACETRACKER))
#define GST_IS_FACETRACKER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FACETRACKER))

typedef struct _GstFacetracker GstFacetracker;
typedef struct _GstFacetrackerClass GstFacetrackerClass;

struct _GstFacetracker {
	GstVideoFilter parent;

	GStaticMutex lock;

	GstVideoFormat in_format, out_format;
	gint width, height;

	gint setfps;
	guint64 setfps_delay;
	guint64 setfps_time;

	gboolean display;

	gchar *profile;
	gdouble scale_factor;
	gint min_neighbors;
	gint min_size_width;
	gint min_size_height;

	#if (FACETRK_FORMAT == FACETRK_FORMAT_YUV) && defined(USE_IPP) && defined(FACETRACKER_CAMSHIFT)
	IplImage *cvBGR;
	IplImage *cvYUV;
	t_image *imageCpy;
	bool stopTracking;
	// smoothing filter
	ftrack ftrk;
	//cam shift color tracking buffers
	camshift_kalman_tracker camKalTrk;
	bool firstTime;
	#endif

	#ifndef USE_IPP
	IplImage *cvBGR;
	t_image *imageBGR;
	#endif

	#if (FACETRK_FORMAT == FACETRK_FORMAT_RGBA) || !defined(USE_IPP)
	IplImage *cvYUV;
	#endif

	#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
	IplImage *img;
	#endif

	#if (FACETRK_FORMAT == FACETRK_FORMAT_YUV) && !defined(USE_IPP)
	t_image *imageCpy;
	#endif

	t_haarclass *hc;
	t_face *face;
	t_image *image;
	unsigned int timer;

	int faceAppearanceCnt;
	int binsInitialized;

	t_statslog* statslog;

	long frameCount, faceCount;

  bool debug;
  bool showskin;
  bool enableskin;
  gint last_known_face_size;
  gint last_known_face_x;
  gint last_known_face_y;
  gint frame_last_known_face;

  // to keep the HSV thresholding between frames.
  gint h_thr_low, h_thr_high, s_thr_low, s_thr_high, v_thr_low, v_thr_high;
};

struct _GstFacetrackerClass {
	GstVideoFilterClass parent_class;
};

GType gst_facetracker_get_type(void);

G_END_DECLS

gboolean gst_facetracker_plugin_init(GstPlugin * plugin);

#endif /* __GST_FACETRACKER_H__ */
