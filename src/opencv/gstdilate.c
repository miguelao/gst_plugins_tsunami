/* GStreamer
 */
/**
 * SECTION:element- dilate
 *
 * This element takes a RGB image, and applies the dilate morphological operator.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstdilate.h"

GST_DEBUG_CATEGORY_STATIC (gst_dilate_debug);
#define GST_CAT_DEFAULT gst_dilate_debug


enum {
	PROP_0,
	PROP_ITERATIONS,
	PROP_LAST
};

static GstStaticPadTemplate gst_dilate_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_dilate_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_DILATE_LOCK(dilate) G_STMT_START { \
	GST_LOG_OBJECT (dilate, "Locking dilate from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&dilate->lock); \
	GST_LOG_OBJECT (dilate, "Locked dilate from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_DILATE_UNLOCK(dilate) G_STMT_START { \
	GST_LOG_OBJECT (dilate, "Unlocking dilate from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&dilate->lock); \
} G_STMT_END

static gboolean gst_dilate_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_dilate_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_dilate_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_dilate_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_dilate_transform(GstBaseTransform * btrans, GstBuffer * in, GstBuffer * out);

static void gst_dilate_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_dilate_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_dilate_finalize(GObject * object);

GST_BOILERPLATE (GstDilate, gst_dilate, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanDilate(GstDilate *dilate) 
{
  if (dilate->cvBGR)        cvReleaseImageHeader(&dilate->cvBGR);
  if (dilate->cvBGR_out)    cvReleaseImageHeader(&dilate->cvBGR_out);
}

static void gst_dilate_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Dilate filter", "Filter/Effect/Video",
    "Apply morphological dilation filter to an image",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_dilate_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_dilate_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_dilate_debug, "dilate2", 0, \
                           "dilate - Apply morphological dilation to an image");
}

static void gst_dilate_class_init(GstDilateClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_dilate_set_property;
  gobject_class->get_property = gst_dilate_get_property;
  gobject_class->finalize = gst_dilate_finalize;
  
  btrans_class->passthrough_on_same_caps = FALSE;
  //btrans_class->always_in_place = FALSE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_dilate_transform_ip);
  btrans_class->transform    = GST_DEBUG_FUNCPTR (gst_dilate_transform);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_dilate_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_dilate_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_dilate_set_caps);

  g_object_class_install_property(gobject_class, PROP_ITERATIONS, 
                                  g_param_spec_int("iters", "Amount of dilation iterations",
                                                      "Amount of dilation iterations", 
                                                      1, 100, 1, (GParamFlags)G_PARAM_READWRITE ));
}

static void gst_dilate_init(GstDilate * dilate, GstDilateClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)dilate, TRUE);
  g_static_mutex_init(&dilate->lock);
  dilate->cvBGR      = NULL;
  dilate->cvBGR      = NULL;
  dilate->iterations = 1;
}

static void gst_dilate_finalize(GObject * object) 
{
  GstDilate *dilate = GST_DILATE (object);
  
  GST_DILATE_LOCK (dilate);
  CleanDilate(dilate);
  GST_DILATE_UNLOCK (dilate);
  GST_INFO("Dilate destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&dilate->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_dilate_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstDilate *dilate = GST_DILATE (object);
  
  GST_DILATE_LOCK (dilate);
  switch (prop_id) {
  case PROP_ITERATIONS:
    dilate->iterations = g_value_get_int(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_DILATE_UNLOCK (dilate);
}

static void gst_dilate_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstDilate *dilate = GST_DILATE (object);

  switch (prop_id) { 
  case PROP_ITERATIONS:
    g_value_set_int(value, dilate->iterations);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_dilate_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_dilate_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstDilate *dilate = GST_DILATE (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_DILATE_LOCK (dilate);
  
  gst_video_format_parse_caps(incaps, &dilate->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &dilate->out_format, &out_width, &out_height);

  if (!(dilate->in_format == dilate->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_DILATE_UNLOCK (dilate);
    return FALSE;
  }
  
  dilate->width  = in_width;
  dilate->height = in_height;
  
  GST_INFO("Initialising Dilate...");

  const CvSize size = cvSize(dilate->width, dilate->height);
  GST_WARNING (" width %d, height %d", dilate->width, dilate->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB  ////////////////////////////////////////////
  dilate->cvBGR     = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);
  dilate->cvBGR_out = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);

  GST_INFO("Dilate initialized.");
  
  GST_DILATE_UNLOCK (dilate);
  
  return TRUE;
}

static void gst_dilate_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_dilate_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstDilate *dilate = GST_DILATE (btrans);

  GST_DILATE_LOCK (dilate);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR
  dilate->cvBGR->imageData = (char*)GST_BUFFER_DATA(gstbuf);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
  cvDilate( dilate->cvBGR, dilate->cvBGR, NULL, dilate->iterations);

  //////////////////////////////////////////////////////////////////////////////
  GST_DILATE_UNLOCK (dilate);  
  
  return GST_FLOW_OK;
}

static GstFlowReturn gst_dilate_transform(GstBaseTransform * btrans, GstBuffer * in, GstBuffer * out) 
{
  GstDilate *dilate = GST_DILATE (btrans);

  GST_DILATE_LOCK (dilate);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR
  dilate->cvBGR->imageData     = (char*)GST_BUFFER_DATA(in);
  dilate->cvBGR_out->imageData = (char*)GST_BUFFER_DATA(out);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
  cvDilate( dilate->cvBGR, dilate->cvBGR_out, NULL, dilate->iterations);

  //////////////////////////////////////////////////////////////////////////////
  GST_DILATE_UNLOCK (dilate);  
  
  return GST_FLOW_OK;
}


gboolean gst_dilate_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "dilate2", GST_RANK_NONE, GST_TYPE_DILATE);
}
