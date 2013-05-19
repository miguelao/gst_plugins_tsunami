/* GStreamer
 */
/**
 * SECTION:element- objecttracker
 *
 * This element integrates Predator tracking algorithm in a GStreamer plugin
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstobjecttracker.h"
#include <opencv/cv.h>

GST_DEBUG_CATEGORY_STATIC (gst_objecttracker_debug);
#define GST_CAT_DEFAULT gst_objecttracker_debug

#define DEFAULT_EVENTNAME  			"objectlocation"
#define DEFAULT_EVENTRESULTNAME		"objectfound"

enum {
	PROP_0,
	PROP_X,
	PROP_Y,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_DISPLAYBB,
	PROP_EVENTNAME,
	PROP_LAST
};

static GstStaticPadTemplate gst_objecttracker_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);
static GstStaticPadTemplate gst_objecttracker_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS ("video/x-raw-rgb")
);

#define GST_OBJECTTRACKER_LOCK(objecttracker) G_STMT_START { \
	GST_LOG_OBJECT (objecttracker, "Locking objecttracker from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&objecttracker->lock); \
	GST_LOG_OBJECT (objecttracker, "Locked objecttracker from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_OBJECTTRACKER_UNLOCK(objecttracker) G_STMT_START { \
	GST_LOG_OBJECT (objecttracker, "Unlocking objecttracker from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&objecttracker->lock); \
} G_STMT_END

static gboolean gst_objecttracker_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_objecttracker_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_objecttracker_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_objecttracker_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);
static gboolean gst_objecttracker_sink_event(GstPad *pad, GstEvent * event);
static gboolean gst_objecttracker_notify_trackerresults(GstBaseTransform * btrans,
                                                          GstObjecttracker *objecttracker,
                                                          TrackerResult* trackerResult);

static void gst_objecttracker_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_objecttracker_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_objecttracker_finalize(GObject * object);
static gboolean properties_values_are_consistent(IplImage *img, cv::Rect boundingBox);

GST_BOILERPLATE (GstObjecttracker, gst_objecttracker, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanObjecttracker(GstObjecttracker *objecttracker)
{
  if (objecttracker->cvRGB)
	  cvReleaseImageHeader(&objecttracker->cvRGB);
  delete objecttracker->predator;
  g_free(objecttracker->eventname);
  g_free(objecttracker->eventresultname);
}

static void gst_objecttracker_base_init(gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Objecttracker filter", "Filter/Effect/Video",
    "Object tracker based on Predator algorithm",
    "Paul HENRYS <Paul.Henrys@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_objecttracker_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_objecttracker_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_objecttracker_debug, "objecttracker", 0, \
                           "objecttracker - Object tracker based on Predator algorithm");
}

static void gst_objecttracker_class_init(GstObjecttrackerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_objecttracker_set_property;
  gobject_class->get_property = gst_objecttracker_get_property;
  gobject_class->finalize = gst_objecttracker_finalize;
  

  g_object_class_install_property(gobject_class, PROP_X,
  	      g_param_spec_int ("x", "x",
  	          "x coordinate of the upper-left corner",
  	          0, 100000,
  	          0, (GParamFlags) G_PARAM_READWRITE));
  g_object_class_install_property(gobject_class, PROP_Y,
  	      g_param_spec_int ("y", "y",
  	          "y coordinate of the upper-left corner",
  	          0, 100000,
  	          0, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_WIDTH,
  	      g_param_spec_int ("width", "width",
  	          "width of the bounding box",
  	          0, 100000,
  	          0, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_HEIGHT,
  	      g_param_spec_int ("height", "height",
  	          "height of the bounding box",
  	          0, 100000,
  	          0, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_DISPLAYBB,
  	      g_param_spec_boolean ("displayBB", "displayBB",
  	          "Display the bounding box",
  	          FALSE,
  	          (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class, PROP_EVENTNAME,
                                  g_param_spec_string(
                                    "eventname", "eventname", "Name of the DS detection event",
                                    DEFAULT_EVENTNAME,	
                                    (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_objecttracker_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_objecttracker_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_objecttracker_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_objecttracker_set_caps);
}

static void gst_objecttracker_init(GstObjecttracker * objecttracker, GstObjecttrackerClass * klass)
{
  gst_base_transform_set_in_place((GstBaseTransform *)objecttracker, TRUE);
  g_static_mutex_init(&objecttracker->lock);
  objecttracker->cvRGB = NULL;
  objecttracker->initDone = FALSE;
  objecttracker->displayBB = FALSE;
  objecttracker->predator = new Predator();
  objecttracker->bb_x = objecttracker->bb_y = objecttracker->bb_width = objecttracker->bb_height = 0;
  objecttracker->nframes = objecttracker->objectCount = 0;
  objecttracker->eventname = g_strdup(DEFAULT_EVENTNAME);
  objecttracker->eventresultname = g_strdup(DEFAULT_EVENTRESULTNAME);
}

static void gst_objecttracker_finalize(GObject * object)
{
  GstObjecttracker *objecttracker = GST_OBJECTTRACKER (object);
  
  GST_OBJECTTRACKER_LOCK (objecttracker);
  printf("Object detection rate: total:%ld found:%ld(%.2f%%)\n", objecttracker->nframes,objecttracker->objectCount,(objecttracker->objectCount*100.0)/objecttracker->nframes);
  CleanObjecttracker(objecttracker);
  GST_OBJECTTRACKER_UNLOCK (objecttracker);
  GST_INFO("Objecttracker destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&objecttracker->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_objecttracker_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstObjecttracker *objecttracker = GST_OBJECTTRACKER (object);
  
  GST_OBJECTTRACKER_LOCK (objecttracker);
  switch (prop_id) {
  case PROP_WIDTH:
    objecttracker->bb_width = g_value_get_int(value);
    objecttracker->initDone = FALSE;
    break;
  case PROP_HEIGHT:
    objecttracker->bb_height = g_value_get_int(value);
    objecttracker->initDone = FALSE;
    break;
  case PROP_X:
    objecttracker->bb_x = g_value_get_int(value);
    objecttracker->initDone = FALSE;
    break;
  case PROP_Y:
    objecttracker->bb_y = g_value_get_int(value);
    objecttracker->initDone = FALSE;
    break;
  case PROP_DISPLAYBB:
    objecttracker->displayBB = g_value_get_boolean (value);
    break;
  case PROP_EVENTNAME:
    g_free(objecttracker->eventname);
    g_free(objecttracker->eventresultname);
    objecttracker->eventname = g_value_dup_string(value);
    objecttracker->eventresultname = g_strdup(strcmp(objecttracker->eventname, DEFAULT_EVENTNAME) ? "facefound" : DEFAULT_EVENTRESULTNAME);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_OBJECTTRACKER_UNLOCK (objecttracker);
}

static void gst_objecttracker_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstObjecttracker *objecttracker = GST_OBJECTTRACKER (object);

  switch (prop_id) {
  case PROP_WIDTH:
    g_value_set_int(value, objecttracker->bb_width);
    break;
  case PROP_HEIGHT:
	g_value_set_int(value, objecttracker->bb_height);
    break;
  case PROP_X:
	g_value_set_int(value, objecttracker->bb_x);
    break;
  case PROP_Y:
	g_value_set_int(value, objecttracker->bb_y);
    break;
  case PROP_DISPLAYBB:
    g_value_set_boolean (value, objecttracker->displayBB);
    break;
  case PROP_EVENTNAME:
    g_value_set_string(value, objecttracker->eventname);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_objecttracker_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_objecttracker_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps)
{
  GstObjecttracker *objecttracker = GST_OBJECTTRACKER (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_OBJECTTRACKER_LOCK (objecttracker);
  
  gst_video_format_parse_caps(incaps, &objecttracker->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &objecttracker->out_format, &out_width, &out_height);
  if (!(objecttracker->in_format == objecttracker->out_format) ||
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_OBJECTTRACKER_UNLOCK (objecttracker);
    return FALSE;
  }
  
  objecttracker->width  = in_width;
  objecttracker->height = in_height;
  
  GST_INFO("Initialising Objecttracker...");
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(objecttracker),  gst_objecttracker_sink_event);

  const CvSize size = cvSize(objecttracker->width, objecttracker->height);
  GST_WARNING (" width %d, height %d", objecttracker->width, objecttracker->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in RGB (BGR in reality, careful)////////////////////
  objecttracker->cvRGB = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);


  GST_INFO("Objecttracker initialized.");
  
  GST_OBJECTTRACKER_UNLOCK (objecttracker);
  
  return TRUE;
}

static void gst_objecttracker_before_transform(GstBaseTransform * btrans, GstBuffer * buf)
{
}

static GstFlowReturn gst_objecttracker_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf)
{
  GstObjecttracker *objecttracker = GST_OBJECTTRACKER (btrans);
  cv::Rect boundingBox;
  CvScalar colour;

  GST_OBJECTTRACKER_LOCK (objecttracker);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is RGB
  objecttracker->cvRGB->imageData = (char*)GST_BUFFER_DATA(gstbuf);


  //////////////////////////////////////////////////////////////////////////////
  // Create the bounding box to use for tracking
  boundingBox = cv::Rect(objecttracker->bb_x, objecttracker->bb_y, objecttracker->bb_width, objecttracker->bb_height);

  //////////////////////////////////////////////////////////////////////////////
  //  Run iteration of predator tracker, and send event DS if successful

  if ( properties_values_are_consistent(objecttracker->cvRGB, boundingBox) ) {
	// Init or update the BB when updating the plugin parameters or after receiving an upstream event (face, etc)
    if ( !objecttracker->initDone ) {
      GST_INFO("(Re)initializing object tracker with bounding box parameters:(%d,%d)+(%d,%d)\n",
    	       objecttracker->bb_x, objecttracker->bb_y, objecttracker->bb_width, objecttracker->bb_height);
      objecttracker->predator->resetTemplate(objecttracker->cvRGB, boundingBox); //This specifies the template to be tracked
      objecttracker->initDone = TRUE;

      if ( objecttracker->displayBB )
            cvRectangle(objecttracker->cvRGB,
                        cvPoint(objecttracker->bb_x, objecttracker->bb_y),
                        cvPoint(objecttracker->bb_x+objecttracker->bb_width, objecttracker->bb_y+objecttracker->bb_height),
                        CV_RGB(255, 0, 0), 1, 1, 0);
    }
    //////////////////////////////////////////////////////////////////////////
    TrackerResult trackerResult = objecttracker->predator->doWork(objecttracker->cvRGB);
      
    if(trackerResult.success) {
      objecttracker->objectCount++;
      colour = CV_RGB(0, 255, 0);
      if ( objecttracker->displayBB )
        cvRectangle(objecttracker->cvRGB, trackerResult.boundingBox.tl(), trackerResult.boundingBox.br(), colour, 2, 2, 0);

      // Save coordinates of last BB found
      objecttracker->bb_x = trackerResult.boundingBox.x;
      objecttracker->bb_y = trackerResult.boundingBox.y;
      objecttracker->bb_width = trackerResult.boundingBox.width;
      objecttracker->bb_height = trackerResult.boundingBox.height;
    }
    else{
      colour = CV_RGB(0, 255, 255);
      if ( objecttracker->displayBB )
        cvRectangle(objecttracker->cvRGB,
                    cvPoint(objecttracker->bb_x, objecttracker->bb_y),
                    cvPoint(objecttracker->bb_x + objecttracker->bb_width, objecttracker->bb_y + objecttracker->bb_height),
                    colour, 2, 2, 0);
      GST_INFO("Unsuccessful tracking :(");
    }
    // Send an inbound message downstream
    gst_objecttracker_notify_trackerresults(btrans, objecttracker, &trackerResult);
  }
  
  objecttracker->nframes++;
  GST_OBJECTTRACKER_UNLOCK (objecttracker);
  
  return GST_FLOW_OK;
}

static gboolean properties_values_are_consistent(IplImage *img, cv::Rect boundingBox)
{
	if ( boundingBox.x > 0 && boundingBox.y > 0 && \
		 boundingBox.x < img->width && boundingBox.y < img->height && \
		 (boundingBox.x + boundingBox.width) <= img->width && \
		 (boundingBox.y + boundingBox.height) <= img->height )
		return TRUE;
	else
		return FALSE;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static gboolean gst_objecttracker_sink_event(GstPad *pad, GstEvent * event)
{
  GstObjecttracker *objecttracker = GST_OBJECTTRACKER (gst_pad_get_parent( pad ));
  gboolean ret = FALSE;
  double x,y,w,h;
  gboolean t;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &x); // check bool return
      gst_structure_get_double(str, "y", &y); // check bool return
      gst_structure_get_double(str, "width", &w); // check bool return
      gst_structure_get_double(str, "height", &h);// check bool return
      gst_structure_get_boolean(str, "facefound", &t);// check bool return
      
      objecttracker->bb_x = (int)x - (int)(w/2);
      objecttracker->bb_y = (int)y - (int)(h/2);
      objecttracker->bb_width = (int)w;
      objecttracker->bb_height = (int)h;
      objecttracker->initDone = !t;

      gst_event_unref(event);
      ret = TRUE;
    }
    else if (gst_event_has_name(event, "objectlocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &x);
      gst_structure_get_double(str, "y", &y);
      gst_structure_get_double(str, "width", &w);
      gst_structure_get_double(str, "height", &h);

      objecttracker->bb_x = (int)x;
      objecttracker->bb_y = (int)y;
      objecttracker->bb_width = (int)w;
      objecttracker->bb_height = (int)h;
      objecttracker->initDone = FALSE;

      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(objecttracker);
  return ret;
}

gboolean gst_objecttracker_plugin_init(GstPlugin * plugin)
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "objecttracker", GST_RANK_NONE, GST_TYPE_OBJECTTRACKER);
}

//////////////////////////////////////////////////////////////////////////////
static gboolean gst_objecttracker_notify_trackerresults(GstBaseTransform * btrans,
                                                          GstObjecttracker *objecttracker,
                                                          TrackerResult* trackerResult)
{
  GstStructure *str;
  if (trackerResult->success)
	str = gst_structure_new(objecttracker->eventname,
                                          "x", G_TYPE_DOUBLE, (double)trackerResult->boundingBox.x + trackerResult->boundingBox.width/2,
                                          "y", G_TYPE_DOUBLE, (double)trackerResult->boundingBox.y + trackerResult->boundingBox.height/2,
                                          "width", G_TYPE_DOUBLE, (double)trackerResult->boundingBox.width,
                                          "height", G_TYPE_DOUBLE, (double)trackerResult->boundingBox.height,
                                          objecttracker->eventresultname, G_TYPE_BOOLEAN, TRUE, NULL);
  else
	str = gst_structure_new(objecttracker->eventname,
	                                      "x", G_TYPE_DOUBLE, (double)objecttracker->bb_x+ objecttracker->bb_width/2,
	                                      "y", G_TYPE_DOUBLE, (double)objecttracker->bb_y + objecttracker->bb_height/2,
	                                      "width", G_TYPE_DOUBLE, (double)objecttracker->bb_width,
	                                      "height", G_TYPE_DOUBLE, (double)objecttracker->bb_height,
	                                      objecttracker->eventresultname, G_TYPE_BOOLEAN, FALSE, NULL);

  GstStructure *strcpy = gst_structure_copy(str);
  GstEvent* ev = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, str);
  GST_INFO("sending custom DS event of object detection on (%d,%d)",
           trackerResult->boundingBox.x, trackerResult->boundingBox.y);
  gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(&(btrans->element)), ev);
  
  GstMessage *msg;
  msg = gst_message_new_element(GST_OBJECT(objecttracker), strcpy);
  if(gst_element_post_message (GST_ELEMENT (objecttracker), msg) == FALSE)
  {
    printf("Error. objecttracker: Could not send facedata message. gst_element_post_message() failed.");
  }
  
}
