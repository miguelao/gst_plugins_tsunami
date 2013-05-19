/* GStreamer
 */

/**
 * SECTION:element- imgsrc
 *
 * Element originally being the videotestsrc:
 * "The videotestsrc element is used to produce test video data in a wide variaty
 * of formats. The video test data produced can be controlled with the "pattern"
 * property."
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v imgsrc location=bg_hills.jpg" ! ximagesink
 * ]| Shows the picture bg_hills.jpg
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstimgsrc.h"
//#include "gstimgsrcorc"
#include "imgsrc.h"
#include <opencv2/imgproc/imgproc_c.h>
#include <gst/video/video.h>

#include <string.h>
#include <stdlib.h>

//#include <sys/time.h>
//static int rubbish;

GST_DEBUG_CATEGORY_STATIC (img_src_debug);
#define GST_CAT_DEFAULT img_src_debug

#define DEFAULT_PATTERN            GST_IMG_SRC_SMPTE
#define DEFAULT_TIMESTAMP_OFFSET   0
#define DEFAULT_IS_LIVE            TRUE
#define DEFAULT_PEER_ALLOC         TRUE
#define DEFAULT_COLOR_SPEC         GST_IMG_SRC_BT601
#define DEFAULT_FOREGROUND_COLOR   0xffffffff
#define DEFAULT_BACKGROUND_COLOR   0xff000000
#define DEFAULT_HORIZONTAL_SPEED   0

enum
{
  PROP_0,
  PROP_TIMESTAMP_OFFSET,
  PROP_IS_LIVE,
  PROP_LOCATION,
  PROP_LAST
};
#define DEFAULT_LOCATION ""


#define IMG_SRC_CAPS      \
    GST_VIDEO_CAPS_GRAY8";"   \
    GST_VIDEO_CAPS_RGB";"     \
    GST_VIDEO_CAPS_BGR";"     \
    GST_VIDEO_CAPS_RGBx";"    \
    GST_VIDEO_CAPS_xRGB";"    \
    GST_VIDEO_CAPS_BGRx";"    \
    GST_VIDEO_CAPS_xBGR";"    \
    GST_VIDEO_CAPS_RGBA";"    \
    GST_VIDEO_CAPS_ARGB";"    \
    GST_VIDEO_CAPS_BGRA";"    \
    GST_VIDEO_CAPS_ABGR";"    \
    GST_VIDEO_CAPS_GRAY16("BIG_ENDIAN")";" \
    GST_VIDEO_CAPS_GRAY16("LITTLE_ENDIAN")";"

static GstStaticPadTemplate gst_img_src_pad_template =
    GST_STATIC_PAD_TEMPLATE ("src",
        GST_PAD_SRC,
        GST_PAD_ALWAYS,
//        GST_STATIC_CAPS_ANY);
        GST_STATIC_CAPS(IMG_SRC_CAPS));

GST_BOILERPLATE (GstImgSrc, gst_img_src, GstPushSrc,
    GST_TYPE_PUSH_SRC);


static void gst_img_src_set_pattern (GstImgSrc * imgsrc,
    int pattern_type);
static void gst_img_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_img_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_img_src_getcaps (GstBaseSrc * bsrc);
static gboolean gst_img_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps);
static void gst_img_src_src_fixate (GstPad * pad, GstCaps * caps);

static gboolean gst_img_src_is_seekable (GstBaseSrc * psrc);
static gboolean gst_img_src_do_seek (GstBaseSrc * bsrc,
    GstSegment * segment);
static gboolean gst_img_src_query (GstBaseSrc * bsrc, GstQuery * query);

static void gst_img_src_get_times (GstBaseSrc * basesrc,
    GstBuffer * buffer, GstClockTime * start, GstClockTime * end);
static GstFlowReturn gst_img_src_create (GstPushSrc * psrc,
    GstBuffer ** buffer);
static gboolean gst_img_src_start (GstBaseSrc * basesrc);


static gboolean gst_img_src_read_and_decode_picture(GstImgSrc * src);
static gchar *gst_img_src_get_filename(GstImgSrc * imgsrc);
static gboolean gst_img_src_set_location(GstImgSrc * src, const gchar * location);

static void
gst_img_src_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "Image source", "Source/Video",
      "Creates a video stream from a picture", "Jan Van Winkel <jan.van_winkel@alcatel-lucent.com> , Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com");

  gst_element_class_add_pad_template(element_class,
      gst_static_pad_template_get(&gst_img_src_pad_template));

  // gst_element_class_add_pad_template (element_class,
  //       gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, gst_img_src_getcaps (NULL)));
}

static void
gst_img_src_class_init (GstImgSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_img_src_set_property;
  gobject_class->get_property = gst_img_src_get_property;

  g_object_class_install_property (gobject_class, PROP_TIMESTAMP_OFFSET,
      g_param_spec_int64 ("timestamp-offset", "Timestamp offset",
          "An offset added to timestamps set on buffers (in ns)", G_MININT64,
          G_MAXINT64, 0, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_IS_LIVE,
      g_param_spec_boolean ("is-live", "Is Live",
          "Whether to act as a live source", DEFAULT_IS_LIVE,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(
      gobject_class,
      PROP_LOCATION,
      g_param_spec_string("location", "File Location",
          "Location of the Picture File", DEFAULT_LOCATION,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  gstbasesrc_class->get_caps = gst_img_src_getcaps;
  gstbasesrc_class->set_caps = gst_img_src_setcaps;
  gstbasesrc_class->is_seekable = gst_img_src_is_seekable;
  gstbasesrc_class->do_seek = gst_img_src_do_seek;
  gstbasesrc_class->query = gst_img_src_query;
  gstbasesrc_class->get_times = gst_img_src_get_times;
  gstbasesrc_class->start = gst_img_src_start;

  gstpushsrc_class->create = gst_img_src_create;
}

static void
gst_img_src_init (GstImgSrc * src, GstImgSrcClass * g_class)
{
  GstPad *pad = GST_BASE_SRC_PAD (src);

  gst_pad_set_fixatecaps_function (pad, gst_img_src_src_fixate);

  gst_img_src_set_pattern (src, DEFAULT_PATTERN);

  src->timestamp_offset = DEFAULT_TIMESTAMP_OFFSET;
  src->foreground_color = DEFAULT_FOREGROUND_COLOR;
  src->background_color = DEFAULT_BACKGROUND_COLOR;
  src->horizontal_speed = DEFAULT_HORIZONTAL_SPEED;

  src->filename = g_strdup(DEFAULT_LOCATION);

  /* we operate in time */
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (src), DEFAULT_IS_LIVE);
  src->peer_alloc = DEFAULT_PEER_ALLOC;
}

