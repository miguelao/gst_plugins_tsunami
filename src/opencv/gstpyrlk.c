/* GStreamer
 */
/**
 * SECTION:element- pyrlk
 *
 * This element implements a pyramidal lukas-kanade optical flow implementation,
 * based on the natural image's good features to track (not on its edges).
 * The output is the points tracked over a Sobel edge image
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstpyrlk.h"

GST_DEBUG_CATEGORY_STATIC (gst_pyrlk_debug);
#define GST_CAT_DEFAULT gst_pyrlk_debug


enum {
	PROP_0,
	PROP_LAST
};

static GstStaticPadTemplate gst_pyrlk_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_pyrlk_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_PYRLK_LOCK(pyrlk) G_STMT_START { \
	GST_LOG_OBJECT (pyrlk, "Locking pyrlk from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&pyrlk->lock); \
	GST_LOG_OBJECT (pyrlk, "Locked pyrlk from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_PYRLK_UNLOCK(pyrlk) G_STMT_START { \
	GST_LOG_OBJECT (pyrlk, "Unlocking pyrlk from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&pyrlk->lock); \
} G_STMT_END

static gboolean gst_pyrlk_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_pyrlk_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_pyrlk_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_pyrlk_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_pyrlk_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_pyrlk_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_pyrlk_finalize(GObject * object);
static gboolean gst_pyrlk_sink_event(GstPad *pad, GstEvent * event);


GST_BOILERPLATE (GstPyrlk, gst_pyrlk, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanPyrlk(GstPyrlk *pyrlk) 
{
  if (pyrlk->cvRGB)  cvReleaseImageHeader(&pyrlk->cvRGB);
}

static void gst_pyrlk_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Pyr-LK filter on edges", "Filter/Effect/Video",
    "Performs some image adjust operations",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_pyrlk_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_pyrlk_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_pyrlk_debug, "pyrlk", 0, \
                           "pyrlk - Performs some image adjust operations");
}

static void gst_pyrlk_class_init(GstPyrlkClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_pyrlk_set_property;
  gobject_class->get_property = gst_pyrlk_get_property;
  gobject_class->finalize = gst_pyrlk_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_pyrlk_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_pyrlk_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_pyrlk_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_pyrlk_set_caps);
}

static void gst_pyrlk_init(GstPyrlk * pyrlk, GstPyrlkClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)pyrlk, TRUE);
  g_static_mutex_init(&pyrlk->lock);
  pyrlk->cvRGB     = NULL;

}

static void gst_pyrlk_finalize(GObject * object) 
{
  GstPyrlk *pyrlk = GST_PYRLK (object);
  
  GST_PYRLK_LOCK (pyrlk);
  CleanPyrlk(pyrlk);
  GST_PYRLK_UNLOCK (pyrlk);
  GST_INFO("Pyrlk destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&pyrlk->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_pyrlk_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstPyrlk *pyrlk = GST_PYRLK (object);
  
  GST_PYRLK_LOCK (pyrlk);
  switch (prop_id) {
    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_PYRLK_UNLOCK (pyrlk);
}

static void gst_pyrlk_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  //GstPyrlk *pyrlk = GST_PYRLK (object);

  switch (prop_id) { 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_pyrlk_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_pyrlk_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstPyrlk *pyrlk = GST_PYRLK (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_PYRLK_LOCK (pyrlk);
  
  gst_video_format_parse_caps(incaps, &pyrlk->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &pyrlk->out_format, &out_width, &out_height);
  if (!(pyrlk->in_format == pyrlk->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_PYRLK_UNLOCK (pyrlk);
    return FALSE;
  }
  
  pyrlk->width  = in_width;
  pyrlk->height = in_height;
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(pyrlk),  gst_pyrlk_sink_event);
  
  GST_INFO("Initialising Pyrlk...");

  const CvSize size = cvSize(pyrlk->width, pyrlk->height);
  GST_WARNING (" width %d, height %d", pyrlk->width, pyrlk->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB  ////////////////////////////////////////////
  pyrlk->cvRGB = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);
  pyrlk->cvRGBprev = cvCreateImage(size, IPL_DEPTH_8U, 3);
  pyrlk->cvRGBout  = cvCreateImage(size, IPL_DEPTH_8U, 3);

  //////////////////////////////////////////////////////////////////////////////
  // Full allocation of Grey Images (for motion flow)
  pyrlk->cvGrey     = cvCreateImage(size, IPL_DEPTH_8U, 1);
  //pyrlk->cvGreyPrev = cvCreateImage(size, IPL_DEPTH_8U, 1);
  pyrlk->cvGreyPrev = NULL;

  pyrlk->cvEdgeImage   = cvCreateImage(size, IPL_DEPTH_16S, 1);
  pyrlk->cvEdgeImage2  = cvCreateImage(size, IPL_DEPTH_16S, 1);

  pyrlk->facepos.x=0; pyrlk->facepos.y=0;
  pyrlk->center.x=0;  pyrlk->center.y=0;

  GST_INFO("Pyrlk initialized.");
  
  GST_PYRLK_UNLOCK (pyrlk);
  
  return TRUE;
}

static void gst_pyrlk_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_pyrlk_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstPyrlk *pyrlk = GST_PYRLK (btrans);

  GST_PYRLK_LOCK (pyrlk);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc

  // get image data from the input, which is RGB or BGR
  pyrlk->cvRGB->imageData = (char*)GST_BUFFER_DATA(gstbuf);

  // apply some pre-filtering to the input image
  cvSmooth( pyrlk->cvRGB, pyrlk->cvRGBout, CV_BILATERAL, 7, 7, 3, 3);
  //cvSmooth( pyrlk->cvRGB, pyrlk->cvRGBout, CV_MEDIAN, 3, 3);

  // get absolute difference between frames                         // 
  //cvAbsDiff(pyrlk->cvRGBout, pyrlk->cvRGBprev, pyrlk->cvRGBout);  // <-+
                                                                    //   | one or the
  // from the pre-cooked image, we pass to GRAY for the pyrlk       //   | other
  cvCvtColor( pyrlk->cvRGB, pyrlk->cvGrey, CV_RGB2GRAY );           //   | (pre: pyrlk on diff image)
                                                                    //   | (post: just for drawing)
  // get absolute difference between frames                         //   |
  //cvAbsDiff(pyrlk->cvRGBout, pyrlk->cvRGBprev, pyrlk->cvRGBout);    // <-+



  //calc_eisemann_durand_luminance(pyrlk->cvRGB, pyrlk->cvGrey);
  if( pyrlk->cvGreyPrev == NULL)
    pyrlk->cvGreyPrev = cvCloneImage( pyrlk->cvGrey );


  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  #define MAX_CORNERS 500
  int num_corners = MAX_CORNERS;
  CvPoint2D32f* cornersA = (CvPoint2D32f*)malloc(sizeof(CvPoint2D32f)*MAX_CORNERS);
  CvPoint2D32f* cornersB = (CvPoint2D32f*)malloc(sizeof(CvPoint2D32f)*MAX_CORNERS);
  char corners_found[ MAX_CORNERS ];
  float corners_error[ MAX_CORNERS ];


  //cvSobel( pyrlk->cvGrey    , pyrlk->cvEdgeImage , 1, 1, 3 );
  //cvSobel( pyrlk->cvGreyPrev, pyrlk->cvEdgeImage2, 1, 1, 3 );
  //cvConvertScale(pyrlk->cvEdgeImage, pyrlk->cvGrey, 1.0/256.0, 0); 
  //cvCvtColor( pyrlk->cvGrey, pyrlk->cvRGBout, CV_GRAY2RGB );
  //cvConvertScale(pyrlk->cvEdgeImage2, pyrlk->cvGreyPrev, 1.0/256, 0); 

  num_corners = calculate_pyrlk( pyrlk->cvGreyPrev, pyrlk->cvGrey, 
                                 num_corners, cornersA, corners_found, corners_error, cornersB);
  //////////////////////////////////////////////////////////////////////////////
  int i;
  if( (pyrlk->facepos.x != 0) && ( pyrlk->facepos.y != 0)){
    for( i=0; i < num_corners; i++ ){
      //if( ! is_point_in_bbox( cvPoint( cvRound( cornersA[i].x ), cvRound( cornersA[i].y ) ),
      //                        pyrlk->facepos))
      //  corners_found[i]=0;
    }
  }


  if((pyrlk->center.x==0) && (pyrlk->facepos.x!=0) && (pyrlk->facepos.x<=pyrlk->cvRGB->width)){
    pyrlk->center.x = pyrlk->facepos.x;
    pyrlk->center.y = pyrlk->facepos.y;
  }
  //if( !is_point_in_bbox(pyrlk->center, pyrlk->facepos)) {pyrlk->center.x = pyrlk->facepos.x;pyrlk->center.y = pyrlk->facepos.y;}

  // Make an image of the results  
  draw2_pyrlk_vectors( pyrlk->cvRGBout, num_corners, cornersA, corners_found, corners_error, cornersB, &pyrlk->center);
  draw_points( pyrlk->cvRGBout, num_corners, cornersA, CV_RGB(0,255,0) );

  cvRectangle( pyrlk->cvRGBout, 
               cvPoint( pyrlk->facepos.x-0.5*pyrlk->facepos.w,   pyrlk->facepos.y-0.5*pyrlk->facepos.w),
               cvPoint((pyrlk->facepos.x+0.5*pyrlk->facepos.w), (pyrlk->facepos.y+0.5*pyrlk->facepos.w)),
               CV_RGB(255,0,0), 1 , 8, 0);



  // save the current image for the next iteration
  pyrlk->cvGreyPrev = cvCloneImage( pyrlk->cvGrey );
  // save the original frame for the next iteration
  pyrlk->cvRGBprev  = cvCloneImage( pyrlk->cvRGB );

  // copy the RGBout to the input
  cvCopy( pyrlk->cvRGBout, pyrlk->cvRGB, NULL );

  GST_PYRLK_UNLOCK (pyrlk); 
  return GST_FLOW_OK;
}



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static gboolean gst_pyrlk_sink_event(GstPad *pad, GstEvent * event)
{
  GstPyrlk *pyrlk = GST_PYRLK (gst_pad_get_parent( pad ));
  gboolean ret = FALSE;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &pyrlk->facepos.x); // check bool return
      gst_structure_get_double(str, "y", &pyrlk->facepos.y); // check bool return
      gst_structure_get_double(str, "width", &pyrlk->facepos.w); // check bool return
      gst_structure_get_double(str, "height", &pyrlk->facepos.h);// check bool return
      GST_INFO("Received custom event with face detection, (%.2f,%.2f)", pyrlk->facepos.x, pyrlk->facepos.y);
      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(pyrlk);
  return ret;
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
gboolean gst_pyrlk_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "pyrlk", GST_RANK_NONE, GST_TYPE_PYRLK);
}
