/* GStreamer
 */
/**
 * SECTION:element- emptyopencv
 *
 * This element is empty, it does nothing. But it has all the necessary housekeeping
 * functionality for any user to copy-modify from here any opencv functional element
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstemptyopencv.h"

GST_DEBUG_CATEGORY_STATIC (gst_emptyopencv_debug);
#define GST_CAT_DEFAULT gst_emptyopencv_debug


enum {
	PROP_0,
	PROP_LAST
};

static GstStaticPadTemplate gst_emptyopencv_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_emptyopencv_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_EMPTYOPENCV_LOCK(emptyopencv) G_STMT_START { \
	GST_LOG_OBJECT (emptyopencv, "Locking emptyopencv from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&emptyopencv->lock); \
	GST_LOG_OBJECT (emptyopencv, "Locked emptyopencv from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_EMPTYOPENCV_UNLOCK(emptyopencv) G_STMT_START { \
	GST_LOG_OBJECT (emptyopencv, "Unlocking emptyopencv from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&emptyopencv->lock); \
} G_STMT_END

static gboolean gst_emptyopencv_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_emptyopencv_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_emptyopencv_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_emptyopencv_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_emptyopencv_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_emptyopencv_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_emptyopencv_finalize(GObject * object);

GST_BOILERPLATE (GstEmptyopencv, gst_emptyopencv, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanEmptyopencv(GstEmptyopencv *emptyopencv) 
{
  if (emptyopencv->cvRGB)  cvReleaseImageHeader(&emptyopencv->cvRGB);
}

static void gst_emptyopencv_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Emptyopencv filter", "Filter/Effect/Video",
    "Performs some image adjust operations",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_emptyopencv_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_emptyopencv_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_emptyopencv_debug, "emptyopencv", 0, \
                           "emptyopencv - Performs some image adjust operations");
}

static void gst_emptyopencv_class_init(GstEmptyopencvClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_emptyopencv_set_property;
  gobject_class->get_property = gst_emptyopencv_get_property;
  gobject_class->finalize = gst_emptyopencv_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_emptyopencv_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_emptyopencv_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_emptyopencv_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_emptyopencv_set_caps);
}

static void gst_emptyopencv_init(GstEmptyopencv * emptyopencv, GstEmptyopencvClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)emptyopencv, TRUE);
  g_static_mutex_init(&emptyopencv->lock);
  emptyopencv->cvRGB     = NULL;

}

static void gst_emptyopencv_finalize(GObject * object) 
{
  GstEmptyopencv *emptyopencv = GST_EMPTYOPENCV (object);
  
  GST_EMPTYOPENCV_LOCK (emptyopencv);
  CleanEmptyopencv(emptyopencv);
  GST_EMPTYOPENCV_UNLOCK (emptyopencv);
  GST_INFO("Emptyopencv destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&emptyopencv->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_emptyopencv_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstEmptyopencv *emptyopencv = GST_EMPTYOPENCV (object);
  
  GST_EMPTYOPENCV_LOCK (emptyopencv);
  switch (prop_id) {
    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_EMPTYOPENCV_UNLOCK (emptyopencv);
}

static void gst_emptyopencv_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  //GstEmptyopencv *emptyopencv = GST_EMPTYOPENCV (object);

  switch (prop_id) { 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_emptyopencv_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_emptyopencv_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstEmptyopencv *emptyopencv = GST_EMPTYOPENCV (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_EMPTYOPENCV_LOCK (emptyopencv);
  
  gst_video_format_parse_caps(incaps, &emptyopencv->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &emptyopencv->out_format, &out_width, &out_height);
  if (!(emptyopencv->in_format == emptyopencv->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_EMPTYOPENCV_UNLOCK (emptyopencv);
    return FALSE;
  }
  
  emptyopencv->width  = in_width;
  emptyopencv->height = in_height;
  
  GST_INFO("Initialising Emptyopencv...");

  const CvSize size = cvSize(emptyopencv->width, emptyopencv->height);
  GST_WARNING (" width %d, height %d", emptyopencv->width, emptyopencv->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB (BGR in reality, careful)////////////////////
  emptyopencv->cvRGB = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);


  GST_INFO("Emptyopencv initialized.");
  
  GST_EMPTYOPENCV_UNLOCK (emptyopencv);
  
  return TRUE;
}

static void gst_emptyopencv_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_emptyopencv_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstEmptyopencv *emptyopencv = GST_EMPTYOPENCV (btrans);

  GST_EMPTYOPENCV_LOCK (emptyopencv);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is RGB
  emptyopencv->cvRGB->imageData = (char*)GST_BUFFER_DATA(gstbuf);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
 
  GST_EMPTYOPENCV_UNLOCK (emptyopencv);  
  
  return GST_FLOW_OK;
}


gboolean gst_emptyopencv_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "emptyopencv", GST_RANK_NONE, GST_TYPE_EMPTYOPENCV);
}