static void
gst_img_src_src_fixate (GstPad * pad, GstCaps * caps)
{
  GstStructure *structure;

  structure = gst_caps_get_structure (caps, 0);

  gst_structure_fixate_field_nearest_int (structure, "width", 320);
  gst_structure_fixate_field_nearest_int (structure, "height", 240);
  gst_structure_fixate_field_nearest_fraction (structure, "framerate", 30, 1);
  if (gst_structure_has_field (structure, "pixel-aspect-ratio"))
    gst_structure_fixate_field_nearest_fraction (structure,
        "pixel-aspect-ratio", 1, 1);
  if (gst_structure_has_field (structure, "color-matrix"))
    gst_structure_fixate_field_string (structure, "color-matrix", "sdtv");
  if (gst_structure_has_field (structure, "chroma-site"))
    gst_structure_fixate_field_string (structure, "chroma-site", "mpeg2");

  if (gst_structure_has_field (structure, "interlaced"))
    gst_structure_fixate_field_boolean (structure, "interlaced", FALSE);
}

static void
gst_img_src_set_pattern (GstImgSrc * imgsrc,
    int pattern_type)
{
  imgsrc->pattern_type = (GstImgSrcPattern)pattern_type;

  GST_DEBUG_OBJECT (imgsrc, "setting pattern to %d", pattern_type);

/*  switch (pattern_type) {
    case GST_IMG_SRC_SMPTE:
      imgsrc->make_image = gst_img_src_smpte;
      break;
    case GST_IMG_SRC_SNOW:
      imgsrc->make_image = gst_img_src_snow;
      break;
    case GST_IMG_SRC_BLACK:
      imgsrc->make_image = gst_img_src_black;
      break;
    case GST_IMG_SRC_WHITE:
      imgsrc->make_image = gst_img_src_white;
      break;
    case GST_IMG_SRC_RED:
      imgsrc->make_image = gst_img_src_red;
      break;
    case GST_IMG_SRC_GREEN:
      imgsrc->make_image = gst_img_src_green;
      break;
    case GST_IMG_SRC_BLUE:
      imgsrc->make_image = gst_img_src_blue;
      break;
    case GST_IMG_SRC_CHECKERS1:
      imgsrc->make_image = gst_img_src_checkers1;
      break;
    case GST_IMG_SRC_CHECKERS2:
      imgsrc->make_image = gst_img_src_checkers2;
      break;
    case GST_IMG_SRC_CHECKERS4:
      imgsrc->make_image = gst_img_src_checkers4;
      break;
    case GST_IMG_SRC_CHECKERS8:
      imgsrc->make_image = gst_img_src_checkers8;
      break;
    case GST_IMG_SRC_CIRCULAR:
      imgsrc->make_image = gst_img_src_circular;
      break;
    case GST_IMG_SRC_BLINK:
      imgsrc->make_image = gst_img_src_blink;
      break;
    case GST_IMG_SRC_SMPTE75:
      imgsrc->make_image = gst_img_src_smpte75;
      break;
    case GST_IMG_SRC_ZONE_PLATE:
      imgsrc->make_image = gst_img_src_zoneplate;
      break;
    case GST_IMG_SRC_GAMUT:
      imgsrc->make_image = gst_img_src_gamut;
      break;
    case GST_IMG_SRC_CHROMA_ZONE_PLATE:
      imgsrc->make_image = gst_img_src_chromazoneplate;
      break;
    case GST_IMG_SRC_SOLID:
      imgsrc->make_image = gst_img_src_solid;
      break;
    case GST_IMG_SRC_BALL:
      imgsrc->make_image = gst_img_src_ball;
      break;
    case GST_IMG_SRC_SMPTE100:
      imgsrc->make_image = gst_img_src_smpte100;
      break;
    case GST_IMG_SRC_BAR:
      imgsrc->make_image = gst_img_src_bar;
      break;
    default:
      g_assert_not_reached ();
  }
  */
}

