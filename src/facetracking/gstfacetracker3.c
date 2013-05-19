/* GStreamer
 */
/**
 * SECTION:element- facetracker3
 *
 * This element detects and tracks the largest face position in the scene.
 * 
 * Different from facetracker2: profile face is also used.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstfacetracker3.h"
#include "facetracking_kernel.h"

GST_DEBUG_CATEGORY_STATIC (gst_facetracker3_debug);
#define GST_CAT_DEFAULT gst_facetracker3_debug

#define DEFAULT_PROFILE  "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml"
#define DEFAULT_PROFILE2 "/home/mcasassa/haar/HS.xml"
//#define DEFAULT_PROFILE3 "/home/mcasassa/haar/ojoD.xml"
#define DEFAULT_PROFILE3 "/usr/share/opencv/haarcascades/haarcascade_profileface.xml"

#define DEFAULT_SCALE_FACTOR 1.2
#define DEFAULT_MIN_NEIGHBORS 3
#define DEFAULT_MIN_SIZE_WIDTH 0
#define DEFAULT_MIN_SIZE_HEIGHT 0
#define DEFAULT_FPS G_MAXINT

#define FACETRACKER3_TIMEOUT 50

enum {
	PROP_0,
	PROP_FPS,
	PROP_STATS,
	PROP_SETFPS,
	PROP_DISPLAY,
	PROP_PROFILE,
	PROP_PROFILE2,
	PROP_SCALE_FACTOR,
	PROP_MIN_NEIGHBORS,
	PROP_MIN_SIZE_WIDTH,
	PROP_MIN_SIZE_HEIGHT,
	PROP_TRACKER,
	PROP_LAST
};

static GstStaticPadTemplate gst_facetracker3_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_facetracker3_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_FACETRACKER3_LOCK(facetracker3) G_STMT_START { \
	GST_LOG_OBJECT (facetracker3, "Locking facetracker3 from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&facetracker3->lock); \
	GST_LOG_OBJECT (facetracker3, "Locked facetracker3 from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_FACETRACKER3_UNLOCK(facetracker3) G_STMT_START { \
	GST_LOG_OBJECT (facetracker3, "Unlocking facetracker3 from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&facetracker3->lock); \
} G_STMT_END

static gboolean gst_facetracker3_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_facetracker3_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_facetracker3_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_facetracker3_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_facetracker3_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_facetracker3_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_facetracker3_finalize(GObject * object);

GST_BOILERPLATE (GstFacetracker3, gst_facetracker3, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanFacetracker3(GstFacetracker3 *facetracker3) 
{
  if (facetracker3->hc )  haarwrapper_destroy(facetracker3->hc );
  if (facetracker3->hc2)  haarwrapper_destroy(facetracker3->hc2);
  if (facetracker3->hc3)  haarwrapper_destroy(facetracker3->hc3);


  if (facetracker3->face )  free(facetracker3->face );
  if (facetracker3->torso)  free(facetracker3->torso);

  if (facetracker3->image_rgb)   haarwrapper_destroy_image(facetracker3->image_rgb);
  
  if (facetracker3->cvBGR)  cvReleaseImageHeader(&facetracker3->cvBGR);
}

static void gst_facetracker3_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Facetracker3 filter", "Filter/Effect/Video",
    "Performs multiple item detection and tracking using OpenCV -- KALMANISATION!",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_facetracker3_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_facetracker3_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_facetracker3_debug, "facetracker3", 0, \
                           "facetracker3 - Performs face detection and tracking on input images");
}

static void gst_facetracker3_class_init(GstFacetracker3Class * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_facetracker3_set_property;
  gobject_class->get_property = gst_facetracker3_get_property;
  gobject_class->finalize = gst_facetracker3_finalize;
  
  g_object_class_install_property (gobject_class, PROP_FPS, g_param_spec_boolean ("fps", "FPS", "Show frames/second",
                                                                                  FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
  g_object_class_install_property(gobject_class, PROP_SETFPS, g_param_spec_int("setfps", "SETFPS", "set the maximum frames/second", 1,
                                                                               G_MAXINT, DEFAULT_FPS, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_DISPLAY, g_param_spec_boolean("display", "Display",
                                                                                    "Sets whether the detected faces should be highlighted in the output", TRUE, (GParamFlags)(G_PARAM_READWRITE
                                                                                                                                                                               | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_PROFILE, g_param_spec_string("profile", "Profile",
                                                                                   "Location of Haar cascade file to use for face detection", DEFAULT_PROFILE, (GParamFlags)(G_PARAM_READWRITE
                                                                                                                                                                             | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_PROFILE2, g_param_spec_string("profile2", "Profile2",
                                                                                    "Location of Haar cascade file to use for torso detection", DEFAULT_PROFILE2, (GParamFlags)(G_PARAM_READWRITE
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
  g_object_class_install_property(gobject_class, PROP_TRACKER, 
                                  g_param_spec_int("tracker", "tracker",
                                                   "tracker on/off", 0, 1, 1, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_facetracker3_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_facetracker3_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_facetracker3_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_facetracker3_set_caps);
}

static void gst_facetracker3_init(GstFacetracker3 * facetracker3, GstFacetracker3Class * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)facetracker3, TRUE);
  g_static_mutex_init(&facetracker3->lock);
  facetracker3->setfps = DEFAULT_FPS;
  facetracker3->setfps_delay = 0;
  facetracker3->profile = g_strdup(DEFAULT_PROFILE);
  facetracker3->profile2 = g_strdup(DEFAULT_PROFILE2);
  facetracker3->profile3 = g_strdup(DEFAULT_PROFILE3);
  facetracker3->display = 0;
  facetracker3->tracker = 1;
  facetracker3->nframes=0;
  facetracker3->nframes_with_face_detected=0;
  facetracker3->scale_factor = DEFAULT_SCALE_FACTOR;
  facetracker3->min_neighbors = DEFAULT_MIN_NEIGHBORS;
  facetracker3->min_size_width = DEFAULT_MIN_SIZE_WIDTH;
  facetracker3->min_size_height = DEFAULT_MIN_SIZE_HEIGHT;

  facetracker3->image_rgb = NULL;
  facetracker3->cvBGR     = NULL;

  facetracker3->hc  = NULL;
  facetracker3->hc2 = NULL;
  facetracker3->hc3 = NULL;

  facetracker3->timer = 0;
}

static void gst_facetracker3_finalize(GObject * object) 
{
  GstFacetracker3 *facetracker3 = GST_FACETRACKER3 (object);
  
  GST_WARNING(" faces detected in %ld frames out of %ld in total (%f%%)", 
              facetracker3->nframes_with_face_detected, facetracker3->nframes,
              100.0 * facetracker3->nframes_with_face_detected/ facetracker3->nframes);
  
  GST_FACETRACKER3_LOCK (facetracker3);
  CleanFacetracker3(facetracker3);
  g_free(facetracker3->profile);
  GST_FACETRACKER3_UNLOCK (facetracker3);
  GST_INFO("Facetracker3 destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&facetracker3->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_facetracker3_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) {
	GstFacetracker3 *facetracker3 = GST_FACETRACKER3 (object);

	GST_FACETRACKER3_LOCK (facetracker3);
	switch (prop_id) {
	case PROP_FPS:
		break;
	case PROP_STATS:
		// read only
		break;
	case PROP_SETFPS:
		facetracker3->setfps = g_value_get_int(value);
//		if (facetracker3->setfps > 500) {
//			facetracker3->setfps_delay = 0;
//		} else {
//			facetracker3->setfps_delay = 1000000ull / facetracker3->setfps;
//			facetracker3->setfps_time = getTime() + facetracker3->setfps_delay;
//		}
		break;
	case PROP_PROFILE:
		g_free(facetracker3->profile);
		facetracker3->profile = g_value_dup_string(value);
		break;
	case PROP_PROFILE2:
		g_free(facetracker3->profile2);
		facetracker3->profile2 = g_value_dup_string(value);
		break;
	case PROP_DISPLAY:
		facetracker3->display = g_value_get_boolean(value);
		break;
	case PROP_SCALE_FACTOR:
		facetracker3->scale_factor = g_value_get_double(value);
		break;
	case PROP_MIN_NEIGHBORS:
		facetracker3->min_neighbors = g_value_get_int(value);
		break;
	case PROP_MIN_SIZE_WIDTH:
		facetracker3->min_size_width = g_value_get_int(value);
		break;
	case PROP_MIN_SIZE_HEIGHT:
		facetracker3->min_size_height = g_value_get_int(value);
		break;
	case PROP_TRACKER:
		facetracker3->tracker = g_value_get_int(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
	GST_FACETRACKER3_UNLOCK (facetracker3);
}

static void gst_facetracker3_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstFacetracker3 *facetracker3 = GST_FACETRACKER3 (object);

  switch (prop_id) {
  case PROP_SETFPS:
    g_value_set_int(value, facetracker3->setfps);
    break;
  case PROP_PROFILE:
    g_value_set_string(value, facetracker3->profile);
    break;
  case PROP_PROFILE2:
    g_value_set_string(value, facetracker3->profile2);
    break;
  case PROP_DISPLAY:
    g_value_set_boolean(value, facetracker3->display);
    break;
  case PROP_SCALE_FACTOR:
    g_value_set_double(value, facetracker3->scale_factor);
    break;
  case PROP_MIN_NEIGHBORS:
    g_value_set_int(value, facetracker3->min_neighbors);
    break;
  case PROP_MIN_SIZE_WIDTH:
    g_value_set_int(value, facetracker3->min_size_width);
    break;
  case PROP_MIN_SIZE_HEIGHT:
    g_value_set_int(value, facetracker3->min_size_height);
    break;
  case PROP_TRACKER:
    g_value_set_int(value, facetracker3->tracker);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_facetracker3_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_facetracker3_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstFacetracker3 *facetracker3 = GST_FACETRACKER3 (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_FACETRACKER3_LOCK (facetracker3);
  
  gst_video_format_parse_caps(incaps, &facetracker3->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &facetracker3->out_format, &out_width, &out_height);
  if (!(facetracker3->in_format == facetracker3->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_FACETRACKER3_UNLOCK (facetracker3);
    return FALSE;
  }
  
  facetracker3->width  = in_width;
  facetracker3->height = in_height;
  
  GST_INFO("Initialising Facetracker3...");

  // init haar classifiers /////////////////////////////////////////////////////
  CleanFacetracker3(facetracker3);
  facetracker3->hc  = haarwrapper_create(facetracker3->profile );
  if( facetracker3->profile2 )
    facetracker3->hc2 = haarwrapper_create(facetracker3->profile2);
  if( facetracker3->profile3 )
    facetracker3->hc3 = haarwrapper_create(facetracker3->profile3);
  
  const CvSize size = cvSize(facetracker3->width, facetracker3->height);
  GST_WARNING (" width %d, height %d", facetracker3->width, facetracker3->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in BGR  ////////////////////////////////////////////
  facetracker3->cvBGR_input = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);
  facetracker3->cvBGR       = cvCreateImage      (size, IPL_DEPTH_8U, 3);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in BGR or RGB or similar ///////////////////////////
  // this is needed for internal manipulation, so the IplImage needs to be /////
  facetracker3->image_rgb = haarwrapper_create_image();
  haarwrapper_resize_image(facetracker3->image_rgb, facetracker3->width, facetracker3->height);
  // point our image struct buffers to the cv-allocated ones
  haarwrapper_setdata_image(facetracker3->image_rgb, (guint8*)facetracker3->cvBGR->imageData);

  //a scratchpad image in rgb
  facetracker3->image_rgb2 = haarwrapper_create_image();
  haarwrapper_resize_image(facetracker3->image_rgb2, facetracker3->width, facetracker3->height);
  haarwrapper_alloc_image(facetracker3->image_rgb2, facetracker3->width, facetracker3->height); 

  // init bounding boxes ///////////////////////////////////////////////////////
  if (facetracker3->face == NULL)    facetracker3->face     = bbox_int_create();  
  if (facetracker3->torso == NULL)   facetracker3->torso    = bbox_int_create();
  if (facetracker3->side == NULL)    facetracker3->side     = bbox_int_create();

  facetracker3->facek   =  facetracking_init();
  facetracker3->torsok  =  facetracking_init();
  facetracker3->sidek   =  facetracking_init();


  facetracker3->timer = 0;
  GST_INFO("Facetracker3 initialized.");
  
  facetracker3->faceAppearanceCnt = 0;
  facetracker3->binsInitialized = 0;
  GST_FACETRACKER3_UNLOCK (facetracker3);
  
  return TRUE;
}

static void gst_facetracker3_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_facetracker3_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstFacetracker3 *facetracker3 = GST_FACETRACKER3 (btrans);

  GST_FACETRACKER3_LOCK (facetracker3);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc

  // get image data from the input, which is BGR
  facetracker3->cvBGR_input->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCopy(facetracker3->cvBGR_input, facetracker3->cvBGR, NULL);

  //////////////////////////////////////////////////////////////////////////////
  // Actual object detection and kalman tracking
  facetracking_kernel(facetracker3);



  //////////////////////////////////////////////////////////////////////////////
  // Box drawing, output text etc
  if( facetracker3->display ){
    facetracking_display_info(facetracker3);
  }
  
  GST_INFO(" detected face in %d,%d, w %d, h %d", 
           facetracker3->face->x, facetracker3->face->y, facetracker3->face->w, facetracker3->face->h);

  //miguel casas, addendum, send an inbound message downstream /////////////////
  GstStructure *str = gst_structure_new("facelocation", 
                                        "x", G_TYPE_DOUBLE, (float)(facetracker3->face->x + facetracker3->face->w/2),
                                        "y", G_TYPE_DOUBLE, (float)(facetracker3->face->y + facetracker3->face->h/2),
                                        "width", G_TYPE_DOUBLE, (float)facetracker3->face->w *0.75, 
                                        "height", G_TYPE_DOUBLE, (float)facetracker3->face->h * 0.85, NULL);
  GstEvent* ev = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, str);  
  gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(&(btrans->element)), ev);
  
 
  //facetracker3->nframes++; facetracker3->nframes_with_face_detected += ( nobjects > 0 ) ? 1 :0;
 
  GST_FACETRACKER3_UNLOCK (facetracker3);
  
  
  return GST_FLOW_OK;
}


gboolean gst_facetracker3_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "facetracker3", GST_RANK_NONE, GST_TYPE_FACETRACKER3);
}
