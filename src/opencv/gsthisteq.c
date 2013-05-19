/* GStreamer
 */
/**
 * SECTION:element- histeq
 *
 * This element equalises the histogram by independently equalising the 3
 * colours components
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gsthisteq.h"
#include "../retinex/retinex.h"

GST_DEBUG_CATEGORY_STATIC (gst_histeq_debug);
#define GST_CAT_DEFAULT gst_histeq_debug


enum {
	PROP_0,
        PROP_METHOD,
	PROP_LAST
};

static GstStaticPadTemplate gst_histeq_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_histeq_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_HISTEQ_LOCK(histeq) G_STMT_START { \
	GST_LOG_OBJECT (histeq, "Locking histeq from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&histeq->lock); \
	GST_LOG_OBJECT (histeq, "Locked histeq from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_HISTEQ_UNLOCK(histeq) G_STMT_START { \
	GST_LOG_OBJECT (histeq, "Unlocking histeq from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&histeq->lock); \
} G_STMT_END

static gboolean gst_histeq_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_histeq_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_histeq_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_histeq_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_histeq_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_histeq_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_histeq_finalize(GObject * object);

static void moulay_y_prenorm(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean);
static void moulay_y_postnorm(IplImage* img) ;

GST_BOILERPLATE (GstHisteq, gst_histeq, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanHisteq(GstHisteq *histeq) 
{
  if (histeq->cvYUV)  cvReleaseImageHeader(&histeq->cvYUV);
  if (histeq->cvRGB)  cvReleaseImageHeader(&histeq->cvRGB);
}

static void gst_histeq_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, 
    "Y-Histogram Equalisation in Colour Images OpenCV filter", "Filter/Effect/Video",
    "Equalises Luminance",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_histeq_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_histeq_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_histeq_debug, "histeq", 0, \
                           "histeq - Equalises luminance of a colour image");
}

static void gst_histeq_class_init(GstHisteqClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_histeq_set_property;
  gobject_class->get_property = gst_histeq_get_property;
  gobject_class->finalize = gst_histeq_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_histeq_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_histeq_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_histeq_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_histeq_set_caps);

  g_object_class_install_property(gobject_class, PROP_METHOD, g_param_spec_int(
    "method", "Method to use (0-Y-channel histogram norm; 1-Moulay's global Y norm; 2-Retinex RGB)",
    "Method to use (0-Y-channel histogram norm; 1-Moulay's global Y norm; 2-Retinex RGB)", 0, 2, 0, 
    (GParamFlags)(G_PARAM_READWRITE)));
}

static void gst_histeq_init(GstHisteq * histeq, GstHisteqClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)histeq, TRUE);
  g_static_mutex_init(&histeq->lock);
  histeq->cvYUV     = NULL;
  histeq->cvRGB     = NULL;
  histeq->method    = 0;
}

static void gst_histeq_finalize(GObject * object) 
{
  GstHisteq *histeq = GST_HISTEQ (object);
  
  GST_HISTEQ_LOCK (histeq);
  CleanHisteq(histeq);
  GST_HISTEQ_UNLOCK (histeq);
  GST_INFO("Histeq destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&histeq->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_histeq_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstHisteq *histeq = GST_HISTEQ (object);
  
  GST_HISTEQ_LOCK (histeq);
  switch (prop_id) {
    
  case PROP_METHOD:
    histeq->method = g_value_get_int(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_HISTEQ_UNLOCK (histeq);
}

static void gst_histeq_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstHisteq *histeq = GST_HISTEQ (object);

  switch (prop_id) { 
  case PROP_METHOD:
    g_value_set_int(value, histeq->method);
    break;  
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_histeq_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}

static gboolean gst_histeq_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstHisteq *histeq = GST_HISTEQ (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_HISTEQ_LOCK (histeq);
  
  gst_video_format_parse_caps(incaps, &histeq->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &histeq->out_format, &out_width, &out_height);
  if (!(histeq->in_format == histeq->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_HISTEQ_UNLOCK (histeq);
    return FALSE;
  }
  
  histeq->width  = in_width;
  histeq->height = in_height;
  
  GST_INFO("Initialising Histeq...");

  const CvSize size = cvSize(histeq->width, histeq->height);
  GST_WARNING (" width %d, height %d", histeq->width, histeq->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB /////////////////////////////////////////////
  histeq->cvRGBA = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);
  histeq->cvRGB = cvCreateImage(size, IPL_DEPTH_8U, 3);

  histeq->ch1    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  histeq->ch2    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  histeq->ch3    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  histeq->chA    = cvCreateImage(size, IPL_DEPTH_8U, 1);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in YUV or similar //////////////////////////////////
  // fully allocated, as opposed to RGBA, which data we take from gstreamer /////
  histeq->cvYUV = cvCreateImage(size, IPL_DEPTH_8U, 3);

  // structures for each channel before and after equalisation
  histeq->im_y = cvCreateImage( cvGetSize(histeq->cvYUV), 8, 1 );
  histeq->im_u = cvCreateImage( cvGetSize(histeq->cvYUV), 8, 1 );
  histeq->im_v = cvCreateImage( cvGetSize(histeq->cvYUV), 8, 1 );
  histeq->eq_im_y = cvCreateImage( cvGetSize(histeq->cvYUV), 8, 1 );
  histeq->eq_im_v = cvCreateImage( cvGetSize(histeq->cvYUV), 8, 1 );
  histeq->eq_im_u = cvCreateImage( cvGetSize(histeq->cvYUV), 8, 1 );

  GST_INFO("Histeq initialized.");
  
  GST_HISTEQ_UNLOCK (histeq);
  
  return TRUE;
}

static void gst_histeq_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_histeq_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstHisteq *histeq = GST_HISTEQ (btrans);

  GST_HISTEQ_LOCK (histeq);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is YUV already
  histeq->cvRGBA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(histeq->cvRGBA, histeq->cvRGB, CV_RGBA2RGB);

  cvCvtColor( histeq->cvRGB, histeq->cvYUV, CV_RGB2YUV );

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////

  // separate the input colours into individual IplImages
  cvSplit( histeq->cvYUV, histeq->im_y, histeq->im_u, histeq->im_v, NULL);

    double sigma = 14.0;
    int gain = 128;
    int offset = 128;

  switch( histeq->method ){        
  case 0 :  /// Y-channel histogram normalisation
    cvEqualizeHist(histeq->im_y, histeq->eq_im_y);
    break;
  case 1:  // Moulay's global Y normalisation
    moulay_y_prenorm((uint8_t*)histeq->im_y->imageData, histeq->width, histeq->height, 1, 85);
    //moulay_y_postnorm(histeq->im_y);
    cvCopy( histeq->im_y, histeq->eq_im_y);
    break;
  case 2:  // Retinex normalisation (?)
    Retinex( histeq->cvRGB, sigma, gain, offset );
    cvCvtColor( histeq->cvRGB, histeq->cvYUV, CV_RGB2YUV );
    cvSplit( histeq->cvYUV, histeq->eq_im_y, histeq->im_u, histeq->im_v, NULL);
    break;
  default:
    cvCopy( histeq->im_y, histeq->eq_im_y);
    break;
  }

  // recompose the equalised image with the equalised channels
  cvMerge( histeq->eq_im_y, histeq->im_u,  histeq->im_v, NULL,
           histeq->cvYUV);

  //////////////////////////////////////////////////////////////////////////////
  // normally here we would convert back RGB to YUV, overwriting the input
  // of this plugin element, which goes directly to the output
  cvCvtColor( histeq->cvYUV, histeq->cvRGB, CV_YUV2RGB );

  // copy the RGB to the RGB of the output image and leave alpha same
  cvSplit(histeq->cvRGB , histeq->ch1, histeq->ch2, histeq->ch3, NULL);
  cvSplit(histeq->cvRGBA, NULL,        NULL,        NULL,        histeq->chA);
  cvMerge(                histeq->ch1, histeq->ch2, histeq->ch3, histeq->chA,    histeq->cvRGBA);
 
 
  GST_HISTEQ_UNLOCK (histeq);  
  
  return GST_FLOW_OK;
}

////////////////////////////////////////////////////////////////////////////////
// W=width, H=height, C=number of channels (byte), 
// y_mean: y threshold not to touch the image
////////////////////////////////////////////////////////////////////////////////
void moulay_y_prenorm(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean) 
{
  
  const uint32_t S = W * H;
  
  uint32_t mean_global = 0;
  for (uint32_t k = 0; k < S; ++k)
    mean_global += data[k*C];
  mean_global /= S;
  if (mean_global == 0)
    return;
  
  if (y_mean > mean_global) {
    for (uint32_t k = 0; k < S; ++k) {
      uint8_t& val = data[k*C];
      const uint32_t tmp = (y_mean * val) / mean_global;
      if (tmp > 255)
        val = 255;
      else
        val = (uint8_t)tmp;
    }
  } else {
    for (uint32_t k = 0; k < S; ++k) {
      uint8_t& val = data[k*C];
      val = (y_mean * val) / mean_global;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Originally an openCL kernel, hence the /256.0 at the beginning and the 
// *255.0 at the end. (Might not be needed?)
////////////////////////////////////////////////////////////////////////////////
void moulay_y_postnorm(IplImage* img) 
{
#define INTERP(x,y,w) ((x)+(1/(w))*((y)-(x)))
  
  //uint32_t mean_global = 0;

  float val, hp1, hn1, vp1, vn1, mean, vmax, vmin, mean_mul_max, dmx_mean;

  for( int row=1; row<img->height-1; row++){    // rows
    for( int col=1; col<img->width-1; col++){   // columns

      val = cvGetReal2D( img, row, col)     /256.0;
      hp1 = cvGetReal2D( img, row + 1, col )/256.0;
      hn1 = cvGetReal2D( img, row - 1, col )/256.0;
      vp1 = cvGetReal2D( img, row , col + 1)/256.0;
      vn1 = cvGetReal2D( img, row , col + 1)/256.0;

      // interpolations
      hp1 = INTERP(val, hp1, img->width);
      hn1 = INTERP(val, hn1, img->width);
      vp1 = INTERP(val, vp1, img->height);
      vn1 = INTERP(val, vn1, img->height);

      mean = (2*val + hp1 + hn1 + vp1 + vn1)/6.0f;

      vmax = MAX(hn1, MAX( val, MAX( hp1, MAX( vp1, vn1))));
      vmin = MIN(hn1, MIN( val, MIN( hp1, MIN( vp1, vn1))));

      mean_mul_max = vmax * mean * 64.0f;
      dmx_mean = vmax - vmin;
      //printf(" %f ", 255 * (val + 8.0f * dmx_mean) / mean_mul_max);
      cvSetReal2D( img, row, col,
                   256 * ((val + 8.0f * dmx_mean) / mean_mul_max) );
    }
  }


}



gboolean gst_histeq_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "histeq", GST_RANK_NONE, GST_TYPE_HISTEQ);
}