static void
gst_img_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstImgSrc *src = GST_IMG_SRC (object);

  switch (prop_id) {
  case PROP_TIMESTAMP_OFFSET:
    src->timestamp_offset = g_value_get_int64 (value);
    break;
  case PROP_IS_LIVE:
    gst_base_src_set_live (GST_BASE_SRC (src), g_value_get_boolean (value));
    break;
  case PROP_LOCATION:
    gst_img_src_set_location(src, g_value_get_string(value));
    break;
  default:
    break;
  }
}

static void
gst_img_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstImgSrc *src = GST_IMG_SRC (object);

  switch (prop_id) {
  case PROP_TIMESTAMP_OFFSET:
    g_value_set_int64 (value, src->timestamp_offset);
    break;
  case PROP_IS_LIVE:
    g_value_set_boolean (value, gst_base_src_is_live (GST_BASE_SRC (src)));
    break;
  case PROP_LOCATION:
    g_value_set_string(value, src->filename);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

/* threadsafe because this gets called as the plugin is loaded */
static GstCaps *
gst_img_src_getcaps (GstBaseSrc * bsrc)
{
  GstCaps *caps = gst_caps_new_empty();
  gst_caps_append(caps, gst_caps_from_string(GST_VIDEO_CAPS_RGB));

//  if (!capslist) {
//    GstCaps *caps;
//    GstStructure *structure;
//    int i;
//
//    caps = gst_caps_new_empty ();
//    for (i = 0; i < n_fourccs; i++) {
//      structure = paint_get_structure (fourcc_list + i);
//      gst_structure_set (structure,
//          "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
//          "height", GST_TYPE_INT_RANGE, 1, G_MAXINT,
//          "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1, NULL);
//      gst_caps_append_structure (caps, structure);
//    }
//
//    capslist = caps;
//  }
//
//  return gst_caps_copy (capslist);
  return caps;
}

static gboolean
gst_img_src_parse_caps (const GstCaps * caps,
    gint * width, gint * height, gint * rate_numerator, gint * rate_denominator,
    struct fourcc_list_struct **fourcc, GstImgSrcColorSpec * color_spec)
{
  const GstStructure *structure;
  GstPadLinkReturn ret, ret1;
  const GValue *framerate;
  const char *csp;

  GST_DEBUG ("parsing caps");

  if (gst_caps_get_size (caps) < 1)
    return FALSE;

  structure = gst_caps_get_structure (caps, 0);

  if (!(*fourcc = paintinfo_find_by_structure (structure)))
    goto unknown_format;

  ret1  = (GstPadLinkReturn)gst_structure_get_int (structure, "width", width);
  ret   = (GstPadLinkReturn)(gst_structure_get_int (structure, "height", height));
  framerate = gst_structure_get_value (structure, "framerate");

  if (framerate) {
    *rate_numerator = gst_value_get_fraction_numerator (framerate);
    *rate_denominator = gst_value_get_fraction_denominator (framerate);
  } else
    goto no_framerate;

  csp = gst_structure_get_string (structure, "color-matrix");
  if (csp) {
    if (strcmp (csp, "sdtv") == 0) {
      *color_spec = GST_IMG_SRC_BT601;
    } else if (strcmp (csp, "hdtv") == 0) {
      *color_spec = GST_IMG_SRC_BT709;
    } else {
      GST_DEBUG ("unknown color-matrix");
      return FALSE;
    }
  } else {
    *color_spec = GST_IMG_SRC_BT601;
  }

  return ret;

  /* ERRORS */
unknown_format:
  {
    GST_DEBUG ("imgsrc format not found");
    return FALSE;
  }
no_framerate:
  {
    GST_DEBUG ("imgsrc no framerate given");
    return FALSE;
  }
}

static gboolean
gst_img_src_setcaps (GstBaseSrc * bsrc, GstCaps * caps)
{
  gboolean res;
  gint width, height, rate_denominator, rate_numerator;
  struct fourcc_list_struct *fourcc;
  GstImgSrc *imgsrc;
  GstImgSrcColorSpec color_spec;

  imgsrc = GST_IMG_SRC (bsrc);

  res = gst_img_src_parse_caps (caps, &width, &height,
      &rate_numerator, &rate_denominator, &fourcc, &color_spec);
  if (res) {
    /* looks ok here */
    imgsrc->fourcc = fourcc;
    imgsrc->width = width;
    imgsrc->height = height;
    imgsrc->rate_numerator = rate_numerator;
    imgsrc->rate_denominator = rate_denominator;
    imgsrc->bpp = imgsrc->fourcc->bitspp;
    imgsrc->color_spec = color_spec;

    GST_DEBUG_OBJECT (imgsrc, "size %dx%d, %d/%d fps, %d depth, %d bpp",
                      imgsrc->width, imgsrc->height,
                      imgsrc->rate_numerator, imgsrc->rate_denominator,
                      imgsrc->fourcc->depth, imgsrc->fourcc->bitspp );
  }

  cvReleaseImage(&(imgsrc->img));
  imgsrc->img = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U, imgsrc->bpp/8 );

  return res;
}

