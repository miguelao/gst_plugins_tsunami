/* GStreamer
 */
/**
 * SECTION:element- gc
 *
 * This element is a Wrap-around GC for gstreamer: takes in-alpha composed of 4 numbers and runs a GC iteration
enum{  
  GC_BGD    = 0,  //!< background
  GC_FGD    = 1,  //!< foreground
  GC_PR_BGD = 2,  //!< most probably background
  GC_PR_FGD = 3   //!< most probably foreground
};   

 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstgc.h"

#include <png.h>


GST_DEBUG_CATEGORY_STATIC (gst_gc_debug);
#define GST_CAT_DEFAULT gst_gc_debug

enum {
	PROP_0,
        PROP_DISPLAY,
        PROP_GROWFACTOR,
	PROP_LAST
};

static GstStaticPadTemplate gst_gc_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_gc_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_GC_LOCK(gc) G_STMT_START { \
	GST_LOG_OBJECT (gc, "Locking gc from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&gc->lock); \
	GST_LOG_OBJECT (gc, "Locked gc from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_GC_UNLOCK(gc) G_STMT_START { \
	GST_LOG_OBJECT (gc, "Unlocking gc from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&gc->lock); \
} G_STMT_END

static gboolean gst_gc_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_gc_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_gc_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_gc_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_gc_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gc_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_gc_finalize(GObject * object);
static gboolean gst_gc_sink_event(GstPad *pad, GstEvent * event);

static void compose_matrix_from_image(CvMat* output, IplImage* input);




GST_BOILERPLATE (GstGc, gst_gc, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanGc(GstGc *gc) 
{
  if (gc->pImageRGBA)  cvReleaseImageHeader(&gc->pImageRGBA);

  finalise_grabcut( &gc->GC );
}

static void gst_gc_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Image GC filter - Grabcut expansion", "Filter/Effect/Video",
"Runs Grabcut algorithm on input alpha. Values: BG=0, FG=1, PR_BG=2, PR_FGD=3; \n\
NOTE: larger values of alpha (notably 255) is interpreted as PR_FGD too.\n\
IN CASE OF no alpha mask input (all 0's or all 1's), the 'facelocation' or \n\
'objectlocation' downstream event is used to create a bbox of PR_FG elements.\n\
IF nothing is present, then a by default bbox is taken.",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_gc_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_gc_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_gc_debug, "gc", 0, \
                           "gc - Runs Grabcut algorithm on input alpha");
}

static void gst_gc_class_init(GstGcClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_gc_set_property;
  gobject_class->get_property = gst_gc_get_property;
  gobject_class->finalize = gst_gc_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_gc_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_gc_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_gc_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_gc_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output would be the actual bg/fg model", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, 
                                  PROP_GROWFACTOR, g_param_spec_float(
                                  "growfactor", "growfactor",
                                  "Multiplier factor for input bbox, usually too small for practical purposes", 
                                  0, 100, 1.25, (GParamFlags)(G_PARAM_READWRITE)));
}

static void gst_gc_init(GstGc * gc, GstGcClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)gc, TRUE);
  g_static_mutex_init(&gc->lock);

  gc->pImageRGBA        = NULL;
  gc->pImgRGB           = NULL;

  gc->pImgChX           = NULL;
  gc->pImgChA           = NULL;
  gc->pImgCh1           = NULL;
  gc->pImgCh2           = NULL;
  gc->pImgCh3           = NULL;

  gc->display       = false;
  gc->growfactor        = 1.7;
}

static void gst_gc_finalize(GObject * object) 
{
  GstGc *gc = GST_GC (object);
  
  GST_GC_LOCK (gc);
  CleanGc(gc);
  GST_GC_UNLOCK (gc);
  GST_INFO("Gc destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&gc->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_gc_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstGc *gc = GST_GC (object);
  
  GST_GC_LOCK (gc);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    gc->display = g_value_get_boolean(value);
    break;    
  case PROP_GROWFACTOR:
    gc->growfactor = g_value_get_float(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_GC_UNLOCK (gc);
}

static void gst_gc_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstGc *gc = GST_GC (object);

  switch (prop_id) {
  case PROP_DISPLAY:
    g_value_set_boolean(value, gc->display);
    break; 
  case PROP_GROWFACTOR:
    g_value_set_float(value, gc->growfactor);
    break; 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_gc_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) 
{
  GstVideoFormat format;
  gint width, height;
  
  if (!gst_video_format_parse_caps(caps, &format, &width, &height))
    return FALSE;
  
  *size = gst_video_format_get_size(format, width, height);
  
  GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);
  
  return TRUE;
}


static gboolean gst_gc_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstGc *gc = GST_GC (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_GC_LOCK (gc);
  
  gst_video_format_parse_caps(incaps, &gc->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &gc->out_format, &out_width, &out_height);
  if (!(gc->in_format == gc->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_GC_UNLOCK (gc);
    return FALSE;
  }
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(gc),  gst_gc_sink_event);
  
  gc->width  = in_width;
  gc->height = in_height;
  
  GST_INFO("Initialising Gc...");

  const CvSize size = cvSize(gc->width, gc->height);
  GST_WARNING (" width %d, height %d", gc->width, gc->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  gc->pImageRGBA    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  gc->pImgRGB       = cvCreateImage(size, IPL_DEPTH_8U, 3);

  gc->pImgChA       = cvCreateImageHeader(size, IPL_DEPTH_8U, 1);
  gc->pImgCh1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gc->pImgCh2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gc->pImgCh3       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gc->pImgChX       = cvCreateImage(size, IPL_DEPTH_8U, 1);

  gc->grabcut_mask   = cvCreateMat( size.height, size.width, CV_8UC1);
  cvZero(gc->grabcut_mask);
  initialise_grabcut( &(gc->GC), gc->pImgRGB, gc->grabcut_mask );


  gc->facepos.x     = 132;
  gc->facepos.y     = 77;
  gc->facepos.width = 60;
  gc->facepos.height= 70;

  gc->numframes = 0;

  GST_INFO("Gc initialized.");
  
  GST_GC_UNLOCK (gc);
  
  return TRUE;
}

static void gst_gc_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_gc_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstGc *gc = GST_GC (btrans);

  GST_GC_LOCK (gc);

  //////////////////////////////////////////////////////////////////////////////
  // get image data from the input, which is RGBA or BGRA
  gc->pImageRGBA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvSplit(gc->pImageRGBA,   gc->pImgCh1, gc->pImgCh2, gc->pImgCh3, gc->pImgChX );
  cvCvtColor(gc->pImageRGBA,  gc->pImgRGB, CV_BGRA2BGR);

  compose_matrix_from_image(gc->grabcut_mask, gc->pImgChX);

  // Pass pImgChX to grabcut_mask for the graphcut stuff
  // but that only if really there is something in the mask! 
  // otherwise -->input bbox is what we use
  bool using_input_bbox = false;
  int alphapixels = cvCountNonZero(gc->pImgChX);
  if( (0 < alphapixels) && (alphapixels < (gc->width * gc->height)) ){
    GST_INFO("running on mask");
    run_graphcut_iteration( &(gc->GC), gc->pImgRGB, gc->grabcut_mask, NULL);
  }
  else{
    GST_INFO("running on bbox (%d,%d),(%d,%d)", gc->facepos.x,gc->facepos.y,gc->facepos.width,gc->facepos.height);
    run_graphcut_iteration2( &(gc->GC), gc->pImgRGB, gc->grabcut_mask, &(gc->facepos) );
  }



  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( gc->display ){
    cvAndS(gc->grabcut_mask, cvScalar(1), gc->grabcut_mask, NULL);  // get only FG, PR_FG
    cvConvertScale( gc->grabcut_mask, gc->grabcut_mask, 255.0);     // (saturated) FG, PR_FG --> 255

    cvAnd( gc->grabcut_mask,  gc->pImgCh1,  gc->pImgCh1, NULL);
    cvAnd( gc->grabcut_mask,  gc->pImgCh2,  gc->pImgCh2, NULL);
    cvAnd( gc->grabcut_mask,  gc->pImgCh3,  gc->pImgCh3, NULL);
    //cvCvtColor( gc->pImgGRAY, gc->pImgRGB, CV_GRAY2BGR );   

    //cvRectangle(gc->pImgCh1, cvPoint(gc->facepos.x, gc->facepos.y), 
    //              cvPoint(gc->facepos.x + gc->facepos.width, gc->facepos.y+gc->facepos.height),
    //              cvScalar(127,0.0), 1, 8, 0 );

  }

  //////////////////////////////////////////////////////////////////////////////
  // copy anyhow the fg/bg to the alpha channel in the output image alpha ch
  cvAndS(gc->grabcut_mask, cvScalar(1), gc->grabcut_mask, NULL);  // get only FG and possible FG
  cvConvertScale( gc->grabcut_mask, gc->grabcut_mask, 255.0);
  gc->pImgChA->imageData = (char*)gc->grabcut_mask->data.ptr;
  cvMerge(              gc->pImgCh1, gc->pImgCh2, gc->pImgCh3, gc->pImgChA, gc->pImageRGBA);

  gc->numframes++;

  GST_GC_UNLOCK (gc);  
  
  return GST_FLOW_OK;
}


gboolean gst_gc_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "gc", GST_RANK_NONE, GST_TYPE_GC);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static gboolean gst_gc_sink_event(GstPad *pad, GstEvent * event)
{
  GstGc *gc = GST_GC (gst_pad_get_parent( pad ));
  gboolean ret = FALSE;
  double x,y,w,h;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation") || gst_event_has_name(event, "objectlocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &x); // check bool return
      gst_structure_get_double(str, "y", &y); // check bool return
      gst_structure_get_double(str, "width", &w); // check bool return
      gst_structure_get_double(str, "height", &h);// check bool return
      
      //if(gst_event_has_name(event, "facelocation")){
      //  gboolean facebool;
      //  gst_structure_get_boolean(str, "facefound", &facebool);// check bool return
      //  w *= gc->growfactor * ((facebool) ? 1.15 : 1.15);
      //  h *= gc->growfactor * ((facebool) ? 1.25 : 1.25);
      //}
      //else{
      //  w *= gc->growfactor;
      //  h *= gc->growfactor;
      //}
      w *= gc->growfactor;
      h *= gc->growfactor;
      
      if( abs(x) > 2 )    gc->facepos.x = (int)x -w/2;
      if( abs(y) > 2 )    gc->facepos.y = (int)y -h/2;
      if( abs(w) > 2 )    gc->facepos.width = (int)w;
      if( abs(h) > 2 )    gc->facepos.height = (int)h;

      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(gc);
  return ret;
}


// copied, otherwise only available in C++
enum{  
  GCS_BGD    = 0,  //!< background
  GCS_FGD    = 1,  //!< foreground
  GCS_PR_BGD = 2,  //!< most probably background
  GCS_PR_FGD = 3   //!< most probably foreground
};   

////////////////////////////////////////////////////////////////////////////////
void compose_matrix_from_image(CvMat* output, IplImage* input)
{

  int x, y;
  for( x = 0; x < output->cols; x++ ){
    for( y = 0; y < output->rows; y++ ){
      CV_MAT_ELEM(*output, uchar, y, x) = (cvGetReal2D( input, y, x) <= GCS_PR_FGD ) ? cvGetReal2D( input, y, x) : GCS_PR_FGD;
    }
  }

  
}



