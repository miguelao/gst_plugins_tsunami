/* GStreamer
 */
/**
 * SECTION:element- retinex
 *
 * This element is an empty element that accepts RGBA and does the a retinex illumination comp.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstretinex.h"
#include "retinex.h"

GST_DEBUG_CATEGORY_STATIC (gst_retinex_debug);
#define GST_CAT_DEFAULT gst_retinex_debug


enum {
	PROP_0,
        PROP_DISPLAY,
	PROP_LAST
};

static GstStaticPadTemplate gst_retinex_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_retinex_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_RETINEX_LOCK(retinex) G_STMT_START { \
	GST_LOG_OBJECT (retinex, "Locking retinex from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&retinex->lock); \
	GST_LOG_OBJECT (retinex, "Locked retinex from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_RETINEX_UNLOCK(retinex) G_STMT_START { \
	GST_LOG_OBJECT (retinex, "Unlocking retinex from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&retinex->lock); \
} G_STMT_END

static gboolean gst_retinex_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_retinex_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_retinex_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_retinex_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_retinex_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_retinex_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_retinex_finalize(GObject * object);

GST_BOILERPLATE (GstRetinex, gst_retinex, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanRetinex(GstRetinex *retinex) 
{
  if (retinex->pFrame)  cvReleaseImageHeader(&retinex->pFrame);
}

static void gst_retinex_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Image Segmentation filter - Template ", "Filter/Effect/Video",
    "This element uses the Retinex algorithm to compensate illumination",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_retinex_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_retinex_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_retinex_debug, "retinex", 0, \
                           "retinex illumination compensation");
}

static void gst_retinex_class_init(GstRetinexClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_retinex_set_property;
  gobject_class->get_property = gst_retinex_get_property;
  gobject_class->finalize = gst_retinex_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_retinex_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_retinex_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_retinex_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_retinex_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output would be the actual compensation ", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_retinex_init(GstRetinex * retinex, GstRetinexClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)retinex, TRUE);
  g_static_mutex_init(&retinex->lock);

  retinex->pFrame        = NULL;
  retinex->pFrame2       = NULL;
  retinex->pFrameA       = NULL;

  retinex->ch1           = NULL;
  retinex->ch2           = NULL;
  retinex->ch3           = NULL;

  retinex->display       = false;
}

static void gst_retinex_finalize(GObject * object) 
{
  GstRetinex *retinex = GST_RETINEX (object);
  
  GST_RETINEX_LOCK (retinex);
  CleanRetinex(retinex);
  GST_RETINEX_UNLOCK (retinex);
  GST_INFO("Retinex destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&retinex->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_retinex_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstRetinex *retinex = GST_RETINEX (object);
  
  GST_RETINEX_LOCK (retinex);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    retinex->display = g_value_get_boolean(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_RETINEX_UNLOCK (retinex);
}

static void gst_retinex_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstRetinex *retinex = GST_RETINEX (object);

  switch (prop_id) {
  case PROP_DISPLAY:
    g_value_set_boolean(value, retinex->display);
    break; 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_retinex_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_retinex_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstRetinex *retinex = GST_RETINEX (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_RETINEX_LOCK (retinex);
  
  gst_video_format_parse_caps(incaps, &retinex->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &retinex->out_format, &out_width, &out_height);
  if (!(retinex->in_format == retinex->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_RETINEX_UNLOCK (retinex);
    return FALSE;
  }
  
  retinex->width  = in_width;
  retinex->height = in_height;
  
  GST_INFO("Initialising Retinex...");

  const CvSize size = cvSize(retinex->width, retinex->height);
  GST_WARNING (" width %d, height %d", retinex->width, retinex->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  retinex->pFrame    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  retinex->pFrame2   = cvCreateImage(size, IPL_DEPTH_8U, 3);
  retinex->pFrameA   = cvCreateImage(size, IPL_DEPTH_8U, 1);

  retinex->ch1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  retinex->ch2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  retinex->ch3       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  
  GST_INFO("Retinex initialized.");
  
  GST_RETINEX_UNLOCK (retinex);
  
  return TRUE;
}

static void gst_retinex_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_retinex_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstRetinex *retinex = GST_RETINEX (btrans);

  GST_RETINEX_LOCK (retinex);

  //////////////////////////////////////////////////////////////////////////////
  // get image data from the input, which is RGBA or BGRA
  retinex->pFrame->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvSplit(retinex->pFrame, 
          retinex->ch1, retinex->ch2, retinex->ch3, NULL        );
  cvMerge(retinex->ch1, retinex->ch2, retinex->ch3, NULL, 
          retinex->pFrame2);

  double sigma = 14.0;
  int gain = 128;
  int offset = 128;
  Retinex( retinex->pFrame2, sigma, gain, offset );
  cvSplit(retinex->pFrame2,  retinex->ch1, retinex->ch2, retinex->ch3, NULL);

  //////////////////////////////////////////////////////////////////////////////
  // restore alpha channel from input
  cvMerge(retinex->ch1, retinex->ch2, retinex->ch3, retinex->pFrameA, 
          retinex->pFrame);
  

  GST_RETINEX_UNLOCK (retinex);  
  
  return GST_FLOW_OK;
}


gboolean gst_retinex_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "retinex", GST_RANK_NONE, GST_TYPE_RETINEX);
}