static gboolean
gst_img_src_query (GstBaseSrc * bsrc, GstQuery * query)
{
  gboolean res;
  GstImgSrc *src;

  src = GST_IMG_SRC (bsrc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
    {
      GstFormat src_fmt, dest_fmt;
      gint64 src_val, dest_val;

      gst_query_parse_convert (query, &src_fmt, &src_val, &dest_fmt, &dest_val);
      if (src_fmt == dest_fmt) {
        dest_val = src_val;
        goto done;
      }

      switch (src_fmt) {
        case GST_FORMAT_DEFAULT:
          switch (dest_fmt) {
            case GST_FORMAT_TIME:
              /* frames to time */
              if (src->rate_numerator) {
                dest_val = gst_util_uint64_scale (src_val,
                    src->rate_denominator * GST_SECOND, src->rate_numerator);
              } else {
                dest_val = 0;
              }
              break;
            default:
              goto error;
          }
          break;
        case GST_FORMAT_TIME:
          switch (dest_fmt) {
            case GST_FORMAT_DEFAULT:
              /* time to frames */
              if (src->rate_numerator) {
                dest_val = gst_util_uint64_scale (src_val,
                    src->rate_numerator, src->rate_denominator * GST_SECOND);
              } else {
                dest_val = 0;
              }
              break;
            default:
              goto error;
          }
          break;
        default:
          goto error;
      }
    done:
      gst_query_set_convert (query, src_fmt, src_val, dest_fmt, dest_val);
      res = TRUE;
      break;
    }
    default:
      res = GST_BASE_SRC_CLASS (parent_class)->query (bsrc, query);
  }
  return res;

  /* ERROR */
error:
  {
    GST_DEBUG_OBJECT (src, "query failed");
    return FALSE;
  }
}

