/* GStreamer
 */
/**
 * SECTION:element- skin
 *
 * This element gets the RGB (BGR in reality) input and outputs a BW skin map.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstskin.h"

GST_DEBUG_CATEGORY_STATIC (gst_skin_debug);
#define GST_CAT_DEFAULT gst_skin_debug


enum {
	PROP_0,
	PROP_SHOWH,
	PROP_SHOWS,
	PROP_SHOWV,
	PROP_ENABLE,
	PROP_DISPLAY,
        PROP_METHOD,
	PROP_LAST
};

#define METHOD_HSV 0
#define METHOD_RGB 1

static GstStaticPadTemplate gst_skin_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_skin_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_SKIN_LOCK(skin) G_STMT_START { \
	GST_LOG_OBJECT (skin, "Locking skin from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&skin->lock); \
	GST_LOG_OBJECT (skin, "Locked skin from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_SKIN_UNLOCK(skin) G_STMT_START { \
	GST_LOG_OBJECT (skin, "Unlocking skin from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&skin->lock); \
} G_STMT_END

static gboolean gst_skin_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_skin_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_skin_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_skin_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_skin_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_skin_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_skin_finalize(GObject * object);


gint gstskin_find_skin_center_of_mass(struct _GstSkin *skin, gint display);
gint gstskin_find_skin_center_of_mass2(struct _GstSkin *skin, gint display);

GST_BOILERPLATE (GstSkin, gst_skin, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanSkin(GstSkin *skin) 
{
  if (skin->cvRGB)  cvReleaseImageHeader(&skin->cvRGB);
}

static void gst_skin_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Skin colour filter", "Filter/Effect/Video",
    "Performs skin detection, outputting a black and white image of skin pixels",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_skin_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_skin_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_skin_debug, "skin", 0, \
                           "skin - Performs some image adjust operations");
}

static void gst_skin_class_init(GstSkinClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_skin_set_property;
  gobject_class->get_property = gst_skin_get_property;
  gobject_class->finalize = gst_skin_finalize;
  
  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output RGB will be the skin segmentation output", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_ENABLE, 
                                  g_param_spec_boolean("enable", "Enable",
                                  "Sets whether the skin colour detector is enabled or just passthrough", 
                                  TRUE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_SHOWH, 
                                  g_param_spec_boolean("showh", "showH",
                                  "Show as output only the thresholded H channel", 
                                  FALSE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_SHOWS, 
                                  g_param_spec_boolean("shows", "showS",
                                  "Show as output only the thresholded S channel", 
                                  FALSE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_SHOWV, 
                                  g_param_spec_boolean("showv", "showV",
                                  "Show as output only the thresholded V channel", 
                                  FALSE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_METHOD, g_param_spec_int(
                                    "method", "Method to use, meaning thresholds (0-HSV; 1-RGB)",
                                    "Method to use, meaning thresholds (0-HSV; 1-RGB)", 0, 1, 1, 
                                    (GParamFlags)(G_PARAM_READWRITE)));

  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_skin_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_skin_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_skin_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_skin_set_caps);
}

static void gst_skin_init(GstSkin * skin, GstSkinClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)skin, TRUE);
  g_static_mutex_init(&skin->lock);
  skin->cvRGB     = NULL;

  skin->display    = false;
  skin->enableskin = true;
  skin->showH      = false;
  skin->showS      = false;
  skin->showV      = false;
  skin->method     = 1;     // RGB by default!!!
}

static void gst_skin_finalize(GObject * object) 
{
  GstSkin *skin = GST_SKIN (object);
  
  GST_SKIN_LOCK (skin);
  CleanSkin(skin);
  GST_SKIN_UNLOCK (skin);
  GST_INFO("Skin destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&skin->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_skin_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstSkin *skin = GST_SKIN (object);
  
  GST_SKIN_LOCK (skin);
  switch (prop_id) {
  case PROP_DISPLAY:
    skin->display = g_value_get_boolean(value);
    break;
  case PROP_ENABLE:
    skin->enableskin = g_value_get_boolean(value);
    break;
  case PROP_SHOWH:
    skin->showH = g_value_get_boolean(value);
    break;
  case PROP_SHOWS:
    skin->showS = g_value_get_boolean(value);
    break;
  case PROP_SHOWV:
    skin->showV = g_value_get_boolean(value);
    break;
  case PROP_METHOD:
    skin->method = g_value_get_int(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_SKIN_UNLOCK (skin);
}

static void gst_skin_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstSkin *skin = GST_SKIN (object);

  switch (prop_id) { 
  case PROP_DISPLAY:
    g_value_set_boolean(value, skin->display);
    break;
  case PROP_ENABLE:
    g_value_set_boolean(value, skin->enableskin);
    break;  
  case PROP_SHOWH:
    g_value_set_boolean(value, skin->showH);
    break;  
  case PROP_SHOWS:
    g_value_set_boolean(value, skin->showS);
    break;  
  case PROP_SHOWV:
    g_value_set_boolean(value, skin->showV);
    break;  
  case PROP_METHOD:
    g_value_set_int(value, skin->method);
    break;  
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_skin_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) 
{
  GstVideoFormat format;
  gint width, height;
  
  if (!gst_video_format_parse_caps(caps, &format, &width, &height))
    return FALSE;
  
  *size = gst_video_format_get_size(format, width, height);
  
  GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);
  
  return TRUE;
}


static gboolean gst_skin_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstSkin *skin = GST_SKIN (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_SKIN_LOCK (skin);
  
  gst_video_format_parse_caps(incaps, &skin->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &skin->out_format, &out_width, &out_height);
  if (!(skin->in_format == skin->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_SKIN_UNLOCK (skin);
    return FALSE;
  }
  
  skin->width  = in_width;
  skin->height = in_height;
  
  GST_INFO("Initialising Skin...");

  const CvSize size = cvSize(skin->width, skin->height);
  GST_WARNING (" width %d, height %d", skin->width, skin->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB(A) //////////////////////////////////////////
  skin->cvRGBA = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);
  skin->cvRGB  = cvCreateImage(size, IPL_DEPTH_8U, 3);

  skin->ch1    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  skin->ch2    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  skin->ch3    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  skin->chA    = cvCreateImage(size, IPL_DEPTH_8U, 1);

  GST_INFO("Skin initialized.");
  
  GST_SKIN_UNLOCK (skin);
  
  return TRUE;
}

static void gst_skin_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_skin_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstSkin *skin = GST_SKIN (btrans);

  GST_SKIN_LOCK (skin);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is BGR/RGB
  skin->cvRGBA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(skin->cvRGBA, skin->cvRGB, CV_BGRA2BGR);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the bussiness logic
  //////////////////////////////////////////////////////////////////////////////
  ///////////// SKIN COLOUR BLOB FACE DETECTION/////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  if( skin->enableskin ) 
  {                                                            
    int display = 1;                                           
    if( METHOD_HSV == skin->method ){ // HSV
      gstskin_find_skin_center_of_mass( skin, display); 
    }
    else if( METHOD_RGB == skin->method ){ // RGB
      gstskin_find_skin_center_of_mass2( skin, display); 
    }
  }                                         
  //////////////////////////////////////////////////////////////////////////////
  // After this we have a RGB Black and white image with the skin, in skin->cvRGB
  // Just copy one channel of the RGB skin, which anyway has just values 255 or 0
  // and save it for later
  cvSplit(skin->cvRGB, skin->chA, NULL, NULL, NULL);

  cvErode( skin->chA, skin->chA, cvCreateStructuringElementEx(3,3, 1,1, CV_SHAPE_RECT,NULL), 1);
  cvDilate(skin->chA, skin->chA, cvCreateStructuringElementEx(3,3, 1,1, CV_SHAPE_RECT,NULL), 2);
  cvErode( skin->chA, skin->chA, cvCreateStructuringElementEx(3,3, 1,1, CV_SHAPE_RECT,NULL), 1);

  // copy the skin output to the alpha channel in the output image
  cvSplit(skin->cvRGBA, skin->ch1, skin->ch2, skin->ch3, NULL);
  cvMerge(skin->ch1, skin->ch2, skin->ch3, skin->chA,    skin->cvRGBA);
 
  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( skin->display ){
    cvCvtColor(skin->chA, skin->cvRGBA, CV_GRAY2RGB);
  }

  GST_SKIN_UNLOCK (skin);  
  
  return GST_FLOW_OK;
}


gboolean gst_skin_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "skin", GST_RANK_NONE, GST_TYPE_SKIN);
}















////////////////////////////////////////////////////////////////////////////////
gint gstskin_find_skin_center_of_mass(struct _GstSkin *skin, gint display)
{
  int skin_under_seed = 0;

  IplImage* imageRGB = cvCreateImageHeader( cvSize(skin->width, skin->height), IPL_DEPTH_8U, 3);
  imageRGB->imageData = skin->cvRGB->imageData;

  IplImage* imageHSV = cvCreateImage( cvSize(skin->width, skin->height), IPL_DEPTH_8U, 3);
  cvCvtColor(imageRGB, imageHSV, CV_RGB2HSV);

  IplImage* planeH = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Hue component.
  IplImage* planeH2= cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Hue component, 2nd threshold
  IplImage* planeS = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Saturation component.
  IplImage* planeV = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Brightness component.
  cvCvtPixToPlane(imageHSV, planeH, planeS, planeV, 0);	// Extract the 3 color components.

  // Detect which pixels in each of the H, S and V channels are probably skin pixels.
  // Assume that skin has a Hue between 0 to 18 (out of 180), and Saturation above 50, and Brightness above 80.
  cvThreshold(planeH , planeH2, 10, UCHAR_MAX, CV_THRESH_BINARY);         //(hue > 10)
  cvThreshold(planeH , planeH , 20, UCHAR_MAX, CV_THRESH_BINARY_INV);     //(hue < 20)
  cvThreshold(planeS , planeS , 48, UCHAR_MAX, CV_THRESH_BINARY);         //(sat > 48)
  cvThreshold(planeV , planeV , 80, UCHAR_MAX, CV_THRESH_BINARY);         //(val > 80)

  // erode the HUE to get rid of noise.
  cvErode(planeH, planeH, NULL, 1);

  // Combine all 3 thresholded color components, so that an output pixel will only
  // be white (255) if the H, S and V pixels were also white.
  IplImage* imageSkinPixels = cvCreateImage( cvGetSize(imageHSV), 8, 1);        // Greyscale output image.
  // imageSkin = (hue > 10) ^ (hue < 20) ^ (sat > 48) ^ (val > 80), where   ^ mean pixels-wise AND
  cvAnd(planeH         , planeS , imageSkinPixels);	
  cvAnd(imageSkinPixels, planeH2, imageSkinPixels);	
  cvAnd(imageSkinPixels, planeV , imageSkinPixels);	

  if(display){
    if( skin->showH )
      cvCvtColor(planeH, imageRGB, CV_GRAY2RGB);
    else if( skin->showS )
      cvCvtColor(planeS, imageRGB, CV_GRAY2RGB);
    else if( skin->showV )
      cvCvtColor(planeV, imageRGB, CV_GRAY2RGB);
    else
      cvCvtColor(imageSkinPixels, imageRGB, CV_GRAY2RGB);
  }


  cvReleaseImage( &imageSkinPixels );
  cvReleaseImage( &planeH );
  cvReleaseImage( &planeH2);
  cvReleaseImage( &planeS );
  cvReleaseImage( &planeV );
  cvReleaseImage( &imageHSV );
  cvReleaseImage( &imageRGB );

  return(skin_under_seed);
}


////////////////////////////////////////////////////////////////////////////////
gint gstskin_find_skin_center_of_mass2(struct _GstSkin *skin, gint display)
{
  int skin_under_seed = 0;

  IplImage* imageRGB = cvCreateImageHeader( cvSize(skin->width, skin->height), IPL_DEPTH_8U, 3);
  imageRGB->imageData = skin->cvRGB->imageData;


  IplImage* planeR  = cvCreateImage( cvGetSize(imageRGB), 8, 1);	// R component.
  IplImage* planeG  = cvCreateImage( cvGetSize(imageRGB), 8, 1);	// G component.
  IplImage* planeB  = cvCreateImage( cvGetSize(imageRGB), 8, 1);	// B component.

  IplImage* planeAll = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);	// (R+G+B) component.
  IplImage* planeR2  = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);	// R component, 32bits
  IplImage* planeRp  = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);	// R' and >0.4
  IplImage* planeGp  = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);	// G' and > 0.28

  IplImage* planeRp2 = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);	// R' <0.6
  IplImage* planeGp2 = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);	// G' <0.4

  cvCvtPixToPlane(imageRGB, planeR, planeG, planeB, 0);	// Extract the 3 color components.
  cvAdd( planeR, planeG,   planeAll, NULL);
  cvAdd( planeB, planeAll, planeAll, NULL);  // All = R + G + B
  cvDiv( planeR, planeAll, planeRp, 1.0);    // R' = R / ( R + G + B)
  cvDiv( planeG, planeAll, planeGp, 1.0);    // G' = G / ( R + G + B)

  cvConvertScale( planeR, planeR2, 1.0, 0.0);
  cvCopy(planeGp, planeGp2, NULL);
  cvCopy(planeRp, planeRp2, NULL);

  cvThreshold(planeR2 , planeR2,   60, UCHAR_MAX, CV_THRESH_BINARY);     //(R > 60)
  cvThreshold(planeRp , planeRp, 0.42, UCHAR_MAX, CV_THRESH_BINARY);     //(R'> 0.4)
  cvThreshold(planeRp2, planeRp2, 0.6, UCHAR_MAX, CV_THRESH_BINARY_INV); //(R'< 0.6)
  cvThreshold(planeGp , planeGp, 0.28, UCHAR_MAX, CV_THRESH_BINARY);     //(G'> 0.28)
  cvThreshold(planeGp2, planeGp2, 0.4, UCHAR_MAX, CV_THRESH_BINARY_INV); //(G'< 0.4)

  // Combine all 3 thresholded color components, so that an output pixel will only
  // be white (255) if the H, S and V pixels were also white.
  IplImage* imageSkinPixels = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_32F, 1);        // Greyscale output image.

  cvAnd( planeR2 ,         planeRp , imageSkinPixels);	
  cvAnd( planeRp , imageSkinPixels , imageSkinPixels);	
  cvAnd( planeRp2, imageSkinPixels , imageSkinPixels);	
  cvAnd( planeGp , imageSkinPixels , imageSkinPixels);	
  cvAnd( planeGp2, imageSkinPixels , imageSkinPixels);	

  IplImage* draft = cvCreateImage( cvGetSize(imageRGB), IPL_DEPTH_8U , 1);        // Greyscale output image.
  if(display){
    if( skin->showH )
      cvConvertScale( planeRp, draft, 1.0, 0.0);
    else if( skin->showS )
      cvConvertScale( planeG, draft, 1.0, 0.0);
    else if( skin->showV )
      cvConvertScale( planeB, draft, 1.0, 0.0);
    else
      cvConvertScale( imageSkinPixels, draft, 1.0, 0.0);
    cvCvtColor(draft, imageRGB, CV_GRAY2RGB);
  }


  cvReleaseImage( &imageSkinPixels );
  cvReleaseImage( &planeR );
  cvReleaseImage( &planeG );
  cvReleaseImage( &planeB );
  cvReleaseImage( &planeAll );
  cvReleaseImage( &planeR2 );
  cvReleaseImage( &planeRp );
  cvReleaseImage( &planeGp );
  cvReleaseImage( &planeRp2 );
  cvReleaseImage( &planeGp2 );
  cvReleaseImage( &draft );

  return(skin_under_seed);
}
