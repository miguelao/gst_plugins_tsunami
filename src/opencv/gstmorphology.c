/* GStreamer
 */
/**
 * SECTION:element- morphology
 *
 * This element takes a RGB image, and applies one morphology operator.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstmorphology.h"

GST_DEBUG_CATEGORY_STATIC (gst_morphology_debug);
#define GST_CAT_DEFAULT gst_morphology_debug


enum {
	PROP_0,
	PROP_OP,
	PROP_ITERATIONS,
	PROP_LAST
};

static GstStaticPadTemplate gst_morphology_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_morphology_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_MORPHOLOGY_LOCK(morphology) G_STMT_START { \
	GST_LOG_OBJECT (morphology, "Locking morphology from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&morphology->lock); \
	GST_LOG_OBJECT (morphology, "Locked morphology from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_MORPHOLOGY_UNLOCK(morphology) G_STMT_START { \
	GST_LOG_OBJECT (morphology, "Unlocking morphology from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&morphology->lock); \
} G_STMT_END

static gboolean gst_morphology_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_morphology_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_morphology_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_morphology_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_morphology_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_morphology_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_morphology_finalize(GObject * object);

GST_BOILERPLATE (GstMorphology, gst_morphology, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanMorphology(GstMorphology *morphology) 
{
  if (morphology->cvBGR)        cvReleaseImageHeader(&morphology->cvBGR);
  if (morphology->cvBGR_out)    cvReleaseImageHeader(&morphology->cvBGR_out);
}

static void gst_morphology_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Morphology filter", "Filter/Effect/Video",
    "Apply a morphology filter to an image",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_morphology_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_morphology_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_morphology_debug, "morphology", 0, \
                           "morphology - Apply a morphology filter to an image");
}

static void gst_morphology_class_init(GstMorphologyClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_morphology_set_property;
  gobject_class->get_property = gst_morphology_get_property;
  gobject_class->finalize = gst_morphology_finalize;
  
  btrans_class->passthrough_on_same_caps = FALSE;
  //btrans_class->always_in_place = FALSE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_morphology_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_morphology_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_morphology_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_morphology_set_caps);

  g_object_class_install_property(gobject_class, PROP_OP, 
                                  g_param_spec_int("operator", "Operator",
                                                   "Operation (0=opening, 1=closing, 2=gradient, 3=top hat, 4=blackhat", 
                                                   0, 5, 2, (GParamFlags)G_PARAM_READWRITE ));
  g_object_class_install_property(gobject_class, PROP_ITERATIONS, 
                                  g_param_spec_int("iters", "Amount of iterations",
                                                   "Amount of iterations", 
                                                   1, 100, 1, (GParamFlags)G_PARAM_READWRITE ));
}

static void gst_morphology_init(GstMorphology * morphology, GstMorphologyClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)morphology, TRUE);
  g_static_mutex_init(&morphology->lock);
  morphology->cvBGR      = NULL;
  morphology->cvBGR_out  = NULL;
  morphology->iterations = 1;
  morphology->op         = CV_MOP_GRADIENT;
}

static void gst_morphology_finalize(GObject * object) 
{
  GstMorphology *morphology = GST_MORPHOLOGY (object);
  
  GST_MORPHOLOGY_LOCK (morphology);
  CleanMorphology(morphology);
  GST_MORPHOLOGY_UNLOCK (morphology);
  GST_INFO("Morphology destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&morphology->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_morphology_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstMorphology *morphology = GST_MORPHOLOGY (object);
  
  GST_MORPHOLOGY_LOCK (morphology);
  switch (prop_id) {
  case PROP_OP:
    morphology->op = g_value_get_int(value);
    switch (morphology->op){
    case 0: morphology->op = CV_MOP_OPEN;     break;
    case 1: morphology->op = CV_MOP_CLOSE;    break;
    case 2: morphology->op = CV_MOP_GRADIENT; break;
    case 3: morphology->op = CV_MOP_TOPHAT;   break;
    case 4: morphology->op = CV_MOP_BLACKHAT; break;
    }  
    break;    
  case PROP_ITERATIONS:
    morphology->iterations = g_value_get_int(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_MORPHOLOGY_UNLOCK (morphology);
}

static void gst_morphology_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstMorphology *morphology = GST_MORPHOLOGY (object);

  switch (prop_id) { 
  case PROP_OP:
    g_value_set_int(value, morphology->op);
    break;    
  case PROP_ITERATIONS:
    g_value_set_int(value, morphology->iterations);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_morphology_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_morphology_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstMorphology *morphology = GST_MORPHOLOGY (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_MORPHOLOGY_LOCK (morphology);
  
  gst_video_format_parse_caps(incaps, &morphology->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &morphology->out_format, &out_width, &out_height);

  if (!(morphology->in_format == morphology->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_MORPHOLOGY_UNLOCK (morphology);
    return FALSE;
  }
  
  morphology->width  = in_width;
  morphology->height = in_height;
  
  GST_INFO("Initialising Morphology...");

  const CvSize size = cvSize(morphology->width, morphology->height);
  GST_WARNING (" width %d, height %d", morphology->width, morphology->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB  ////////////////////////////////////////////
  morphology->cvBGR     = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);
  morphology->cvBGR_out = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);

  GST_INFO("Morphology initialized.");
  
  GST_MORPHOLOGY_UNLOCK (morphology);
  
  return TRUE;
}

static void gst_morphology_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_morphology_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstMorphology *morphology = GST_MORPHOLOGY (btrans);

  GST_MORPHOLOGY_LOCK (morphology);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR
  morphology->cvBGR->imageData = (char*)GST_BUFFER_DATA(gstbuf);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
  cvMorphologyEx( morphology->cvBGR, morphology->cvBGR, NULL, NULL, 
                   morphology->op, morphology->iterations);

  //////////////////////////////////////////////////////////////////////////////
  GST_MORPHOLOGY_UNLOCK (morphology);  
  
  return GST_FLOW_OK;
}


gboolean gst_morphology_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "morphology", GST_RANK_NONE, GST_TYPE_MORPHOLOGY);
}