static void
gst_img_src_get_times (GstBaseSrc * basesrc, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end)
{
  /* for live sources, sync on the timestamp of the buffer */
  if (gst_base_src_is_live (basesrc)) {
    GstClockTime timestamp = GST_BUFFER_TIMESTAMP (buffer);

    if (GST_CLOCK_TIME_IS_VALID (timestamp)) {
      /* get duration to calculate end time */
      GstClockTime duration = GST_BUFFER_DURATION (buffer);

      if (GST_CLOCK_TIME_IS_VALID (duration)) {
        *end = timestamp + duration;
      }
      *start = timestamp;
    }
  } else {
    *start = -1;
    *end = -1;
  }
}

static gboolean
gst_img_src_do_seek (GstBaseSrc * bsrc, GstSegment * segment)
{
  GstClockTime time;
  GstImgSrc *src;

  src = GST_IMG_SRC (bsrc);

  segment->time = segment->start;
  time = segment->last_stop;

  /* now move to the time indicated */
  if (src->rate_numerator) {
    src->n_frames = gst_util_uint64_scale (time,
        src->rate_numerator, src->rate_denominator * GST_SECOND);
  } else {
    src->n_frames = 0;
  }
  if (src->rate_numerator) {
    src->running_time = gst_util_uint64_scale (src->n_frames,
        src->rate_denominator * GST_SECOND, src->rate_numerator);
  } else {
    /* FIXME : Not sure what to set here */
    src->running_time = 0;
  }

  g_assert (src->running_time <= time);

  return TRUE;
}

static gboolean
gst_img_src_is_seekable (GstBaseSrc * psrc)
{
  /* we're seekable... */
  return TRUE;
}
static GstFlowReturn
gst_img_src_create (GstPushSrc * psrc, GstBuffer ** buffer)
{
  GstImgSrc *src;
  gulong newsize;
  GstBuffer *outbuf = NULL;
  GstClockTime next_time;

  src = GST_IMG_SRC (psrc);

  if (G_UNLIKELY (src->fourcc == NULL))
    goto not_negotiated;

  /* 0 framerate and we are at the second frame, eos */
  if (G_UNLIKELY (src->rate_numerator == 0 && src->n_frames == 1))
    goto eos;

  newsize = gst_img_src_get_size (src, src->width, src->height);

  g_return_val_if_fail (newsize > 0, GST_FLOW_ERROR);

  GST_LOG_OBJECT (src,
      "creating buffer of %lu bytes with %dx%d image for frame %d", newsize,
      src->width, src->height, (gint) src->n_frames);
/*
  if (src->peer_alloc) {
    res =
        gst_pad_alloc_buffer_and_set_caps (GST_BASE_SRC_PAD (psrc),
        GST_BUFFER_OFFSET_NONE, newsize, GST_PAD_CAPS (GST_BASE_SRC_PAD (psrc)),
        &outbuf);
    if (res != GST_FLOW_OK)
      goto no_buffer;

    // the buffer could have renegotiated, we need to discard any buffers of the
    // wrong size.
    size = GST_BUFFER_SIZE (outbuf);
    newsize = gst_img_src_get_size (src, src->width, src->height);

    if (size != newsize) {
      gst_buffer_unref (outbuf);
      outbuf = NULL;
    }
  }
*/

  if (outbuf == NULL) {
    outbuf = gst_buffer_new();
    gst_buffer_set_caps (outbuf, GST_PAD_CAPS (GST_BASE_SRC_PAD (psrc)));
  }
  //////////////////////////////////////////////////////////////////////////////
  if (src->new_file == TRUE){
    if(!gst_img_src_read_and_decode_picture(src))
      return GST_FLOW_ERROR;
    else
      src->new_file = FALSE;
  }
  GST_BUFFER_DATA       (outbuf) = (guint8 *)src->img->imageData;
  GST_BUFFER_MALLOCDATA (outbuf) = NULL;
  GST_BUFFER_SIZE       (outbuf) = src->img->imageSize;

  //////////////////////////////////////////////////////////////////////////////
  //struct timeval tv;
  //struct timezone tz;
  //gettimeofday(&tv, &tz);
  //printf(" timestamp %d.%d (that is %dus diff, %ffps versus wanted %d/%dfps),\n", 
  //       tv.tv_sec, tv.tv_usec, tv.tv_usec-rubbish, 1.0E6/( tv.tv_usec-rubbish ), src->rate_numerator, src->rate_denominator);
  //rubbish = tv.tv_usec;
  //////////////////////////////////////////////////////////////////////////////


  GST_BUFFER_TIMESTAMP (outbuf) = src->timestamp_offset + src->running_time;
  GST_BUFFER_OFFSET (outbuf) = src->n_frames;
  src->n_frames++;
  GST_BUFFER_OFFSET_END (outbuf) = src->n_frames;
  if (src->rate_numerator) {
    next_time = gst_util_uint64_scale_int (src->n_frames * GST_SECOND,
        src->rate_denominator, src->rate_numerator);
    GST_BUFFER_DURATION (outbuf) = next_time - src->running_time;
  } else {
    next_time = src->timestamp_offset;
    /* NONE means forever */
    GST_BUFFER_DURATION (outbuf) = GST_CLOCK_TIME_NONE;
  }

  src->running_time = next_time;

  *buffer = outbuf;

  return GST_FLOW_OK;

not_negotiated:
  {
    GST_ELEMENT_ERROR (src, CORE, NEGOTIATION, (NULL),
        ("format wasn't negotiated before get function"));
    return GST_FLOW_NOT_NEGOTIATED;
  }
eos:
  {
    GST_DEBUG_OBJECT (src, "eos: 0 framerate, frame %d", (gint) src->n_frames);
    return GST_FLOW_UNEXPECTED;
  }
//no_buffer:
//  {
//    GST_DEBUG_OBJECT (src, "could not allocate buffer, reason %s",
//        gst_flow_get_name (res));
//    return res;
//  }
}

