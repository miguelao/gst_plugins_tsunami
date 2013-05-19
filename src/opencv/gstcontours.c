/* GStreamer
 */
/**
 * SECTION:element- contours
 *
 * This element takes a Gray image, which is supposed to be
 * actually 2-valued, and calculates the contours inside. Draws them too.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstcontours.h"

GST_DEBUG_CATEGORY_STATIC (gst_contours_debug);
#define GST_CAT_DEFAULT gst_contours_debug


enum {
	PROP_0,
	PROP_DISPLAY,
	PROP_HISTEQ,
	PROP_MINAREA,
	PROP_LAST
};

static GstStaticPadTemplate gst_contours_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-gray")
);
static GstStaticPadTemplate gst_contours_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-gray")
);

#define GST_CONTOURS_LOCK(contours) G_STMT_START { \
	GST_LOG_OBJECT (contours, "Locking contours from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&contours->lock); \
	GST_LOG_OBJECT (contours, "Locked contours from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_CONTOURS_UNLOCK(contours) G_STMT_START { \
	GST_LOG_OBJECT (contours, "Unlocking contours from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&contours->lock); \
} G_STMT_END

static gboolean gst_contours_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_contours_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_contours_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_contours_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_contours_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_contours_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_contours_finalize(GObject * object);

GST_BOILERPLATE (GstContours, gst_contours, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanContours(GstContours *contours) 
{
  if (contours->cvGRAY)       cvReleaseImageHeader(&contours->cvGRAY);
  if (contours->cvGRAY_copy)  cvReleaseImageHeader(&contours->cvGRAY_copy);
}

static void gst_contours_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Contours filter", "Filter/Effect/Video",
    "Calculates and maybe draws the contours of a 2-value image",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_contours_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_contours_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_contours_debug, "contours", 0, \
                           "contours - Calculate the contours of a 2-value image");
}

static void gst_contours_class_init(GstContoursClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_contours_set_property;
  gobject_class->get_property = gst_contours_get_property;
  gobject_class->finalize = gst_contours_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = FALSE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_contours_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_contours_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_contours_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_contours_set_caps);

  g_object_class_install_property(gobject_class, PROP_DISPLAY, 
                                  g_param_spec_boolean("display", "Display",
                                                       "Draw contours on output", TRUE, 
                                                       (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_HISTEQ, 
                                  g_param_spec_boolean("histeq", "Histogram Eq",
                                                       "Perform or not histogram equalisation before contour", TRUE, 
                                                       (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_MINAREA, 
                                  g_param_spec_double("minarea", "Min contour area",
                                                      "Minimum contour area to be considered as such", 
                                                      1, 100000, 20.0, (GParamFlags)G_PARAM_READWRITE ));
}

static void gst_contours_init(GstContours * contours, GstContoursClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)contours, TRUE);
  g_static_mutex_init(&contours->lock);
  contours->cvGRAY      = NULL;
  contours->cvGRAY_copy = NULL;
  contours->display     = 0;
  contours->histeq      = 1;

  contours->storage     = cvCreateMemStorage();
  contours->contour     = NULL;
  contours->mode        = CV_RETR_TREE;//CV_RETR_EXTERNAL;
  contours->minarea     = 20.0;
}

static void gst_contours_finalize(GObject * object) 
{
  GstContours *contours = GST_CONTOURS (object);
  
  GST_CONTOURS_LOCK (contours);
  CleanContours(contours);
  GST_CONTOURS_UNLOCK (contours);
  GST_INFO("Contours destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&contours->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_contours_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstContours *contours = GST_CONTOURS (object);
  
  GST_CONTOURS_LOCK (contours);
  switch (prop_id) {
  case PROP_DISPLAY:
    contours->display = g_value_get_boolean(value);
    break;    
  case PROP_HISTEQ:
    contours->histeq = g_value_get_boolean(value);
    break;    
  case PROP_MINAREA:
    contours->minarea = g_value_get_double(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_CONTOURS_UNLOCK (contours);
}

static void gst_contours_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstContours *contours = GST_CONTOURS (object);

  switch (prop_id) { 
  case PROP_DISPLAY:
    g_value_set_boolean(value, contours->display);
    break;
  case PROP_HISTEQ:
    g_value_set_boolean(value, contours->histeq);
    break;    
  case PROP_MINAREA:
    g_value_set_double(value, contours->minarea);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_contours_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_contours_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstContours *contours = GST_CONTOURS (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_CONTOURS_LOCK (contours);
  
  gst_video_format_parse_caps(incaps, &contours->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &contours->out_format, &out_width, &out_height);

  if (!(contours->in_format == contours->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_CONTOURS_UNLOCK (contours);
    return FALSE;
  }
  
  contours->width  = in_width;
  contours->height = in_height;
  
  GST_INFO("Initialising Contours...");

  const CvSize size = cvSize(contours->width, contours->height);
  GST_WARNING (" width %d, height %d", contours->width, contours->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB  ////////////////////////////////////////////
  contours->cvGRAY = cvCreateImageHeader(size, IPL_DEPTH_8U, 1);
  // fully allocate the copy
  contours->cvGRAY_copy = cvCreateImage(size, IPL_DEPTH_8U, 1);

  GST_INFO("Contours initialized.");
  
  GST_CONTOURS_UNLOCK (contours);
  
  return TRUE;
}

static void gst_contours_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_contours_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstContours *contours = GST_CONTOURS (btrans);

  GST_CONTOURS_LOCK (contours);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is RGB
  contours->cvGRAY->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCopy( contours->cvGRAY, contours->cvGRAY_copy, NULL);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////

  if( contours->histeq)
    cvEqualizeHist(contours->cvGRAY_copy, contours->cvGRAY_copy);

  cvCanny(contours->cvGRAY_copy, contours->cvGRAY_copy ,10, 150);

  // find the contours
  int Nc =
   cvFindContours( contours->cvGRAY_copy,
                   contours->storage,
                   &(contours->contour),
                   sizeof(CvContour),
                   contours->mode,
                   CV_CHAIN_APPROX_SIMPLE,
                   cvPoint(0,0));

// if( contours->display )
//   cvDrawContours( contours->cvGRAY,
//                   contours->contour,
//                   CV_RGB(0,255,0),
//                   CV_RGB(255,0,0),
//                   2, 2, 8);

  int    large_contours=0;
  CvSeq* current_contour = contours->contour;
  while (current_contour != NULL){
    double area = fabs(cvContourArea(current_contour, CV_WHOLE_SEQ, false));       
    if( area > contours->minarea){
      large_contours++;
      if( contours->display )
        cvDrawContours( contours->cvGRAY,
                        current_contour,
                        CV_RGB(0,255,0),
                        CV_RGB(255,0,0),
                        0, 2, -8);
    }
    current_contour = current_contour->h_next;
  }

  printf(" total contours detected %.5d, large ones: %.5d\n", Nc, large_contours);

  //////////////////////////////////////////////////////////////////////////////
  GST_CONTOURS_UNLOCK (contours);  
  
  return GST_FLOW_OK;
}


gboolean gst_contours_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "contours", GST_RANK_NONE, GST_TYPE_CONTOURS);
}
