/* GStreamer
 */
/**
 * SECTION:element- snakes
 *
 * This element tries to wrap a Snake around the input values
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstsnakes.h"
#include <opencv2/legacy/legacy.hpp> //CV_VALUE

GST_DEBUG_CATEGORY_STATIC (gst_snakes_debug);
#define GST_CAT_DEFAULT gst_snakes_debug


enum {
	PROP_0,
	PROP_ALPHA,
	PROP_BETA,
	PROP_GAMMA,
	PROP_DISPLAY,
        PROP_METHOD,
	PROP_LAST
};

#define METHOD_HSV 0
#define METHOD_RGB 1

static GstStaticPadTemplate gst_snakes_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_snakes_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_SNAKES_LOCK(snakes) G_STMT_START { \
	GST_LOG_OBJECT (snakes, "Locking snakes from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&snakes->lock); \
	GST_LOG_OBJECT (snakes, "Locked snakes from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_SNAKES_UNLOCK(snakes) G_STMT_START { \
	GST_LOG_OBJECT (snakes, "Unlocking snakes from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&snakes->lock); \
} G_STMT_END

static gboolean gst_snakes_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_snakes_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_snakes_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_snakes_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_snakes_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_snakes_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_snakes_finalize(GObject * object);
static gint gstsnakes_reset_points_array(struct _GstSnakes *snakes);

GST_BOILERPLATE (GstSnakes, gst_snakes, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanSnakes(GstSnakes *snakes) 
{
  if (snakes->cvRGB)  cvReleaseImageHeader(&snakes->cvRGB);
}

static void gst_snakes_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Snakes construction filter", "Filter/Effect/Video",
    "Element tries to wrap a snake around the input values; best if these values are B/W",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_snakes_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_snakes_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_snakes_debug, "snakes", 0, \
                           "snakes - Wraps a snake around input");
}

static void gst_snakes_class_init(GstSnakesClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_snakes_set_property;
  gobject_class->get_property = gst_snakes_get_property;
  gobject_class->finalize = gst_snakes_finalize;
  
  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output RGB will be the snakes overinposed on output", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_ALPHA, 
                                  g_param_spec_float("alpha", "alpha",
                                  "Sets the alpha value", 
                                  0.0, 100.0, 0.0, (GParamFlags)(G_PARAM_READWRITE)));
  g_object_class_install_property(gobject_class, PROP_BETA, 
                                  g_param_spec_float("beta", "beta",
                                  "Sets the beta value", 
                                  0.0, 100.0, 0.0, (GParamFlags)(G_PARAM_READWRITE)));
  g_object_class_install_property(gobject_class, PROP_GAMMA, 
                                  g_param_spec_float("gamma", "gamma",
                                  "Sets the gamma value", 
                                  0.0, 100.0, 0.0, (GParamFlags)(G_PARAM_READWRITE)));
  g_object_class_install_property(gobject_class, PROP_METHOD, g_param_spec_int(
                                    "method", "Move to edges or shift via image intensity (0-intensity; 1-edges)",
                                    "Move to edges or shift via image intensity (0-intensity; 1-edges)", 0, 1, 0, 
                                    (GParamFlags)(G_PARAM_READWRITE)));

  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_snakes_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_snakes_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_snakes_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_snakes_set_caps);
}

static void gst_snakes_init(GstSnakes * snakes, GstSnakesClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)snakes, TRUE);
  g_static_mutex_init(&snakes->lock);
  snakes->cvRGB     = NULL;

  snakes->display    = false;

  // alpha: Controls tension, beta: Controls rigidity, gamma: Step size
  //When the alpha value decreases, convergence of elasticity will decrease, and
  // if the alpha value increases, elasticity will increase and the vector field
  // will flexibly converge even in large areas. However, if the alpha is too 
  // large, the transformation cannot form the optimum image result, and the
  // edge will excessively converge so that no edge can be found.
  //Beta denotes the hardness of edge values, or resistance to bending (plaque).

  // Mihai Fagadar:
  // A good discussion with lost of examples can be found in [Zhang, 2001]: 
  // L. Zhang, “Active Contour Model, Snake”, Technical report, University of 
  // Nevada, United States, 2001. 
  snakes->alpha      = 0.45;
  snakes->beta       = 0.65;
  snakes->gamma      = 0.50;
  // attract the snake to image features, like borders (calc_gradient=1) 
  // or by the image intensity itself (calc_gradient=0)
  snakes->method     = 1;
}

static void gst_snakes_finalize(GObject * object) 
{
  GstSnakes *snakes = GST_SNAKES (object);
  
  GST_SNAKES_LOCK (snakes);
  CleanSnakes(snakes);
  GST_SNAKES_UNLOCK (snakes);
  GST_INFO("Snakes destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&snakes->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_snakes_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstSnakes *snakes = GST_SNAKES (object);
  
  GST_SNAKES_LOCK (snakes);
  switch (prop_id) {
  case PROP_DISPLAY:
    snakes->display = g_value_get_boolean(value);
    break;
  case PROP_ALPHA:
    snakes->alpha = g_value_get_float(value);
    break;
  case PROP_BETA:
    snakes->beta = g_value_get_float(value);
    break;
  case PROP_GAMMA:
    snakes->gamma = g_value_get_float(value);
    break;
  case PROP_METHOD:
    snakes->method = g_value_get_int(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_SNAKES_UNLOCK (snakes);
}

static void gst_snakes_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstSnakes *snakes = GST_SNAKES (object);

  switch (prop_id) { 
  case PROP_DISPLAY:
    g_value_set_boolean(value, snakes->display);
    break;
  case PROP_ALPHA:
    g_value_set_float(value, snakes->alpha);
    break;  
  case PROP_BETA:
    g_value_set_float(value, snakes->beta);
    break;  
  case PROP_GAMMA:
    g_value_set_float(value, snakes->gamma);
    break;  
  case PROP_METHOD:
    g_value_set_int(value, snakes->method);
    break;  
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_snakes_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) 
{
  GstVideoFormat format;
  gint width, height;
  
  if (!gst_video_format_parse_caps(caps, &format, &width, &height))
    return FALSE;
  
  *size = gst_video_format_get_size(format, width, height);
  
  GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);
  
  return TRUE;
}


static gboolean gst_snakes_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstSnakes *snakes = GST_SNAKES (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_SNAKES_LOCK (snakes);
  
  gst_video_format_parse_caps(incaps, &snakes->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &snakes->out_format, &out_width, &out_height);
  if (!(snakes->in_format == snakes->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_SNAKES_UNLOCK (snakes);
    return FALSE;
  }
  
  snakes->width  = in_width;
  snakes->height = in_height;
  
  GST_INFO("Initialising Snakes...");

  const CvSize size = cvSize(snakes->width, snakes->height);
  GST_WARNING (" width %d, height %d", snakes->width, snakes->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB(A) //////////////////////////////////////////
  snakes->cvRGBA = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);
  snakes->cvRGB  = cvCreateImage(size, IPL_DEPTH_8U, 3);
  snakes->cvGRAY = cvCreateImage(size, IPL_DEPTH_8U, 1);

  snakes->ch1    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  snakes->ch2    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  snakes->ch3    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  snakes->chA    = cvCreateImage(size, IPL_DEPTH_8U, 1);

  // Note that the amount of points is 4xlength, since length refers to one image edge
  CvPoint* points;
  snakes->length = 10;
  snakes->points  = (CvPoint*)malloc(4*snakes->length*sizeof(CvPoint));
  
  // max 100 iterations, or epsilon 1.0
  snakes->criteria = cvTermCriteria( CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 100, 1.0 );

  snakes->size = cvSize( 11, 11);  
  // attract the snake to image features, like borders (calc_gradient=1) 
  // or by the image intensity itself (calc_gradient=0)
  snakes->calcGradient = snakes->method;


  gstsnakes_reset_points_array(snakes);

  snakes->nframe = 0;
  GST_INFO("Snakes initialized.");
  
  GST_SNAKES_UNLOCK (snakes);
  
  return TRUE;
}

static void gst_snakes_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_snakes_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstSnakes *snakes = GST_SNAKES (btrans);

  GST_SNAKES_LOCK (snakes);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR/RGB
  snakes->cvRGBA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(snakes->cvRGBA, snakes->cvRGB,  CV_RGBA2RGB);
  cvCvtColor(snakes->cvRGB , snakes->cvGRAY, CV_RGB2GRAY);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////

  // draw a line of 0's on the bottom of the image
  // this is necessary for a good convergence on items that extend to the bottom
  for( int x = 0; x < snakes->cvGRAY->width; x++ ){
    cvSet2D( snakes->cvGRAY, snakes->cvGRAY->height-1 , x, cvScalar(0) );
    cvSet2D( snakes->cvGRAY, snakes->cvGRAY->height-2 , x, cvScalar(0) );
    cvSet2D( snakes->cvGRAY, snakes->cvGRAY->height-3 , x, cvScalar(0) );
  }

  gstsnakes_reset_points_array(snakes);

  if( snakes->nframe )
    cvSnakeImage(snakes->cvGRAY, 
                 snakes->points, 4*snakes->length, 
                 &snakes->alpha, &snakes->beta, &snakes->gamma, CV_VALUE, 
                 snakes->size, 
                 snakes->criteria, 
                 snakes->calcGradient);



  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( snakes->display ){

    cvCvtColor(snakes->cvGRAY, snakes->cvRGB, CV_GRAY2RGB);

    for (int i = 0; i < 4*snakes->length-1; i++) {
      cvLine(snakes->cvRGB, 
             cvPoint(snakes->points[i].x,   snakes->points[i].y  ), 
             cvPoint(snakes->points[i+1].x, snakes->points[i+1].y), 
             cvScalar(255, 0 ,0), 1, 8, 0);
      //printf(" (%d, %d) -> (%d, %d) \n",snakes->points[i].x, snakes->points[i].y, snakes->points[i+1].x, snakes->points[i+1].y);
    }
    cvLine(snakes->cvRGB, 
           cvPoint(snakes->points[4*snakes->length-1].x,   snakes->points[4*snakes->length-1].y  ), 
           cvPoint(snakes->points[0].x,            snakes->points[0].y), 
           cvScalar(255,0, 0), 1, 8, 0);
    
    //printf("------- \n");
    cvSet(snakes->chA, cvScalar(255), NULL);
    cvSplit(snakes->cvRGB, snakes->ch1, snakes->ch2, snakes->ch3, NULL);
  }
  
  //////////////////////////////////////////////////////////////////////////////
  // copy the snakes output to the alpha channel in the output image
  if( !snakes->display ){
    cvSplit(snakes->cvRGBA, snakes->ch1, snakes->ch2, snakes->ch3, NULL);
  }

  cvMerge(snakes->ch1, snakes->ch2, snakes->ch3, snakes->chA,    snakes->cvRGBA);

  snakes->nframe++;
  GST_SNAKES_UNLOCK (snakes);  
  
  return GST_FLOW_OK;
}


gboolean gst_snakes_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "snakes", GST_RANK_NONE, GST_TYPE_SNAKES);
}







gint gstsnakes_reset_points_array(struct _GstSnakes *snakes)
{
  // fill in points around x, y
  for( int i=0; i<snakes->length; i++ ){
    snakes->points[i].x = (snakes->cvGRAY->width * i)/snakes->length;
    snakes->points[i].y = 0;
  }
  for( int i=0; i<snakes->length; i++ ){
    snakes->points[snakes->length+i].x = snakes->cvGRAY->width-1;
    snakes->points[snakes->length+i].y = (snakes->cvGRAY->height * i)/snakes->length;
  }
  for( int i=0; i<snakes->length; i++ ){
    snakes->points[2*snakes->length+i].x = (snakes->cvGRAY->width * (snakes->length-i) )/snakes->length -1;
    snakes->points[2*snakes->length+i].y = snakes->cvGRAY->height-1;
  }
  for( int i=0; i<snakes->length; i++ ){
    snakes->points[3*snakes->length+i].x = 0;
    snakes->points[3*snakes->length+i].y = (snakes->cvGRAY->height * (snakes->length-i))/snakes->length -1;
  }

  return 0;
}