static gboolean
gst_img_src_start (GstBaseSrc * basesrc)
{
  GstImgSrc *src = GST_IMG_SRC (basesrc);

  src->running_time = 0;
  src->n_frames = 0;

  return TRUE;
}

gboolean gst_img_src_plugin_init (GstPlugin * plugin)
{
  //gst_imgsrc_orc_init ();

  GST_DEBUG_CATEGORY_INIT (img_src_debug, "imgsrc", 0,
      "Picture as Stream Source");

  return gst_element_register (plugin, "imgsrc", GST_RANK_NONE,
      GST_TYPE_IMG_SRC);
}

/*
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "imgsrc",
    "Creates a test video stream",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
*/

static gchar *
gst_img_src_get_filename(GstImgSrc * imgsrc) 
{
  gchar *filename;

  filename = g_strdup(imgsrc->filename);

  return filename;
}

static gboolean gst_img_src_read_and_decode_picture(GstImgSrc * src)
{

  gchar *filename;

  IplImage *img_in;
  IplImage *img_scale;

  GST_TRACE_OBJECT(src,"Freeing Img object to allocate new one");

  filename = gst_img_src_get_filename(src);

  GST_DEBUG_OBJECT(src, "reading and converting from file \"%s\"", filename);
  img_in = cvLoadImage(filename,CV_LOAD_IMAGE_UNCHANGED);

  if (img_in == NULL) {
      GST_ELEMENT_ERROR (src, RESOURCE, READ,("Error while reading from file \"%s\".", filename),(""));
      return FALSE;
  }

  img_scale = cvCreateImage(cvGetSize(src->img),src->img->depth,3);
  cvResize(img_in,img_scale,CV_INTER_CUBIC);

  if(src->img->nChannels == 4){
    cvCvtColor(img_scale,src->img,CV_BGR2RGBA);
  } else if(src->img->nChannels == 3){
    cvCvtColor(img_scale,src->img,CV_BGR2RGB);
  } else if(src->img->nChannels == 1){
    cvCvtColor(img_scale,src->img,CV_BGR2GRAY);
  }

  cvReleaseImage(&img_in);

  return TRUE;

}

static gboolean gst_img_src_set_location(GstImgSrc * src, const gchar * location) 
{
  g_free(src->filename);
  if (location != NULL) {
    src->filename = g_strdup(location);
  } else {
    src->filename = NULL;
  }
  src->new_file = TRUE;

  return TRUE;
}

// EOF

