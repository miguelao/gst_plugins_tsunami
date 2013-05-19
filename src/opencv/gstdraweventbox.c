/* GStreamer
 */
/**
 * SECTION:element- draweventbox
 *
 * This element Draws a box from the received face/objectlocation
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstdraweventbox.h"

GST_DEBUG_CATEGORY_STATIC (gst_draweventbox_debug);
#define GST_CAT_DEFAULT gst_draweventbox_debug


enum {
	PROP_0,
        PROP_DISPLAY,
	PROP_LAST
};

static GstStaticPadTemplate gst_draweventbox_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_draweventbox_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_DRAWEVENTBOX_LOCK(draweventbox) G_STMT_START { \
	GST_LOG_OBJECT (draweventbox, "Locking draweventbox from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&draweventbox->lock); \
	GST_LOG_OBJECT (draweventbox, "Locked draweventbox from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_DRAWEVENTBOX_UNLOCK(draweventbox) G_STMT_START { \
	GST_LOG_OBJECT (draweventbox, "Unlocking draweventbox from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&draweventbox->lock); \
} G_STMT_END

static gboolean gst_draweventbox_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_draweventbox_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_draweventbox_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_draweventbox_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_draweventbox_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_draweventbox_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_draweventbox_finalize(GObject * object);

static gboolean gst_draweventbox_sink_event(GstPad *pad, GstEvent * event);

GST_BOILERPLATE (GstDraweventbox, gst_draweventbox, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanDraweventbox(GstDraweventbox *draweventbox) 
{
  if (draweventbox->pFrame)  cvReleaseImageHeader(&draweventbox->pFrame);
}

static void gst_draweventbox_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Draw event box on the video stream ", "Filter/Effect/Video",
    "Draws a box from the received face/objectlocation",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_draweventbox_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_draweventbox_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_draweventbox_debug, "draweventbox", 0, \
                           "draweventbox - Draws a box from the received face/objectlocation ");
}

static void gst_draweventbox_class_init(GstDraweventboxClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_draweventbox_set_property;
  gobject_class->get_property = gst_draweventbox_get_property;
  gobject_class->finalize = gst_draweventbox_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_draweventbox_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_draweventbox_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_draweventbox_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_draweventbox_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "draw or not the bounding box from event ", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_draweventbox_init(GstDraweventbox * draweventbox, GstDraweventboxClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)draweventbox, TRUE);
  g_static_mutex_init(&draweventbox->lock);

  draweventbox->pFrame        = NULL;
  draweventbox->pFrame2       = NULL;

  draweventbox->ch1           = NULL;
  draweventbox->ch2           = NULL;
  draweventbox->ch3           = NULL;

  draweventbox->display       = true;
}

static void gst_draweventbox_finalize(GObject * object) 
{
  GstDraweventbox *draweventbox = GST_DRAWEVENTBOX (object);
  
  GST_DRAWEVENTBOX_LOCK (draweventbox);
  CleanDraweventbox(draweventbox);
  GST_DRAWEVENTBOX_UNLOCK (draweventbox);
  GST_INFO("Draweventbox destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&draweventbox->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_draweventbox_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstDraweventbox *draweventbox = GST_DRAWEVENTBOX (object);
  
  GST_DRAWEVENTBOX_LOCK (draweventbox);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    draweventbox->display = g_value_get_boolean(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_DRAWEVENTBOX_UNLOCK (draweventbox);
}

static void gst_draweventbox_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstDraweventbox *draweventbox = GST_DRAWEVENTBOX (object);

  switch (prop_id) {
  case PROP_DISPLAY:
    g_value_set_boolean(value, draweventbox->display);
    break; 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_draweventbox_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_draweventbox_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstDraweventbox *draweventbox = GST_DRAWEVENTBOX (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_DRAWEVENTBOX_LOCK (draweventbox);
  
  gst_video_format_parse_caps(incaps, &draweventbox->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &draweventbox->out_format, &out_width, &out_height);
  if (!(draweventbox->in_format == draweventbox->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_DRAWEVENTBOX_UNLOCK (draweventbox);
    return FALSE;
  }
  
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(draweventbox),  gst_draweventbox_sink_event);

  draweventbox->width  = in_width;
  draweventbox->height = in_height;
  
  GST_INFO("Initialising Draweventbox...");

  const CvSize size = cvSize(draweventbox->width, draweventbox->height);
  GST_WARNING (" width %d, height %d", draweventbox->width, draweventbox->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  draweventbox->pFrame    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  draweventbox->pFrame2   = cvCreateImage(size, IPL_DEPTH_8U, 3);
  draweventbox->pFrameA   = cvCreateImage(size, IPL_DEPTH_8U, 1);

  draweventbox->ch1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  draweventbox->ch2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  draweventbox->ch3       = cvCreateImage(size, IPL_DEPTH_8U, 1);

  draweventbox->bboxpos.x     = 132;
  draweventbox->bboxpos.y     = 77;
  draweventbox->bboxpos.width = 60;
  draweventbox->bboxpos.height= 70;
  draweventbox->bboxvalid     = false;

  GST_INFO("Draweventbox initialized.");
  
  GST_DRAWEVENTBOX_UNLOCK (draweventbox);
  
  return TRUE;
}

static void gst_draweventbox_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_draweventbox_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstDraweventbox *draweventbox = GST_DRAWEVENTBOX (btrans);

  GST_DRAWEVENTBOX_LOCK (draweventbox);

  //////////////////////////////////////////////////////////////////////////////
  // get image data from the input, which is RGBA or BGRA
  draweventbox->pFrame->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvSplit(draweventbox->pFrame, 
          draweventbox->ch1, draweventbox->ch2, draweventbox->ch3, draweventbox->pFrameA  );
  cvMerge(draweventbox->ch1, draweventbox->ch2, draweventbox->ch3, NULL, 
          draweventbox->pFrame2);

  //////////////////////////////////////////////////////////////////////////////
  // paint bbox red/green on pFrame2, a simple RGB IplImage
  //////////////////////////////////////////////////////////////////////////////
  cvRectangle(draweventbox->pFrame2, 
              cvPoint(draweventbox->bboxpos.x, draweventbox->bboxpos.y), 
              cvPoint(draweventbox->bboxpos.x+ draweventbox->bboxpos.width, 
                      draweventbox->bboxpos.y+ draweventbox->bboxpos.height), 
              (draweventbox->bboxvalid == true ) ? cvScalar(0, 255, 0) : cvScalar(255, 0, 255), 1);


  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( draweventbox->display ){
    //cvCvtColor( draweventbox->pFrameA, draweventbox->pFrame2, CV_GRAY2BGR );
  }
  //////////////////////////////////////////////////////////////////////////////
  // copy anyhow the fg/bg to the alpha channel in the output image alpha ch
  cvSplit(draweventbox->pFrame2, 
          draweventbox->ch1, draweventbox->ch2, draweventbox->ch3, NULL        );
  cvMerge(draweventbox->ch1, draweventbox->ch2, draweventbox->ch3, draweventbox->pFrameA, 
          draweventbox->pFrame);
  

  GST_DRAWEVENTBOX_UNLOCK (draweventbox);  
  
  return GST_FLOW_OK;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static gboolean gst_draweventbox_sink_event(GstPad *pad, GstEvent * event)
{
  GstDraweventbox *draweventbox = GST_DRAWEVENTBOX (gst_pad_get_parent( pad ));
  gboolean ret = FALSE;
  double x,y,w,h;
  gboolean facebool;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation") || gst_event_has_name(event, "objectlocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &x); // check bool return
      gst_structure_get_double(str, "y", &y); // check bool return
      gst_structure_get_double(str, "width", &w); // check bool return
      gst_structure_get_double(str, "height", &h);// check bool return
      
      if(gst_event_has_name(event, "facelocation")){
        gst_structure_get_boolean(str, "facefound", &facebool);// check bool return
      }
      else{
        gst_structure_get_boolean(str, "objectfound", &facebool);// check bool return
      }
      
      if( abs(x) > 2 )    draweventbox->bboxpos.x = (int)x -w/2;
      if( abs(y) > 2 )    draweventbox->bboxpos.y = (int)y -h/2;
      if( abs(w) > 2 )    draweventbox->bboxpos.width = (int)w;
      if( abs(h) > 2 )    draweventbox->bboxpos.height = (int)h;
      draweventbox->bboxvalid = facebool;

      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(draweventbox);
  return ret;
}

gboolean gst_draweventbox_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "draweventbox", GST_RANK_NONE, GST_TYPE_DRAWEVENTBOX);
}
