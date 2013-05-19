/* GStreamer
 */
/**
 * SECTION:element- facetracker
 *
 * This element extracts the person from its background and composes
 * an alpha channel which is added to the output. For the mgmt of the
 * alpha channel, gstalpha plugin is modified from the "good' plugins.
 *
 *
 * The alpha element adds an alpha channel to a video stream.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstfacetracker.h"
#include "face.h"
#include "facedetection.h"
#include "drawing.h"
#include <gst/controller/gstcontroller.h>


GST_DEBUG_CATEGORY_STATIC (gst_facetracker_debug);
#define GST_CAT_DEFAULT gst_facetracker_debug

#ifdef USE_IPP
#define DEFAULT_PROFILE "haar.txt"
#else
#define DEFAULT_PROFILE "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
#endif
#define DEFAULT_SCALE_FACTOR 1.2
#define DEFAULT_MIN_NEIGHBORS 3
#define DEFAULT_MIN_SIZE_WIDTH 0
#define DEFAULT_MIN_SIZE_HEIGHT 0
#define DEFAULT_FPS G_MAXINT

#define FACETRACKER_TIMEOUT 200000000



enum {
	PROP_0,
	PROP_FPS,
	PROP_STATS,
	PROP_SETFPS,
	PROP_DISPLAY,
	PROP_PROFILE,
	PROP_SCALE_FACTOR,
	PROP_MIN_NEIGHBORS,
	PROP_MIN_SIZE_WIDTH,
	PROP_MIN_SIZE_HEIGHT,
	PROP_DEBUG,
	PROP_SHOWSKIN,
  PROP_ENABLESKIN,
	PROP_LAST
};

static GstStaticPadTemplate gst_facetracker_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (FACETRK_GST_VIDEO_CAPS)
);
static GstStaticPadTemplate gst_facetracker_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (FACETRK_GST_VIDEO_CAPS)
);

#define GST_FACETRACKER_LOCK(facetracker) G_STMT_START { \
	GST_LOG_OBJECT (facetracker, "Locking facetracker from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&facetracker->lock); \
	GST_LOG_OBJECT (facetracker, "Locked facetracker from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_FACETRACKER_UNLOCK(facetracker) G_STMT_START { \
	GST_LOG_OBJECT (facetracker, "Unlocking facetracker from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&facetracker->lock); \
} G_STMT_END

static gboolean gst_facetracker_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static GstCaps *gst_facetracker_transform_caps(GstBaseTransform * btrans, GstPadDirection direction, GstCaps * caps);
static gboolean gst_facetracker_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_facetracker_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_facetracker_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_facetracker_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_facetracker_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_facetracker_finalize(GObject * object);

gint gstfacetracker_send_event_downstream(struct _GstFacetracker *facetracker, GstBaseTransform * btrans, bool has_haarface);
gint gstfacetracker_send_bus_event(struct _GstFacetracker *facetracker, GstBaseTransform * btrans, 
                                   bool has_haarface, GstBuffer * buf);
gint gstfacetracker_printinfo_n_display(struct _GstFacetracker *facetracker, GstBaseTransform * btrans, bool has_haarface);
gint gstfacetracker_find_skin_center_of_mass(struct _GstFacetracker *facetracker, float *x, float *y, gint display,
                                             float seed_x, float seed_y, float seed_r, bool facefound);

GST_BOILERPLATE (GstFacetracker, gst_facetracker, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanFacetracker(GstFacetracker *facetracker) {
	if (facetracker->face) {
		face_destroy(facetracker->face, true, true);
		facetracker->face = NULL;
	}
	if (facetracker->image) {
		destroy_image(facetracker->image);
		facetracker->image = NULL;
	}
#if (FACETRK_FORMAT == FACETRK_FORMAT_RGBA) || (FACETRK_FORMAT == FACETRK_FORMAT_YUVA && !defined(USE_IPP))
	if (facetracker->cvYUV) {
		cvReleaseImage(&facetracker->cvYUV);
	}
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_YUV && !defined(USE_IPP)
	if (facetracker->cvYUV) {
		cvReleaseImageHeader(&facetracker->cvYUV);
	}
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
	if (facetracker->img) {
		cvReleaseImageHeader(&facetracker->img);
	}
#endif
#if (FACETRK_FORMAT == FACETRK_FORMAT_YUV) && !defined(USE_IPP)
	if (facetracker->imageCpy) {
		free_image(facetracker->imageCpy);
		destroy_image(facetracker->imageCpy);
		facetracker->imageCpy = NULL;
	}
#endif
	if (facetracker->hc) {
		haarclass_destroy(facetracker->hc);
	}

	printf("Face detection rate: total:%ld found:%ld(%.2f%%)\n",facetracker->frameCount,facetracker->faceCount,(facetracker->faceCount*100.0)/facetracker->frameCount);
}

static void gst_facetracker_base_init(gpointer g_class) {
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_set_details_simple(element_class, "Facetracker filter", "Filter/Effect/Video",
#if USE_IPP
			"Performs face detection and tracking using Intel IPP, sends faceinfo via custom events",
#else //!USE_IPP
			"Performs face detection and tracking using OpenCV, sends faceinfo via custom events",
#endif //USE_IPP
			"Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>, "
				"Mukul Dhankhar <mukul.dhankhar@alcatel-lucent.com>");
        
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_facetracker_sink_template));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_facetracker_src_template));

	GST_DEBUG_CATEGORY_INIT (gst_facetracker_debug, "facetracker", 0, "facetracker - Performs face detection and tracking on input images");
}

static void gst_facetracker_class_init(GstFacetrackerClass * klass) {
	GObjectClass *gobject_class = (GObjectClass *)klass;
	GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;

	gobject_class->set_property = gst_facetracker_set_property;
	gobject_class->get_property = gst_facetracker_get_property;
	gobject_class->finalize = gst_facetracker_finalize;

	g_object_class_install_property (gobject_class, PROP_FPS, g_param_spec_boolean ("fps", "FPS", "Show frames/second",
			FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );

	g_object_class_install_property(gobject_class, PROP_STATS,
			g_param_spec_string("stats", "statslog", "statistical info",
			"",	(GParamFlags)(G_PARAM_READABLE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_SETFPS, g_param_spec_int("setfps", "SETFPS", "set the maximum frames/second", 1,
			G_MAXINT, DEFAULT_FPS, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_DISPLAY, g_param_spec_boolean("display", "Display",
			"Sets whether the detected faces should be highlighted in the output", TRUE, (GParamFlags)(G_PARAM_READWRITE
					| G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_PROFILE, g_param_spec_string("profile", "Profile",
			"Location of Haar cascade file to use for face detection", DEFAULT_PROFILE, (GParamFlags)(G_PARAM_READWRITE
					| G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_SCALE_FACTOR, g_param_spec_double("scale-factor", "Scale factor",
			"Factor by which the windows is scaled after each scan", 1.1, 10.0, DEFAULT_SCALE_FACTOR, (GParamFlags)(G_PARAM_READWRITE
					| G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_MIN_NEIGHBORS, g_param_spec_int("min-neighbors", "Mininum neighbors",
			"Minimum number (minus 1) of neighbor rectangles that makes up "
				"an object", 0, G_MAXINT, DEFAULT_MIN_NEIGHBORS, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_MIN_SIZE_WIDTH, g_param_spec_int("min-size-width", "Minimum size width",
			"Minimum window width size", 0, G_MAXINT, DEFAULT_MIN_SIZE_WIDTH, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property(gobject_class, PROP_MIN_SIZE_HEIGHT, g_param_spec_int("min-size-height", "Minimum size height",
			"Minimum window height size", 0, G_MAXINT, DEFAULT_MIN_SIZE_HEIGHT, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	g_object_class_install_property (gobject_class, PROP_DEBUG, 
                                         g_param_spec_boolean ("debug", "debug", "Show info on frames received",
                                                               FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
	g_object_class_install_property(gobject_class, PROP_SHOWSKIN, g_param_spec_boolean("showskin", "ShowSkin",
			"If selected shows the skin colour patches & gravity center", TRUE, (GParamFlags)(G_PARAM_READWRITE
					| G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class, PROP_ENABLESKIN, g_param_spec_boolean("enableskin", "EnableSkin",
            "If set to true use skin colour patches & gravity center to assist face tracker", TRUE, (GParamFlags)(G_PARAM_READWRITE
                    | G_PARAM_STATIC_STRINGS)));


	btrans_class->passthrough_on_same_caps = TRUE;
	//btrans_class->always_in_place = TRUE;
	btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_facetracker_transform_ip);
	btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_facetracker_before_transform);
	btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_facetracker_get_unit_size);
	btrans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_facetracker_transform_caps);
	btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_facetracker_set_caps);
}

static void gst_facetracker_init(GstFacetracker * facetracker, GstFacetrackerClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)facetracker, TRUE);
  g_static_mutex_init(&facetracker->lock);
  facetracker->setfps = DEFAULT_FPS;
  facetracker->setfps_delay = 0;
  facetracker->profile = g_strdup(DEFAULT_PROFILE);
  facetracker->display = 0;
  facetracker->scale_factor = DEFAULT_SCALE_FACTOR;
  facetracker->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  facetracker->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  facetracker->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;
#if (FACETRK_FORMAT == FACETRK_FORMAT_RGBA) || !defined(USE_IPP)
  facetracker->cvYUV = NULL;
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
  facetracker->img = NULL;
#endif
#if (FACETRK_FORMAT == FACETRK_FORMAT_YUV) && !defined(USE_IPP)
  facetracker->imageCpy = NULL;
#endif
  facetracker->image = NULL;
  facetracker->timer = 0;
  facetracker->statslog = NULL;
}

static void gst_facetracker_finalize(GObject * object) {
	GstFacetracker *facetracker = GST_FACETRACKER (object);

	GST_FACETRACKER_LOCK (facetracker);
	CleanFacetracker(facetracker);
	g_free(facetracker->profile);
	statslog_destroy(facetracker->statslog);
	GST_FACETRACKER_UNLOCK (facetracker);
	GST_INFO("Facetracker destroyed (%s).", GST_OBJECT_NAME(object));

	g_static_mutex_free(&facetracker->lock);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_facetracker_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstFacetracker *facetracker = GST_FACETRACKER (object);
  
  GST_FACETRACKER_LOCK (facetracker);
  switch (prop_id) {
  case PROP_FPS:
    statslog_destroy(facetracker->statslog);
    if (g_value_get_boolean(value))
      facetracker->statslog = statslog_create("Facetracker");
    break;
  case PROP_STATS:
    // read only
    break;
  case PROP_SETFPS:
    facetracker->setfps = g_value_get_int(value);
    if (facetracker->setfps > 500) {
      facetracker->setfps_delay = 0;
    } else {
      facetracker->setfps_delay = 1000000ull / facetracker->setfps;
      facetracker->setfps_time = getTime() + facetracker->setfps_delay;
    }
    break;
  case PROP_PROFILE:
    g_free(facetracker->profile);
    facetracker->profile = g_value_dup_string(value);
    break;
  case PROP_DISPLAY:
    facetracker->display = g_value_get_boolean(value);
    break;
  case PROP_SCALE_FACTOR:
    facetracker->scale_factor = g_value_get_double(value);
    break;
  case PROP_MIN_NEIGHBORS:
    facetracker->min_neighbors = g_value_get_int(value);
    break;
  case PROP_MIN_SIZE_WIDTH:
    facetracker->min_size_width = g_value_get_int(value);
    break;
  case PROP_MIN_SIZE_HEIGHT:
    facetracker->min_size_height = g_value_get_int(value);
    break;
  case PROP_DEBUG:
    facetracker->debug = g_value_get_boolean(value);
    break;
  case PROP_SHOWSKIN:
    facetracker->showskin = g_value_get_boolean(value);
    break;
  case PROP_ENABLESKIN:
    facetracker->enableskin = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_FACETRACKER_UNLOCK (facetracker);
}

static void gst_facetracker_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstFacetracker *facetracker = GST_FACETRACKER (object);
  
  switch (prop_id) {
  case PROP_FPS:
    g_value_set_boolean(value, facetracker->statslog != NULL);
    break;
  case PROP_STATS:
    if (facetracker->statslog) {
      char buffer[1024];
      statslog_print_frame_time(facetracker->statslog, buffer);
      g_value_set_string(value, buffer);
    } else {
      g_value_set_string(value, "NA");
    }
    break;
  case PROP_SETFPS:
    g_value_set_int(value, facetracker->setfps);
    break;
  case PROP_PROFILE:
    g_value_set_string(value, facetracker->profile);
    break;
  case PROP_DISPLAY:
    g_value_set_boolean(value, facetracker->display);
    break;
  case PROP_SCALE_FACTOR:
    g_value_set_float(value, facetracker->scale_factor);
    break;
  case PROP_MIN_NEIGHBORS:
    g_value_set_int(value, facetracker->min_neighbors);
    break;
  case PROP_MIN_SIZE_WIDTH:
    g_value_set_int(value, facetracker->min_size_width);
    break;
  case PROP_MIN_SIZE_HEIGHT:
    g_value_set_int(value, facetracker->min_size_height);
    break;
  case PROP_DEBUG:
    g_value_set_boolean(value, facetracker->debug);
    break;
  case PROP_SHOWSKIN:
    g_value_set_boolean(value, facetracker->showskin);
    break;
  case PROP_ENABLESKIN:
    g_value_set_boolean(value, facetracker->enableskin);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_facetracker_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}

static GstCaps *
gst_facetracker_transform_caps(GstBaseTransform * btrans, GstPadDirection direction, GstCaps * caps) {
	GstFacetracker *facetracker = GST_FACETRACKER (btrans);
	GstCaps *ret, *tmp, *tmplt;
	GstStructure *structure;
	gint i;

	tmp = gst_caps_new_empty();

	GST_FACETRACKER_LOCK (facetracker);

	for (i = 0; i < (int)gst_caps_get_size(caps); i++) {
		structure = gst_structure_copy(gst_caps_get_structure(caps, i));
		gst_structure_remove_fields(structure, "format", "endianness", "depth", "bpp", "red_mask", "green_mask", "blue_mask", "alpha_mask",
				"palette_data", "facetracker_mask", "color-matrix", "chroma-site", NULL);
		#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
		gst_structure_set_name(structure, "video/x-raw-rgb");
		#endif
		#if (FACETRK_FORMAT == FACETRK_FORMAT_YUVA) || (FACETRK_FORMAT == FACETRK_FORMAT_YUV)
		gst_structure_set_name(structure, "video/x-raw-yuv");
		#endif
		gst_caps_append_structure(tmp, gst_structure_copy(structure));
		gst_structure_free(structure);
	}

	if (direction == GST_PAD_SINK) {
		tmplt = gst_static_pad_template_get_caps(&gst_facetracker_src_template);
		ret = gst_caps_intersect(tmp, tmplt);
		gst_caps_unref(tmp);
		gst_caps_unref(tmplt);
		tmp = NULL;
	}
	else {
		ret = tmp;
		tmp = NULL;
	}

	GST_DEBUG("Transformed %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, caps, ret);

	GST_FACETRACKER_UNLOCK (facetracker);

	return ret;
}

static gboolean gst_facetracker_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstFacetracker *facetracker = GST_FACETRACKER (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_FACETRACKER_LOCK (facetracker);
  
  gst_video_format_parse_caps(incaps, &facetracker->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &facetracker->out_format, &out_width, &out_height);
  if (!(facetracker->in_format == facetracker->out_format) || !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_FACETRACKER_UNLOCK (facetracker);
    return FALSE;
  }
  
  facetracker->width = in_width;
  facetracker->height = in_height;
  
  // init cutout images
  GST_INFO("Initialising Facetracker...");
  CleanFacetracker(facetracker);
  facetracker->hc = haarclass_create(facetracker->profile);
  
  //const CvSize size = cvSize(facetracker->width, facetracker->height);
#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_YUVA
#endif
  
#if FACETRK_FORMAT == FACETRK_FORMAT_YUV
  facetracker->image = create_image(COLOR_SPACE_YUV, IMAGE_DATA_FORMAT_PACKED);
  resize_image(facetracker->image, facetracker->width, facetracker->height);
#endif
  
  facetracker->timer = 0;
  facetracker->setfps_time = getTime() + facetracker->setfps_delay;
  GST_INFO("Facetracker initialized.");
  
  facetracker->faceAppearanceCnt = 0;
  facetracker->binsInitialized = 0;
  facetracker->frameCount = 0;
  facetracker->faceCount = 0;
  facetracker->debug = false;
  
  facetracker->frame_last_known_face = 0;
  
  facetracker->h_thr_low   = 10;
  facetracker->h_thr_high  = 20;
  facetracker->s_thr_low   = 48;
  facetracker->s_thr_high  = 360;
  facetracker->v_thr_low   = 80;
  facetracker->v_thr_high  = 256;
  
  GST_FACETRACKER_UNLOCK (facetracker);
  
  return TRUE;
}

static void gst_facetracker_before_transform(GstBaseTransform * btrans, GstBuffer * buf) {
	GstFacetracker *facetracker = GST_FACETRACKER (btrans);
	GstClockTime timestamp;

	timestamp = gst_segment_to_stream_time(&btrans->segment, GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (buf));
	GST_INFO("Got stream time of %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));
	if (GST_CLOCK_TIME_IS_VALID (timestamp))
		gst_object_sync_values(G_OBJECT(facetracker), timestamp);
}

static GstFlowReturn gst_facetracker_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstFacetracker *facetracker = GST_FACETRACKER (btrans);
  bool has_haarface;
  uint64_t now_time;
  facetracker->frameCount++;

  if(facetracker->debug){
    if( 0==(facetracker->frameCount % 100)){
      g_print("[%s] facetracker frame in (%ld)\n", gst_element_get_name(facetracker),facetracker->frameCount);
    }
  }

  // record statistical info
  if (facetracker->statslog) {
    statslog_frame_start(facetracker->statslog);
    now_time = statslog_get_frame_start_time(facetracker->statslog);
  } else {
    now_time = getTime();
  }

  GST_FACETRACKER_LOCK (facetracker);

  if (facetracker->setfps_time < now_time) {
    facetracker->setfps_time = now_time + facetracker->setfps_delay;

#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
    facetracker->img->imageData = (char*)GST_BUFFER_DATA(gstbuf);
    cvCvtColor(facetracker->img, facetracker->cvYUV, CV_RGB2YUV);
#endif

#if (FACETRK_FORMAT == FACETRK_FORMAT_YUVA) || (FACETRK_FORMAT == FACETRK_FORMAT_YUV)
    setdata_image(facetracker->image, (unsigned char*)GST_BUFFER_DATA(gstbuf));
    if (facetracker->face == NULL)
      facetracker->face = face_create(facetracker->image);
#endif

    ////////////////////////////////////////////////////////////////////////////
    // geometry prediction /////////////////////////////////////////////////////
    face_geometry_predict(facetracker->face);

    //face_y_normalization(facetracker->image->data[0], facetracker->width, facetracker->height, 3, 200);
    has_haarface = face_geometry_update_haar(facetracker->face, facetracker->image, facetracker->hc, NULL);

    if( has_haarface ){
      facetracker->faceCount++;
      facetracker->frame_last_known_face =  facetracker->frameCount;                                     
    }

    ///////////// SKIN COLOUR BLOB FACE DETECTION/////////////////////////////////
    ///////////// we correct horizontally the face detection /////////////////////
    //////////////////////////////////////////////////////////////////////////////
    if(facetracker->enableskin)                                                 //
    {                                                                           //
      float x,y;                                                                //
      //printf("still havent seen a face(frame %ld)\n",facetracker->frameCount);//
      if( facetracker->frameCount >= 5 ){                                       //
        int display = facetracker->showskin;                                    //
        gint facefound =
        gstfacetracker_find_skin_center_of_mass( facetracker, &x, &y, display,
                                                 facetracker->face->geometry->mean[0],
                                                 facetracker->face->geometry->mean[1],
                                                 facetracker->face->geometry->mean[2],
                                                 has_haarface);
        if( (x>1.0) && (x<320.0)){                                              //
          // we got a skin correction: use it                                   //
          facetracker->face->geometry->mean[0] = x;                             //
          if( (facetracker->frameCount > 0) && (facetracker->faceCount == 0) ){ //
            // if we are in the first frames and no face detected, just go skin //
            facetracker->face->geometry->mean[1] = y;                           //
          }                                                                     //
        }                                                                       //
        // if not facefound (no skin colour under face bbox), just revert face bbox
        if( (facefound==0) || (facetracker->face->geometry->mean[2]<=20.1) ){     //
          if( facetracker->last_known_face_x > 1.0)
            facetracker->face->geometry->mean[0] = facetracker->last_known_face_x;
          if( facetracker->last_known_face_y > 1.0)
            facetracker->face->geometry->mean[1] = facetracker->last_known_face_y;
          if( facetracker->last_known_face_size > 1.0)
            facetracker->face->geometry->mean[2] = facetracker->last_known_face_size;
        }
#undef  BOOTSTRAPPING
#ifdef  BOOTSTRAPPING
        // EXCEPT if we have seen no face for a long time then bootstrap stuff
        if( (facetracker->frameCount - facetracker->frame_last_known_face)>50){
          printf("bootstrapping face with skin, new pos (%f,%f)(%f)\n",x,y, facetracker->face->geometry->mean[2]);
          facetracker->face->geometry->mean[0] = x;  
          facetracker->face->geometry->mean[1] = y;
          facetracker->frame_last_known_face = facetracker->frameCount;
        }
#endif//  BOOTSTRAPPING
        // keep actual face location for next frame.
        facetracker->last_known_face_size = facetracker->face->geometry->mean[2];
        facetracker->last_known_face_x    = facetracker->face->geometry->mean[0];
        facetracker->last_known_face_y    = facetracker->face->geometry->mean[1];
      }                                                                       //
      if(facetracker->showskin){                                              //
        draw_box_outline(facetracker->image,                                  //
                         (int)facetracker->face->geometry->mean[0] - facetracker->face->geometry->mean[2],       //
                         (int)facetracker->face->geometry->mean[1] - facetracker->face->geometry->mean[2],       //
                         (int)facetracker->face->geometry->mean[0] + facetracker->face->geometry->mean[2],       //
                         (int)facetracker->face->geometry->mean[1] + facetracker->face->geometry->mean[2],       //
                         DRAWING_YUV_RED, 255);                               //
        draw_box_outline(facetracker->image,                                  //
                         (int)x - facetracker->face->geometry->mean[2],       //
                         (int)y - facetracker->face->geometry->mean[2],       //
                         (int)x + facetracker->face->geometry->mean[2],       //
                         (int)y + facetracker->face->geometry->mean[2],       //
                         DRAWING_YUV_GREEN, 255);                             //
        y = (float) facetracker->face->geometry->mean[1];                     //
        draw_box_outline(facetracker->image,                                  //                         
                         (int)x - facetracker->face->geometry->mean[2],       //
                         (int)y - facetracker->face->geometry->mean[2],       //
                         (int)x + facetracker->face->geometry->mean[2],       //
                         (int)y + facetracker->face->geometry->mean[2],       //
                         DRAWING_YUV_YELLOW, 255);                            //
      }                                                                       //

    }                                                                           //
    //////////////////////////////////////////////////////////////////////////////


    //////////////////////////////////////////////////////////////////////////////
    ///////////// Display bboxes etc if so activated /////////////////////////////
    gstfacetracker_printinfo_n_display( facetracker, btrans, has_haarface);


    //////////////////////////////////////////////////////////////////////////////
    ///////////// send an inbound message downstream /////////////////////////////
    gstfacetracker_send_event_downstream( facetracker, btrans, has_haarface);

    ///////////// send an inbound message downstream /////////////////////////////
    gstfacetracker_send_bus_event( facetracker, btrans, has_haarface, gstbuf);

    if (has_haarface) {
      if (facetracker->timer >= FACETRACKER_TIMEOUT)
        GST_INFO("[FaceTracker] Oh, Romeo, there art thou!");
      facetracker->timer = 0;
    }
    else if (facetracker->timer >= FACETRACKER_TIMEOUT) {
      GST_INFO("[FaceTracker] Long time no see... Romeo, where art thou?");
      face_reset(facetracker->face, facetracker->image);
      facetracker->faceAppearanceCnt = 0;
    }
    facetracker->timer++;
  }


  GST_FACETRACKER_UNLOCK (facetracker);
  
  // record statistical info
  if (facetracker->statslog)
    statslog_frame_stop(facetracker->statslog);
  
  return GST_FLOW_OK;
}


gboolean gst_facetracker_plugin_init(GstPlugin * plugin) {
	gst_controller_init(NULL, NULL);

	return gst_element_register(plugin, "facetracker", GST_RANK_NONE, GST_TYPE_FACETRACKER);
}

////GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
//		GST_VERSION_MINOR,
//		"facetracker",
//		"adds an facetracker channel to video - constant or via chroma-keying",
//		plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)







////////////////////////////////////////////////////////////////////////////////
gint gstfacetracker_send_event_downstream(struct _GstFacetracker *facetracker, GstBaseTransform * btrans, bool has_haarface)
{
   //TRACE("[%x] gstfacetracker_send_event_downstream(): face: pos %.2fx%.2f size %.2f\n", (pid_t)syscall(SYS_gettid)), facetracker->face->geometry->mean[0], facetracker->face->geometry->mean[1], facetracker->face->geometry->mean[2]);
   GstStructure *str = gst_structure_new("facelocation", 
                                         "x", G_TYPE_DOUBLE, facetracker->face->geometry->mean[0], 
                                         "y", G_TYPE_DOUBLE, facetracker->face->geometry->mean[1], 
                                         "width", G_TYPE_DOUBLE, facetracker->face->geometry->mean[2] * 2.0 * 0.85, 
                                         "height", G_TYPE_DOUBLE, facetracker->face->geometry->mean[2] * 2.0, 
                                         "facefound", G_TYPE_BOOLEAN, has_haarface, NULL);
   GstEvent* ev = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, str);
   GST_INFO("sending custom DS event of face detection on (%.2f,%.2f)(%.2f)", 
            facetracker->face->geometry->mean[0], facetracker->face->geometry->mean[1], facetracker->face->geometry->mean[2]);
   gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(&(btrans->element)), ev);
   
   return(0);
}


////////////////////////////////////////////////////////////////////////////////
gint gstfacetracker_send_bus_event(struct _GstFacetracker *facetracker, GstBaseTransform * btrans, bool has_haarface, GstBuffer * buf)
{
  guint x = (unsigned int) facetracker->face->geometry->mean[0];
  guint y = (unsigned int) facetracker->face->geometry->mean[1];
  guint w = (unsigned int) facetracker->face->geometry->mean[2] * 2.0 * 0.85;
  guint h = (unsigned int) facetracker->face->geometry->mean[2] * 2.0;
  GstStructure *s;
  ////////////////////////// TJ/ quick hack to add bus msg output //////////////
  bool relative = true; // this should be configurable !!

  if (!relative) {
    s = gst_structure_new ("face",
                           "x", G_TYPE_UINT, x ,
                           "y", G_TYPE_UINT, y,
                           "width", G_TYPE_UINT, w,
                           "height", G_TYPE_UINT, h,
                           "num", G_TYPE_UINT, has_haarface, NULL);
  } else {
    s = gst_structure_new ("face",
                           "x", G_TYPE_FLOAT, (float)x/(float)320,
                           "y", G_TYPE_FLOAT, (float)y/(float)240,
                           "width", G_TYPE_FLOAT, (float)w/(float)320,
                           "height", G_TYPE_FLOAT, (float)h/(float)240, 
                           "num", G_TYPE_UINT, has_haarface, NULL);
  }
  GstMessage *m = gst_message_new_element (GST_OBJECT (facetracker), s);
  
  m->timestamp = GST_BUFFER_TIMESTAMP(buf); //This is the timestamp on the input buf.
  gst_element_post_message (GST_ELEMENT (facetracker), m);
    
   
   return(0);
}



////////////////////////////////////////////////////////////////////////////////
gint gstfacetracker_printinfo_n_display(struct _GstFacetracker *ft, GstBaseTransform * btrans, bool has_haarface)
{
  GST_DEBUG("x:%d y:%d r:%d", 
            (int)ft->face->geometry->mean[0], 
            (int)ft->face->geometry->mean[1], 
            (int)ft->face->geometry->mean[2]);

  if (ft->display) {
#ifdef	FACETRACKER_COLORTRACK
#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
    if (ft->binsInitialized) {
      t_image* in = create_image(COLOR_SPACE_YUV, IMAGE_DATA_FORMAT_PACKED);
      in->width = ft->width;
      in->height = ft->height;

      in->rowbytes = ft->cvYUV->widthStep;
      in->data[0] = (unsigned char*)ft->cvYUV->imageData;

      face_draw_color(ft->face, in, in);

      destroy_image(in);
    }
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_YUVA
    if (ft->binsInitialized)
      face_draw_color(ft->face, ft->image, ft->image);
#endif
#endif

#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA
    CvPoint center;
    center.x = (int)ft->face->geometry->mean[0];
    center.y = (int)ft->face->geometry->mean[1];
    int radius = (int)ft->face->geometry->mean[2];
    cvCircle(ft->img, center, radius, CV_RGB(255, 32, 32), 3);
#endif

#if (FACETRK_FORMAT == FACETRK_FORMAT_YUVA) || (FACETRK_FORMAT == FACETRK_FORMAT_YUV)
    int x = (int)ft->face->geometry->mean[0];
    int y = (int)ft->face->geometry->mean[1];
    int radius = (int)ft->face->geometry->mean[2];

    if( has_haarface )
    {
      draw_box_outline(ft->image, x - radius-1, y - radius-1, x + radius+1, y + radius+1, DRAWING_YUV_BLUE, 255);
      draw_box_outline(ft->image, x - radius-2, y - radius-2, x + radius+2, y + radius+2, DRAWING_YUV_BLUE, 255);
      draw_box_outline(ft->image, x - radius, y - radius, x + radius, y + radius, DRAWING_YUV_BLUE, 255);
    }
    else
    {
      draw_box_outline(ft->image, x - radius-1, y - radius-1, x + radius+1, y + radius+1, DRAWING_YUV_RED, 255);
      draw_box_outline(ft->image, x - radius-2, y - radius-2, x + radius+2, y + radius+2, DRAWING_YUV_RED, 255);
      draw_box_outline(ft->image, x - radius, y - radius, x + radius, y + radius, DRAWING_YUV_RED, 255);
    }

#endif
  }

  return(0);
}

////////////////////////////////////////////////////////////////////////////////
gint gstfacetracker_find_skin_center_of_mass(struct _GstFacetracker *ft, float *x, float *y, gint display,
                                             float seed_x, float seed_y, float seed_r, bool facefound)
{
  int skin_under_seed = 0;
#if FACETRK_FORMAT == FACETRK_FORMAT_YUVA //////////////////////////////////////
  #error dont use YUVA !!!!
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_YUV ///////////////////////////////////////
  IplImage* imageYUV = cvCreateImageHeader(cvSize(ft->width, ft->height), IPL_DEPTH_8U, 3);
  imageYUV->imageData = (char*)ft->image->data[0];
  IplImage* imageRGB = cvCreateImage( cvSize(ft->width, ft->height), IPL_DEPTH_8U, 3);
  cvCvtColor(imageYUV, imageRGB, CV_YUV2RGB);
#endif

#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA //////////////////////////////////////
  #error dont use RGBA !!!!
#endif
  IplImage* imageHSV = cvCreateImage( cvSize(ft->width, ft->height), IPL_DEPTH_8U, 3);
  cvCvtColor(imageRGB, imageHSV, CV_RGB2HSV);

////////////////////////////////////////////////////////////////////////////////
  IplImage* planeH = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Hue component.
  IplImage* planeS = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Saturation component.
  IplImage* planeV = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Brightness component.
  cvCvtPixToPlane(imageHSV, planeH, planeS, planeV, 0);	// Extract the 3 color components.
  IplImage* planeH2 = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Hue component.
  cvCopy( planeH, planeH2, NULL);

  int h_thr =ft->h_thr_high,  s_thr=ft->s_thr_low,   v_thr =ft->v_thr_low;
  int h_thr2=ft->h_thr_low;
  // Detect which pixels in each of the H, S and V channels are probably skin pixels.
  // Assume that skin has a Hue between 0 to 18 (out of 180), and Saturation above 50, and Brightness above 80.
  cvThreshold(planeH , planeH , h_thr, UCHAR_MAX, CV_THRESH_BINARY_INV);     //(hue < 20) !!!!
  cvThreshold(planeS , planeS , s_thr, UCHAR_MAX, CV_THRESH_BINARY);         //(sat > 48)
  cvThreshold(planeV , planeV , v_thr, UCHAR_MAX, CV_THRESH_BINARY);         //(val > 80)
  // erode the HUE to get rid of noise.
  cvErode(planeH,  planeH,  NULL, 1);
  cvThreshold(planeH2 , planeH2 , h_thr2, UCHAR_MAX, CV_THRESH_BINARY);      //(hue > 10)



////////////////////////////////////////////////////////////////////////////////
  // Combine all thresholded color components, so that an output pixel will only
  // be white (255) if the H, S and V pixels were also white.
  IplImage* imageSkinPixels = cvCreateImage( cvGetSize(imageHSV), 8, 1);        // Greyscale output image.
  // originally: imageSkin =  (hue > 10) ^ (hue < 20) ^ (sat > 48) ^ (val > 80), where   ^ mean pixels-wise AND
  cvAnd(planeH         , planeS  , imageSkinPixels);	
  cvAnd(imageSkinPixels, planeV , imageSkinPixels);	
  cvAnd(imageSkinPixels, planeH2,  imageSkinPixels);	


  /// add a weight to the thresholded pixels. /////////////////////////////
  float face_x = ft->face->geometry->mean[0];
  float face_y = ft->face->geometry->mean[1];

#define SKINCOLOUR_WEIGHTING_AROUND_FACE
#ifdef SKINCOLOUR_WEIGHTING_AROUND_FACE
  double beta = (facefound) ? 2.0 : 2.0; // 0.5 : 1.0 ?

  for(int iy=0; iy < imageSkinPixels->height; iy++){
    uchar *ptr = (uchar*) (imageSkinPixels->imageData + iy * imageSkinPixels->widthStep);
    for(int ix=0; ix < imageSkinPixels->width; ix++){
      // pseudo distance: 1==at the point, small == distant
      // NEW: we only look in 2*[(seed_x - seed_r, seed_y - seed_r), (seed_x + seed_r, seed_y + seed_r)]
      //      basically in the bbox 2x size around the face
      if( (seed_x) && (seed_y) && (seed_r) ){
        if( ((seed_x - seed_r*beta) <= ix) && ( ix <= (seed_x + seed_r*beta)) && \
            ((seed_y - seed_r*beta) <= iy) && ( iy <= (seed_y + seed_r*beta)) ){
          ptr[ix] = ptr[ix]/(1 + 35*(abs(ix-face_x)/320.0) + 35*(abs(iy-face_y)/240.0) ); 
        }
        else
          ptr[ix] = 0;
      }
      else
        ptr[ix] = ptr[ix]/(1 + 35*(abs(ix-face_x)/320) + 35*(abs(iy-face_y)/240) );        
    }
  }
#endif //  SKINCOLOUR_WEIGHTING_AROUND_FACE
 
  //then find the centers of mass of the found regions 
  CvMoments moments;
  cvMoments(imageSkinPixels, &moments, 0);
  double m00, m10, m01;
  m00 = cvGetSpatialMoment(&moments, 0,0);
  m10 = cvGetSpatialMoment(&moments, 1,0);
  m01 = cvGetSpatialMoment(&moments, 0,1);
        
  // TBD check that m00 != 0
  if( abs(m00) > 0.1 ){
    *x = m10/m00;
    *y = m01/m00;
    skin_under_seed = 1;
  }
  else{ // no skin pixels under box: use whole image
    //printf(" reset!!\n");
    cvAnd(planeH         , planeS , imageSkinPixels);	
    cvAnd(imageSkinPixels, planeV , imageSkinPixels);	
    cvMoments(imageSkinPixels, &moments, 0);
    m00 = cvGetSpatialMoment(&moments, 0,0);
    m10 = cvGetSpatialMoment(&moments, 1,0);
    m01 = cvGetSpatialMoment(&moments, 0,1);
    if( abs(m00) > 0.1 ){
      *x = m10/m00;
      *y = m01/m00;
    }    
  }

  if(display){
    cvCvtColor(imageSkinPixels, imageRGB, CV_GRAY2RGB);

#if FACETRK_FORMAT == FACETRK_FORMAT_YUV ///////////////////////////////////////
    cvCvtColor(imageRGB, imageYUV, CV_RGB2YUV);
#endif
  }
  //cvCircle(imageYUV,cvPoint(int(*x),int(*y)), 10, CV_RGB(200,50,50),3);


  cvReleaseImage( &imageSkinPixels );
  cvReleaseImage( &planeH );
  cvReleaseImage( &planeS );
  cvReleaseImage( &planeV );
  cvReleaseImage( &planeH2 );
#ifdef ADAPTIVE_SKIN_THRESHOLDING
  cvReleaseImage( &planeS2 );
  cvReleaseImage( &planeV2 );
#endif // ADAPTIVE_SKIN_THRESHOLDING
  cvReleaseImage( &imageHSV );
  cvReleaseImage( &imageRGB );
#if FACETRK_FORMAT == FACETRK_FORMAT_YUV ///////////////////////////////////////
  cvReleaseImage( &imageYUV );
#endif
#if FACETRK_FORMAT == FACETRK_FORMAT_RGBA ///////////////////////////////////////
  cvReleaseImage( &imageRGBA );
#endif

  return(skin_under_seed);
}
