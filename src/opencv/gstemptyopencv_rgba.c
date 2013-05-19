/* GStreamer
 */
/**
 * SECTION:element- emptyopencv_rgba
 *
 * This element is an empty element that accepts RGBA and does the typical ops.
 * A template, if you want.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstemptyopencv_rgba.h"

GST_DEBUG_CATEGORY_STATIC (gst_emptyopencv_rgba_debug);
#define GST_CAT_DEFAULT gst_emptyopencv_rgba_debug


enum {
	PROP_0,
        PROP_DISPLAY,
	PROP_LAST
};

static GstStaticPadTemplate gst_emptyopencv_rgba_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_emptyopencv_rgba_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_EMPTYOPENCV_RGBA_LOCK(emptyopencv_rgba) G_STMT_START { \
	GST_LOG_OBJECT (emptyopencv_rgba, "Locking emptyopencv_rgba from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&emptyopencv_rgba->lock); \
	GST_LOG_OBJECT (emptyopencv_rgba, "Locked emptyopencv_rgba from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_EMPTYOPENCV_RGBA_UNLOCK(emptyopencv_rgba) G_STMT_START { \
	GST_LOG_OBJECT (emptyopencv_rgba, "Unlocking emptyopencv_rgba from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&emptyopencv_rgba->lock); \
} G_STMT_END

static gboolean gst_emptyopencv_rgba_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_emptyopencv_rgba_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_emptyopencv_rgba_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_emptyopencv_rgba_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_emptyopencv_rgba_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_emptyopencv_rgba_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_emptyopencv_rgba_finalize(GObject * object);

GST_BOILERPLATE (GstEmptyopencv_Rgba, gst_emptyopencv_rgba, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanEmptyopencv_Rgba(GstEmptyopencv_Rgba *emptyopencv_rgba) 
{
  if (emptyopencv_rgba->pFrame)  cvReleaseImageHeader(&emptyopencv_rgba->pFrame);
}

static void gst_emptyopencv_rgba_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Image Segmentation filter - Template ", "Filter/Effect/Video",
    "This element does nothing :)",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_emptyopencv_rgba_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_emptyopencv_rgba_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_emptyopencv_rgba_debug, "emptyopencv_rgba", 0, \
                           "emptyopencv_rgba - Template of an RGBA element");
}

static void gst_emptyopencv_rgba_class_init(GstEmptyopencv_RgbaClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_emptyopencv_rgba_set_property;
  gobject_class->get_property = gst_emptyopencv_rgba_get_property;
  gobject_class->finalize = gst_emptyopencv_rgba_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_emptyopencv_rgba_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_emptyopencv_rgba_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_emptyopencv_rgba_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_emptyopencv_rgba_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output would be something different (CHANGEME) ", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_emptyopencv_rgba_init(GstEmptyopencv_Rgba * emptyopencv_rgba, GstEmptyopencv_RgbaClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)emptyopencv_rgba, TRUE);
  g_static_mutex_init(&emptyopencv_rgba->lock);

  emptyopencv_rgba->pFrame        = NULL;
  emptyopencv_rgba->pFrame2       = NULL;

  emptyopencv_rgba->ch1           = NULL;
  emptyopencv_rgba->ch2           = NULL;
  emptyopencv_rgba->ch3           = NULL;

  emptyopencv_rgba->display       = false;
}

static void gst_emptyopencv_rgba_finalize(GObject * object) 
{
  GstEmptyopencv_Rgba *emptyopencv_rgba = GST_EMPTYOPENCV_RGBA (object);
  
  GST_EMPTYOPENCV_RGBA_LOCK (emptyopencv_rgba);
  CleanEmptyopencv_Rgba(emptyopencv_rgba);
  GST_EMPTYOPENCV_RGBA_UNLOCK (emptyopencv_rgba);
  GST_INFO("Emptyopencv_Rgba destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&emptyopencv_rgba->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_emptyopencv_rgba_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstEmptyopencv_Rgba *emptyopencv_rgba = GST_EMPTYOPENCV_RGBA (object);
  
  GST_EMPTYOPENCV_RGBA_LOCK (emptyopencv_rgba);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    emptyopencv_rgba->display = g_value_get_boolean(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_EMPTYOPENCV_RGBA_UNLOCK (emptyopencv_rgba);
}

static void gst_emptyopencv_rgba_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstEmptyopencv_Rgba *emptyopencv_rgba = GST_EMPTYOPENCV_RGBA (object);

  switch (prop_id) {
  case PROP_DISPLAY:
    g_value_set_boolean(value, emptyopencv_rgba->display);
    break; 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_emptyopencv_rgba_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_emptyopencv_rgba_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstEmptyopencv_Rgba *emptyopencv_rgba = GST_EMPTYOPENCV_RGBA (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_EMPTYOPENCV_RGBA_LOCK (emptyopencv_rgba);
  
  gst_video_format_parse_caps(incaps, &emptyopencv_rgba->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &emptyopencv_rgba->out_format, &out_width, &out_height);
  if (!(emptyopencv_rgba->in_format == emptyopencv_rgba->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_EMPTYOPENCV_RGBA_UNLOCK (emptyopencv_rgba);
    return FALSE;
  }
  
  emptyopencv_rgba->width  = in_width;
  emptyopencv_rgba->height = in_height;
  
  GST_INFO("Initialising Emptyopencv_Rgba...");

  const CvSize size = cvSize(emptyopencv_rgba->width, emptyopencv_rgba->height);
  GST_WARNING (" width %d, height %d", emptyopencv_rgba->width, emptyopencv_rgba->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  emptyopencv_rgba->pFrame    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  emptyopencv_rgba->pFrame2   = cvCreateImage(size, IPL_DEPTH_8U, 3);
  emptyopencv_rgba->pFrameA   = cvCreateImage(size, IPL_DEPTH_8U, 1);

  emptyopencv_rgba->ch1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  emptyopencv_rgba->ch2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  emptyopencv_rgba->ch3       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  GST_INFO("Emptyopencv_Rgba initialized.");
  
  GST_EMPTYOPENCV_RGBA_UNLOCK (emptyopencv_rgba);
  
  return TRUE;
}

static void gst_emptyopencv_rgba_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_emptyopencv_rgba_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstEmptyopencv_Rgba *emptyopencv_rgba = GST_EMPTYOPENCV_RGBA (btrans);

  GST_EMPTYOPENCV_RGBA_LOCK (emptyopencv_rgba);

  //////////////////////////////////////////////////////////////////////////////
  // get image data from the input, which is RGBA or BGRA
  emptyopencv_rgba->pFrame->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(emptyopencv_rgba->pFrame,  emptyopencv_rgba->pFrame2, CV_BGRA2BGR);

  //////////////////////////////////////////////////////////////////////////////
  // here we do something with the pFrame2, a simple RGB IplImage
  //////////////////////////////////////////////////////////////////////////////
 

  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( emptyopencv_rgba->display ){
    cvCvtColor( emptyopencv_rgba->pFrameA, emptyopencv_rgba->pFrame2, CV_GRAY2BGR );
  }
  //////////////////////////////////////////////////////////////////////////////
  // copy anyhow the fg/bg to the alpha channel in the output image alpha ch
  cvSplit(emptyopencv_rgba->pFrame2, 
          emptyopencv_rgba->ch1, emptyopencv_rgba->ch2, emptyopencv_rgba->ch3, NULL        );
  cvMerge(emptyopencv_rgba->ch1, emptyopencv_rgba->ch2, emptyopencv_rgba->ch3, emptyopencv_rgba->pFrameA, 
          emptyopencv_rgba->pFrame);
  

  GST_EMPTYOPENCV_RGBA_UNLOCK (emptyopencv_rgba);  
  
  return GST_FLOW_OK;
}


gboolean gst_emptyopencv_rgba_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "emptyopencv_rgba", GST_RANK_NONE, GST_TYPE_EMPTYOPENCV_RGBA);
}
