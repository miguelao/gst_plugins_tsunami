/* GStreamer
 */
/**
 * SECTION:element- vibe
 *
 * This element takes a RGB image, and applies the vibe fg/bg classification
 * with a Y-normalisation pre-step.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstvibe.h"

GST_DEBUG_CATEGORY_STATIC (gst_vibe_debug);
#define GST_CAT_DEFAULT gst_vibe_debug


enum {
	PROP_0,
	PROP_DISPLAY,
	PROP_NORM,
	PROP_LAST
};

static GstStaticPadTemplate gst_vibe_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_vibe_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_VIBE_LOCK(vibe) G_STMT_START { \
	GST_LOG_OBJECT (vibe, "Locking vibe from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&vibe->lock); \
	GST_LOG_OBJECT (vibe, "Locked vibe from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_VIBE_UNLOCK(vibe) G_STMT_START { \
	GST_LOG_OBJECT (vibe, "Unlocking vibe from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&vibe->lock); \
} G_STMT_END

static gboolean gst_vibe_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_vibe_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_vibe_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_vibe_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_vibe_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_vibe_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_vibe_finalize(GObject * object);

static void moulay_y_prenorm(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean);
static void moulay_y_postnorm(IplImage* img) ;


GST_BOILERPLATE (GstVibe, gst_vibe, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanVibe(GstVibe *vibe) 
{
  if (vibe->cvBGRA)    cvReleaseImageHeader(&vibe->cvBGRA);
  if (vibe->cvBGR)     cvReleaseImageHeader(&vibe->cvBGR);
}

static void gst_vibe_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Vibe filter (EXPERIMENTAL)", "Filter/Effect/Video",
    "Vibe algorithm for FG/BG classification - with Y-normalisation",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_vibe_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_vibe_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_vibe_debug, "vibe", 0, \
                           "vibe - vibe algorithm for FG/BG tracking");
}

static void gst_vibe_class_init(GstVibeClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_vibe_set_property;
  gobject_class->get_property = gst_vibe_get_property;
  gobject_class->finalize = gst_vibe_finalize;
  
  btrans_class->passthrough_on_same_caps = FALSE;
  //btrans_class->always_in_place = FALSE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_vibe_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_vibe_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_vibe_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_vibe_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output would be the actual bg/fg model", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, 
                                  PROP_NORM, g_param_spec_boolean(
                                  "norm", "norm",
                                  "if set, the input will be pre-y-normalised", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_vibe_init(GstVibe * vibe, GstVibeClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)vibe, TRUE);
  g_static_mutex_init(&vibe->lock);
  vibe->cvBGR      = NULL;
  vibe->cvBGR      = NULL;
  vibe->display    = false;
  vibe->norm       = true;
}

static void gst_vibe_finalize(GObject * object) 
{
  GstVibe *vibe = GST_VIBE (object);
  
  GST_VIBE_LOCK (vibe);
  CleanVibe(vibe);
  GST_VIBE_UNLOCK (vibe);
  GST_INFO("Vibe destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&vibe->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_vibe_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstVibe *vibe = GST_VIBE (object);
  
  GST_VIBE_LOCK (vibe);
  switch (prop_id) {
  case PROP_DISPLAY:
    vibe->display = g_value_get_boolean(value);
    break;    
  case PROP_NORM:
    vibe->norm = g_value_get_boolean(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_VIBE_UNLOCK (vibe);
}

static void gst_vibe_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstVibe *vibe = GST_VIBE (object);

  switch (prop_id) { 
  case PROP_DISPLAY:
    g_value_set_boolean(value, vibe->display);
    break;    
  case PROP_NORM:
    g_value_set_boolean(value, vibe->norm);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_vibe_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_vibe_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstVibe *vibe = GST_VIBE (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_VIBE_LOCK (vibe);
  
  gst_video_format_parse_caps(incaps, &vibe->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &vibe->out_format, &out_width, &out_height);

  if (!(vibe->in_format == vibe->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_VIBE_UNLOCK (vibe);
    return FALSE;
  }
  
  vibe->width  = in_width;
  vibe->height = in_height;
  
  GST_INFO("Initialising Vibe...");

  const CvSize size = cvSize(vibe->width, vibe->height);
  GST_WARNING (" width %d, height %d", vibe->width, vibe->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB  ////////////////////////////////////////////
  vibe->cvBGRA     = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);
  vibe->cvBGR      = cvCreateImage(size, IPL_DEPTH_8U, 3);

  vibe->cvYUV      = cvCreateImage(size, IPL_DEPTH_8U, 3);
  vibe->cvYUVA     = cvCreateImage(size, IPL_DEPTH_8U, 4);

  vibe->chA        = cvCreateImage(size, IPL_DEPTH_8U, 1);
  vibe->ch1        = cvCreateImage(size, IPL_DEPTH_8U, 1);
  vibe->ch2        = cvCreateImage(size, IPL_DEPTH_8U, 1);
  vibe->ch3        = cvCreateImage(size, IPL_DEPTH_8U, 1);

  vibe->vibe_nsamples = 6;
  vibe->pvibe      = vibe_create(vibe->width, vibe->height, vibe->vibe_nsamples);
  memset(vibe->pvibe->conf, 0, vibe->pvibe->width*vibe->pvibe->height*sizeof(t_u_int8));
  vibe->nframes = 0;

  GST_INFO("Vibe initialized.");
  
  GST_VIBE_UNLOCK (vibe);
  
  return TRUE;
}

static void gst_vibe_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_vibe_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstVibe *vibe = GST_VIBE (btrans);

  GST_VIBE_LOCK (vibe);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGRA
  vibe->cvBGRA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(vibe->cvBGRA,  vibe->cvBGR, CV_BGRA2BGR);
  // internally we are working on YUVA, so we compose one with an empty A-channel
  cvCvtColor(vibe->cvBGR,  vibe->cvYUV, CV_BGR2YCrCb);
  cvSplit(vibe->cvYUV, vibe->ch1, vibe->ch2, vibe->ch3, NULL);
  cvZero(vibe->chA);

  //////////////////////////////////////////////////////////////////////////////
  // normalize the Y component, if enabled
  if( vibe->norm){
    moulay_y_prenorm((uint8_t*)vibe->ch1->imageData, vibe->ch1->width, vibe->ch1->height, 1, 85);
    moulay_y_postnorm(vibe->ch1);
  }

  cvMerge( vibe->ch1, vibe->ch2, vibe->ch3, vibe->chA, vibe->cvYUVA);


  //////////////////////////////////////////////////////////////////////////////
  // here goes the business logic
  //////////////////////////////////////////////////////////////////////////////
  // not too sure if vibe wants RGBA or YUVA or the no-A versions...
  // in this imlpementation, mask is in the 4th channel
  if( vibe->nframes == 1){
    //vibe_initmodel(vibe->pvibe, vibe->cvYUVA);
  }
  if( vibe->nframes <= 10){
    //vibe_initmodel(vibe->pvibe, vibe->cvYUVA);
    vibe_update(vibe->pvibe, vibe->cvYUVA);

  }
  else{
    vibe_segment(vibe->pvibe, vibe->cvYUVA);
    vibe_update(vibe->pvibe, vibe->cvYUVA);
  }
  vibe->nframes++;
  //////////////////////////////////////////////////////////////////////////////


  cvSplit(vibe->cvYUVA, vibe->ch1, vibe->ch2, vibe->ch3, vibe->chA);

  cvThreshold( vibe->chA, vibe->chA, 210, 255, CV_THRESH_BINARY);
  //cvErode ( vibe->chA, vibe->chA, NULL, 1);
  //cvDilate( vibe->chA, vibe->chA, NULL, 2);
  cvErode( vibe->chA, vibe->chA, cvCreateStructuringElementEx(5, 5, 3, 3, CV_SHAPE_RECT,NULL), 1);
  cvDilate(vibe->chA, vibe->chA, cvCreateStructuringElementEx(5, 5, 3, 3, CV_SHAPE_RECT,NULL), 2);
  cvErode( vibe->chA, vibe->chA, cvCreateStructuringElementEx(5, 5, 3, 3, CV_SHAPE_RECT,NULL), 1);
  
  if( !vibe->display ){
    cvSplit(vibe->cvBGRA, vibe->ch1, vibe->ch2, vibe->ch3, NULL);
    cvMerge(vibe->ch1, vibe->ch2, vibe->ch3, vibe->chA, vibe->cvBGRA);    
  }
  else{ 
    cvMerge(vibe->chA, vibe->chA, vibe->chA, NULL, vibe->cvBGRA);
  }

  GST_VIBE_UNLOCK (vibe);  
  
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






gboolean gst_vibe_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "vibe", GST_RANK_NONE, GST_TYPE_VIBE);
}
