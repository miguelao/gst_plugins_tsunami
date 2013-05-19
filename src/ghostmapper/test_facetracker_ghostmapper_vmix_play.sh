/* GStreamer
 */
/**
 * SECTION:element- ghostmapper
 *
 * This element extracts the person from its background and composes
 * an alpha channel which is added to the output. For the mgmt of the
 * alpha channel, gstalpha plugin is modified from the "good' plugins.
 *
 *
 * The alpha element adds an alpha channel to a video stream.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstghostmapper.h"
#include <gst/controller/gstcontroller.h>

#include <opencv/cv.h>


GST_DEBUG_CATEGORY_STATIC (gst_ghostmapper_debug);
#define GST_CAT_DEFAULT gst_ghostmapper_debug

enum {
	PROP_0,
	PROP_PASSTHROUGH,
	PROP_FPS,
	PROP_STATS,
	PROP_APPLY_CUTOUT,
	PROP_DEBUG_VIEW,
	PROP_LAST
};
static GstStaticPadTemplate gst_ghostmapper_src_template =
		GST_STATIC_PAD_TEMPLATE ("src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV("YUVA"))
);
static GstStaticPadTemplate gst_ghostmapper_sink_template =
		GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV("YUVA"))
);

#define GST_GHOSTMAPPER_LOCK(ghostmapper) G_STMT_START { \
	GST_LOG_OBJECT (ghostmapper, "Locking ghostmapper from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&ghostmapper->lock); \
	GST_LOG_OBJECT (ghostmapper, "Locked ghostmapper from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_GHOSTMAPPER_UNLOCK(ghostmapper) G_STMT_START { \
	GST_LOG_OBJECT (ghostmapper, "Unlocking ghostmapper from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&ghostmapper->lock); \
} G_STMT_END

static gboolean gst_ghostmapper_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static GstCaps *gst_ghostmapper_transform_caps(GstBaseTransform * btrans, GstPadDirection direction, GstCaps * caps);
static gboolean gst_ghostmapper_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_ghostmapper_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_ghostmapper_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_ghostmapper_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_ghostmapper_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_ghostmapper_finalize(GObject * object);

static gboolean gst_ghostmapper_sink_event(GstPad *pad, GstEvent * event);

GST_BOILERPLATE (GstGhostmapper, gst_ghostmapper, GstVideoFilter, GST_TYPE_VIDEO_FILTER);


static void gst_ghostmapper_base_init(gpointer g_class) {
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_set_details_simple(element_class, "Ghostmapper filter", "Filter/Effect/Video",
			"Maps+scales a ghost to the face bbox, creating an alpha channel",
			"Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>" );

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_ghostmapper_sink_template));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_ghostmapper_src_template));

	GST_DEBUG_CATEGORY_INIT (gst_ghostmapper_debug, "ghostmapper", 0, "ghostmapper - Element for adding alpha channel to streams");
}

static void gst_ghostmapper_class_init(GstGhostmapperClass * klass) {
	GObjectClass *gobject_class = (GObjectClass *)klass;
	GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;

	gobject_class->set_property = gst_ghostmapper_set_property;
	gobject_class->get_property = gst_ghostmapper_get_property;
	gobject_class->finalize = gst_ghostmapper_finalize;

	g_object_class_install_property (gobject_class, PROP_PASSTHROUGH,
                                         g_param_spec_boolean (
                                          "passthrough", "Passthrough",
                                          "Activate passthrough option: no cutout performed, alpha channel to 1",
                                          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
	g_object_class_install_property (gobject_class, PROP_FPS,
                                         g_param_spec_boolean (
                                          "fps", "FPS",
                                          "Show frames/second",
                                          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
	g_object_class_install_property(gobject_class, PROP_STATS,
                                        g_param_spec_string(
                                         "stats", "statslog", "statistical info",
                                         "",	(GParamFlags)(G_PARAM_READABLE | GST_PARAM_CONTROLLABLE | G_PARAM_STATIC_STRINGS)));
	g_object_class_install_property (gobject_class, PROP_APPLY_CUTOUT,
                                         g_param_spec_boolean (
                                          "applycutout", "ApplyCutout",
                                          "Apply cutout if so, the alpha channel will be applied to the image, thus seeing the final effect",
                                          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
	g_object_class_install_property(gobject_class, PROP_DEBUG_VIEW,
                                        g_param_spec_boolean (
                                         "debugview", "DebugView",
                                         "Activate passthrough option: no cutout performed, alpha channel to 1",
                                         FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );

	btrans_class->passthrough_on_same_caps = TRUE;
	//btrans_class->always_in_place = TRUE;
	btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_ghostmapper_transform_ip);
	btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_ghostmapper_before_transform);
	btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_ghostmapper_get_unit_size);
	btrans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_ghostmapper_transform_caps);
	btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_ghostmapper_set_caps);
}

static void gst_ghostmapper_init(GstGhostmapper * ghostmapper, GstGhostmapperClass * klass) {
	//gst_base_transform_set_in_place((GstBaseTransform *)ghostmapper, TRUE);
	g_static_mutex_init(&ghostmapper->lock);
	ghostmapper->passthrough = false;
	ghostmapper->apply_cutout = false;
	ghostmapper->fps = false;
	ghostmapper->debug_view = 0;
	ghostmapper->id = (guint)-1;
        ghostmapper->findex = 0;
}

static void ghostmapper_finalize(GstGhostmapper* ghostmapper) 
{
}

static void gst_ghostmapper_finalize(GObject * object) {
	GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (object);

	GST_GHOSTMAPPER_LOCK (ghostmapper);
	ghostmapper_finalize(ghostmapper);
	GST_INFO("Ghostmapper destroyed (%s).", GST_OBJECT_NAME(object));
	GST_GHOSTMAPPER_UNLOCK (ghostmapper);

	g_static_mutex_free(&ghostmapper->lock);

	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_ghostmapper_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (object);
  
  GST_GHOSTMAPPER_LOCK (ghostmapper);
  switch (prop_id) {
  case PROP_PASSTHROUGH:
    ghostmapper->passthrough = g_value_get_boolean(value);
    break;
  case PROP_FPS:
    ghostmapper->fps = g_value_get_boolean(value);
  case PROP_STATS:
    // read only
    break;
  case PROP_APPLY_CUTOUT:
    ghostmapper->apply_cutout = g_value_get_boolean(value);
    break;
  case PROP_DEBUG_VIEW:
    ghostmapper->debug_view = g_value_get_boolean(value);
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_GHOSTMAPPER_UNLOCK (ghostmapper);
}

static void gst_ghostmapper_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (object);
  
  switch (prop_id) {
  case PROP_PASSTHROUGH:
    g_value_set_boolean(value, ghostmapper->passthrough);
    break;
  case PROP_FPS:
    g_value_set_boolean(value, ghostmapper->fps);
    break;
  case PROP_STATS:
    if (ghostmapper->fps) {
      char buffer[1024]={" "};
      g_value_set_string(value, buffer);
    } else {
      g_value_set_string(value, "NA");
    }
    break;
  case PROP_APPLY_CUTOUT:
    g_value_set_boolean(value, ghostmapper->apply_cutout);
    break;
  case PROP_DEBUG_VIEW:
    g_value_set_boolean(value, ghostmapper->debug_view);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_ghostmapper_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}

static GstCaps *
gst_ghostmapper_transform_caps(GstBaseTransform * btrans, GstPadDirection direction, GstCaps * caps) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (btrans);
  GstCaps *ret, *tmp, *tmplt;
  GstStructure *structure;
  gint i;
  
  tmp = gst_caps_new_empty();
  
  GST_GHOSTMAPPER_LOCK (ghostmapper);
  
  for (i = 0; i < (int)gst_caps_get_size(caps); i++) {
    structure = gst_structure_copy(gst_caps_get_structure(caps, i));
    gst_structure_remove_fields(structure, "format", "endianness", "depth", "bpp", "red_mask", "green_mask", "blue_mask", "alpha_mask",
				"palette_data", "color-matrix", "chroma-site", NULL);
    gst_structure_set_name(structure, "video/x-raw-yuv");
    gst_caps_append_structure(tmp, gst_structure_copy(structure));
    gst_structure_free(structure);
  }
  
  if (direction == GST_PAD_SINK) {
    tmplt = gst_static_pad_template_get_caps(&gst_ghostmapper_src_template);
    ret = gst_caps_intersect(tmp, tmplt);
    gst_caps_unref(tmp);
    gst_caps_unref(tmplt);
    tmp = NULL;
  }
  else {
    ret = tmp;
    tmp = NULL;
  }
  
  GST_DEBUG("Transformed %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, caps, ret);
  
  GST_GHOSTMAPPER_UNLOCK (ghostmapper);
  
  return ret;
}


static gboolean gst_ghostmapper_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  // get the plugin's ID
  gchar* szName = gst_element_get_name(ghostmapper);
  ghostmapper->id = (guint)-1;
  for (guint i=0; szName[i] != '\0'; ++i) {
    if (TRUE == g_ascii_isdigit(szName[i])) {
      ghostmapper->id = strtol(szName+i, NULL, 10);
      break;
    }
  }
  if (ghostmapper->id == (guint)-1) {
    GST_ERROR("Could not get id from element name %s", szName);
    return FALSE;
  }
  g_free(szName);
  
  GST_GHOSTMAPPER_LOCK (ghostmapper);
  
  gst_video_format_parse_caps(incaps, &ghostmapper->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &ghostmapper->out_format, &out_width, &out_height);
  if (!(ghostmapper->in_format == ghostmapper->out_format) || !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_GHOSTMAPPER_UNLOCK (ghostmapper);
    return FALSE;
  }
  
  ghostmapper->width = in_width;
  ghostmapper->height = in_height;
  
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(ghostmapper), gst_ghostmapper_sink_event);
  
  GST_GHOSTMAPPER_UNLOCK (ghostmapper);
  
  // init cutout GLSL
  GST_INFO("Initialising Ghostmapper...");
  ghostmapper->x = 0.5 * ghostmapper->width;
  ghostmapper->y = 0.4 * ghostmapper->height;
  ghostmapper->w = ghostmapper->h = 0.2 * ghostmapper->width;
  ghostmapper->isface = false;

  GST_INFO("Ghostmapper initialized.");

  return TRUE;
}

static void gst_ghostmapper_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (btrans);
  GstClockTime timestamp;
  
  timestamp = gst_segment_to_stream_time(&btrans->segment, GST_FORMAT_TIME, GST_BUFFER_TIMESTAMP (buf));
  GST_INFO("Got stream time of %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));
  if (GST_CLOCK_TIME_IS_VALID (timestamp))
    gst_object_sync_values(G_OBJECT(ghostmapper), timestamp);
}

static GstFlowReturn gst_ghostmapper_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (btrans);
  
  ghostmapper->findex++;
  if (ghostmapper->passthrough)
    return GST_FLOW_OK;
  
  GST_GHOSTMAPPER_LOCK (ghostmapper);
  
  unsigned char* buf = GST_BUFFER_DATA(gstbuf);
  
  GST_DEBUG("Position of the face is %d,%d, w %d, h %d", ghostmapper->x, ghostmapper->y, ghostmapper->w, ghostmapper->h);

  if(ghostmapper->apply_cutout == TRUE && ghostmapper->debug_view <= 1){
    uint8_t* data = buf;
    for (gint i = 0; i < ghostmapper->height; i++) {
      for (gint j = 0; j < ghostmapper->width; j++) {
        data[0] = data[0] * data[3] / 255;
        data[1] = data[1] * data[3] / 255;
        data[2] = data[2] * data[3] / 255;
        //		data[0] = data[3];
        //		data[1] = 128;
        //		data[2] = 128;
        data += 4;
      }
    }
  }
  GST_GHOSTMAPPER_UNLOCK (ghostmapper);

  return GST_FLOW_OK;
}

gboolean gst_ghostmapper_sink_event(GstPad *pad, GstEvent * event) 
{
  GstGhostmapper* ghostmapper;
  gboolean ret = FALSE;
  
  ghostmapper = GST_GHOSTMAPPER( gst_pad_get_parent( pad ));
  
  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
    ret = gst_pad_event_default(pad, event);
    break;
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &ghostmapper->x); // check bool return
      gst_structure_get_double(str, "y", &ghostmapper->y); // check bool return
      gst_structure_get_double(str, "width", &ghostmapper->w); // check bool return
      gst_structure_get_double(str, "height", &ghostmapper->h);// check bool return
      gst_structure_get_boolean(str, "facefound", &ghostmapper->isface);// check bool return
      GST_INFO("Received custom event with face detection, (%.2f,%.2f)", ghostmapper->x, ghostmapper->y);
      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(ghostmapper);
  return ret;
}

gboolean gst_ghostmapper_plugin_init(GstPlugin * plugin) 
{
  gst_controller_init(NULL, NULL);
  
  return gst_element_register(plugin, "ghostmapper", GST_RANK_NONE, GST_TYPE_GHOSTMAPPER);
}

////GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
//		GST_VERSION_MINOR,
//		"ghostmapper",
//		"adds an ghostmapper channel to video - constant or via chroma-keying",
//		plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
