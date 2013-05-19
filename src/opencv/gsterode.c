/* GStreamer
 */
/**
 * SECTION:element- erode
 *
 * This element takes a RGB image, and applies the erode morphological operator.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gsterode.h"

GST_DEBUG_CATEGORY_STATIC (gst_erode_debug);
#define GST_CAT_DEFAULT gst_erode_debug


enum {
	PROP_0,
	PROP_ITERATIONS,
	PROP_LAST
};

static GstStaticPadTemplate gst_erode_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_erode_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_ERODE_LOCK(erode) G_STMT_START { \
	GST_LOG_OBJECT (erode, "Locking erode from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&erode->lock); \
	GST_LOG_OBJECT (erode, "Locked erode from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_ERODE_UNLOCK(erode) G_STMT_START { \
	GST_LOG_OBJECT (erode, "Unlocking erode from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&erode->lock); \
} G_STMT_END

static gboolean gst_erode_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_erode_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_erode_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_erode_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_erode_transform(GstBaseTransform * btrans, GstBuffer * in, GstBuffer * out);

static void gst_erode_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_erode_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_erode_finalize(GObject * object);

GST_BOILERPLATE (GstErode, gst_erode, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanErode(GstErode *erode) 
{
  if (erode->cvBGR)        cvReleaseImageHeader(&erode->cvBGR);
  if (erode->cvBGR_out)    cvReleaseImageHeader(&erode->cvBGR_out);
}

static void gst_erode_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Erode filter", "Filter/Effect/Video",
    "Apply morphological erosion filter to an image",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_erode_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_erode_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_erode_debug, "erode", 0, \
                           "erode - Apply morphological erosion to an image");
}

static void gst_erode_class_init(GstErodeClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_erode_set_property;
  gobject_class->get_property = gst_erode_get_property;
  gobject_class->finalize = gst_erode_finalize;
  
  btrans_class->passthrough_on_same_caps = FALSE;
  //btrans_class->always_in_place = FALSE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_erode_transform_ip);
  btrans_class->transform    = GST_DEBUG_FUNCPTR (gst_erode_transform);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_erode_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_erode_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_erode_set_caps);

  g_object_class_install_property(gobject_class, PROP_ITERATIONS, 
                                  g_param_spec_int("iters", "Amount of erosion iterations",
                                                      "Amount of erosion iterations", 
                                                      1, 100, 1, (GParamFlags)G_PARAM_READWRITE ));
}

static void gst_erode_init(GstErode * erode, GstErodeClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)erode, TRUE);
  g_static_mutex_init(&erode->lock);
  erode->cvBGR      = NULL;
  erode->cvBGR      = NULL;
  erode->iterations = 1;
}

static void gst_erode_finalize(GObject * object) 
{
  GstErode *erode = GST_ERODE (object);
  
  GST_ERODE_LOCK (erode);
  CleanErode(erode);
  GST_ERODE_UNLOCK (erode);
  GST_INFO("Erode destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&erode->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_erode_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstErode *erode = GST_ERODE (object);
  
  GST_ERODE_LOCK (erode);
  switch (prop_id) {
  case PROP_ITERATIONS:
    erode->iterations = g_value_get_int(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_ERODE_UNLOCK (erode);
}

static void gst_erode_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstErode *erode = GST_ERODE (object);

  switch (prop_id) { 
  case PROP_ITERATIONS:
    g_value_set_int(value, erode->iterations);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_erode_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_erode_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstErode *erode = GST_ERODE (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_ERODE_LOCK (erode);
  
  gst_video_format_parse_caps(incaps, &erode->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &erode->out_format, &out_width, &out_height);

  if (!(erode->in_format == erode->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_ERODE_UNLOCK (erode);
    return FALSE;
  }
  
  erode->width  = in_width;
  erode->height = in_height;
  
  GST_INFO("Initialising Erode...");

  const CvSize size = cvSize(erode->width, erode->height);
  GST_WARNING (" width %d, height %d", erode->width, erode->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB  ////////////////////////////////////////////
  erode->cvBGR     = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);
  erode->cvBGR_out = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);

  GST_INFO("Erode initialized.");
  
  GST_ERODE_UNLOCK (erode);
  
  return TRUE;
}

static void gst_erode_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_erode_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstErode *erode = GST_ERODE (btrans);

  GST_ERODE_LOCK (erode);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR
  erode->cvBGR->imageData = (char*)GST_BUFFER_DATA(gstbuf);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
  cvErode( erode->cvBGR, erode->cvBGR, NULL, erode->iterations);

  //////////////////////////////////////////////////////////////////////////////
  GST_ERODE_UNLOCK (erode);  
  
  return GST_FLOW_OK;
}

static GstFlowReturn gst_erode_transform(GstBaseTransform * btrans, GstBuffer * in, GstBuffer * out) 
{
  GstErode *erode = GST_ERODE (btrans);

  GST_ERODE_LOCK (erode);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR
  erode->cvBGR->imageData     = (char*)GST_BUFFER_DATA(in);
  erode->cvBGR_out->imageData = (char*)GST_BUFFER_DATA(out);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
  cvErode( erode->cvBGR, erode->cvBGR_out, NULL, erode->iterations);

  //////////////////////////////////////////////////////////////////////////////
  GST_ERODE_UNLOCK (erode);  
  
  return GST_FLOW_OK;
}


gboolean gst_erode_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "erode", GST_RANK_NONE, GST_TYPE_ERODE);
}
