/* GStreamer Inet IPP based H264 encoder plugin
 */

#include "gsth264enc.h"

#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (h264_enc_debug);
#define GST_CAT_DEFAULT h264_enc_debug
#define PACKAGE "imco-gst-ipph264"
#define  GST_PACKAGE_ORIGIN  "http://software.intel.com/en-us/articles/intel-integrated-performance-primitives-code-samples"
enum
{
  ARG_0,
  ARG_THREADS,
  ARG_BYTE_STREAM,
  ARG_BITRATE,
  ARG_BFRAMES,
  ARG_REF,
  ARG_KEYINT,
  ARG_PROFILE,
  ARG_TUNE,
};

#define ARG_THREADS_DEFAULT            0        /* 0 means 'auto' which is 1.5x number of CPU cores */
#define ARG_BYTE_STREAM_DEFAULT        FALSE
#define ARG_BITRATE_DEFAULT            (2 * 1024)
#define ARG_BFRAMES_DEFAULT            3
#define ARG_REF_DEFAULT                4
#define ARG_KEYINT_DEFAULT             200
#define h264_picture_t UMC::VideoData
static GString *h264enc_defaults;




static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-yuv, "
        "format = (fourcc) { I420, YV12, I422  }, "
        "framerate = (fraction) [0, MAX], "
        "width = (int) [ 16, MAX ], " "height = (int) [ 16, MAX ]")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "framerate = (fraction) [0/1, MAX], "
        "width = (int) [ 1, MAX ], " "height = (int) [ 1, MAX ], "
        "stream-format = (string) { byte-stream, avc }, "
        "alignment = (string) { au }")
    );

static void gst_h264_enc_finalize (GObject * object);
static void gst_h264_enc_reset (GstH264Enc * encoder);

static gboolean gst_h264_enc_init_encoder (GstH264Enc * encoder);
static void gst_h264_enc_close_encoder (GstH264Enc * encoder);

static gboolean gst_h264_enc_sink_set_caps (GstPad * pad, GstCaps * caps);
static GstCaps *gst_h264_enc_sink_get_caps (GstPad * pad);
static gboolean gst_h264_enc_sink_event (GstPad * pad, GstEvent * event);
static gboolean gst_h264_enc_src_event (GstPad * pad, GstEvent * event);
static GstFlowReturn gst_h264_enc_chain (GstPad * pad, GstBuffer * buf);
static void gst_h264_enc_flush_frames (GstH264Enc * encoder, gboolean send);
static GstFlowReturn gst_h264_enc_encode_frame (GstH264Enc * encoder,
    h264_picture_t * pic_in, int *i_nal, gboolean send);
static GstStateChangeReturn gst_h264_enc_change_state (GstElement * element,
    GstStateChange transition);

static void gst_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
_do_init (GType object_type)
{
  const GInterfaceInfo preset_interface_info = {
    NULL,                       /* interface_init */
    NULL,                       /* interface_finalize */
    NULL                        /* interface_data */
  };

  g_type_add_interface_static (object_type, GST_TYPE_PRESET,
      &preset_interface_info);
}

GST_BOILERPLATE_FULL (GstH264Enc, gst_h264_enc, GstElement, GST_TYPE_ELEMENT,
    _do_init);

static void
gst_h264_enc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (element_class,
      "H264 Encoder", "Codec/Encoder/Video", "Intel IPP based H264 Encoder",
      "Vlad Gridin <vladyslav.gridin@alcatel-lucent.com> ");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_factory));
}


static void
gst_h264_enc_class_init (GstH264EncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  //const gchar *partitions = NULL;

  h264enc_defaults = g_string_new ("");

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_h264_enc_set_property;
  gobject_class->get_property = gst_h264_enc_get_property;
  gobject_class->finalize = gst_h264_enc_finalize;

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_h264_enc_change_state);

  /* options for which we don't use string equivalents */
  g_object_class_install_property (gobject_class, ARG_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate", "Bitrate in kbit/sec", 1,
          2 * 1024, ARG_BITRATE_DEFAULT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING)));


 			 
  /* options for which we _do_ use string equivalents */
  g_object_class_install_property (gobject_class, ARG_THREADS,
      g_param_spec_uint ("threads", "Threads",
          "Number of threads used by the codec (0 for automatic)",
          0, 4, ARG_THREADS_DEFAULT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  /* NOTE: this first string append doesn't require the ':' delimiter but the
   * rest do */
  g_string_append_printf (h264enc_defaults, "threads=%d", ARG_THREADS_DEFAULT);


  g_object_class_install_property (gobject_class, ARG_BYTE_STREAM,
      g_param_spec_boolean ("byte-stream", "Byte Stream",
          "Generate byte stream format of NALU", ARG_BYTE_STREAM_DEFAULT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_string_append_printf (h264enc_defaults, ":annexb=%d",
      ARG_BYTE_STREAM_DEFAULT);
   g_object_class_install_property (gobject_class, ARG_REF,
      g_param_spec_uint ("ref", "Reference Frames",
          "Number of reference frames",
          2, 16, ARG_REF_DEFAULT, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_string_append_printf (h264enc_defaults, ":ref=%d", ARG_REF_DEFAULT);

  g_object_class_install_property (gobject_class, ARG_BFRAMES,
      g_param_spec_uint ("bframes", "B-Frames",
          "Number of B-frames between I and P",
          0, 4, ARG_BFRAMES_DEFAULT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_string_append_printf (h264enc_defaults, ":bframes=%d", ARG_BFRAMES_DEFAULT);
  
  g_object_class_install_property (gobject_class, ARG_KEYINT,
      g_param_spec_uint ("key-int", "Key-frame interval",
          "Distance between two key-frames ",
          0, G_MAXINT, ARG_KEYINT_DEFAULT,
          (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

 }


/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_h264_enc_init (GstH264Enc * encoder, GstH264EncClass * klass)
{
  encoder->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_setcaps_function (encoder->sinkpad,
      GST_DEBUG_FUNCPTR (gst_h264_enc_sink_set_caps));
  gst_pad_set_getcaps_function (encoder->sinkpad,
      GST_DEBUG_FUNCPTR (gst_h264_enc_sink_get_caps));
  gst_pad_set_event_function (encoder->sinkpad,
      GST_DEBUG_FUNCPTR (gst_h264_enc_sink_event));
  gst_pad_set_chain_function (encoder->sinkpad,
      GST_DEBUG_FUNCPTR (gst_h264_enc_chain));
  gst_element_add_pad (GST_ELEMENT (encoder), encoder->sinkpad);

  encoder->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  gst_pad_use_fixed_caps (encoder->srcpad);
  gst_element_add_pad (GST_ELEMENT (encoder), encoder->srcpad);

  gst_pad_set_event_function (encoder->srcpad,
      GST_DEBUG_FUNCPTR (gst_h264_enc_src_event));

  /* properties */
  encoder->threads = ARG_THREADS_DEFAULT;
  encoder->byte_stream = ARG_BYTE_STREAM_DEFAULT;
  encoder->bitrate = ARG_BITRATE_DEFAULT;
  encoder->bframes = ARG_BFRAMES_DEFAULT;
  encoder->ref     = ARG_REF_DEFAULT;
  encoder->keyint  = ARG_KEYINT_DEFAULT;
  /* resources */
  encoder->delay = g_queue_new ();
  
  encoder->threads = ARG_THREADS_DEFAULT;
  encoder->bitrate = ARG_BITRATE_DEFAULT;
  encoder->bframes = ARG_BFRAMES_DEFAULT;
  encoder->option_string = g_string_new (NULL);
  //encoder->option_string_prop = g_string_new (ARG_OPTION_STRING_DEFAULT);

 GST_DEBUG_OBJECT(encoder, "gst_h264_enc_init");

  

  if (ippStaticInit() < ippStsNoErr) {
    GST_ERROR_OBJECT(encoder, "Error: Can't initialize ipp libs!");
  }
  
  encoder->h264param = new UMC::H264EncoderParams();  
   
  encoder->h264enc = new UMC::H264VideoEncoder();
   
  gst_h264_enc_reset (encoder);
}

static void
gst_h264_enc_reset (GstH264Enc * encoder)
{
  encoder->h264enc->Reset();
  encoder->width = 0;
  encoder->height = 0;
  GST_DEBUG_OBJECT(encoder, "gst_h264_enc_reset");
  
  GST_OBJECT_LOCK (encoder);
  // FIXME
  encoder->i_type = 1;
  if (encoder->forcekeyunit_event)
    gst_event_unref (encoder->forcekeyunit_event);
  encoder->forcekeyunit_event = NULL;
  GST_OBJECT_UNLOCK (encoder);


}

static void
gst_h264_enc_finalize (GObject * object)
{
  GstH264Enc *encoder = GST_H264_ENC (object);
  GST_DEBUG_OBJECT(encoder, "gst_h264_enc_finalize");
  gst_h264_enc_close_encoder (encoder);
   if (encoder->h264param != NULL) {
    delete encoder->h264param;
    encoder->h264param = NULL;
	}
  G_OBJECT_CLASS (parent_class)->finalize (object);
}



/*
 * gst_h264_enc_init_encoder
 * @encoder:  Encoder which should be initialized.
 *
 * Initialize h264 encoder.
 *
 */
static gboolean
gst_h264_enc_init_encoder (GstH264Enc * encoder)
{
  

  /* make sure that the encoder is closed */
  gst_h264_enc_close_encoder (encoder);

  GST_DEBUG_OBJECT(encoder, "gst_h264_enc_init_encoder");
  GST_OBJECT_LOCK (encoder);



 /* set up encoder parameters */
  encoder->h264param->info.clip_info.width = encoder->width;
  encoder->h264param->info.clip_info.height = encoder->height;
  encoder->h264param->info.framerate = encoder->fps_num;
  encoder->h264param->info.bitrate = (encoder->bitrate * 1024);
  encoder->h264param->numThreads = encoder->threads;
  encoder->h264param->B_frame_rate = encoder->bframes;
  encoder->h264param->num_ref_frames = encoder->ref;
  encoder->h264param->key_frame_controls.interval = encoder->keyint;
  encoder->h264param->key_frame_controls.method = 1;
  encoder->h264param->key_frame_controls.idr_interval = 1;
  encoder->h264param->profile_idc = (UMC::H264_PROFILE_IDC)100;
  encoder->h264param->chroma_format_idc = 1;
  encoder->h264param->bit_depth_luma = 8;
  encoder->h264param->bit_depth_chroma = 8;
  encoder->reconfig = FALSE;  
	
  GST_OBJECT_UNLOCK (encoder);

  /* init ipp encoder */  
  encoder->h264enc = new UMC::H264VideoEncoder();
  if (encoder->h264enc->Init(encoder->h264param) != UMC::UMC_OK) {
      GST_ERROR_OBJECT (encoder, 
        "Can not initialize h264 encoder.");
		return FALSE;
  }

  if (encoder->h264enc->GetInfo(encoder->h264param) != UMC::UMC_OK) {
    GST_ERROR_OBJECT (encoder, "Error: Video encoder GetInfo failed");
	 return FALSE;
  }

  GST_DEBUG_OBJECT (encoder,"H264 encoder initialized ");
  GST_INFO_OBJECT(encoder, "wxh (%dx%d), bitrate %d, colorformat %d, framerate %d, chroma_format_idc %d", 
   												  encoder->h264param->info.clip_info.width, 
   												  encoder->h264param->info.clip_info.height,
													  encoder->h264param->info.bitrate,
													  encoder->h264param->info.color_format,
													  encoder->h264param->info.framerate,
													  encoder->h264param->chroma_format_idc);
  return TRUE;
}

/* gst_h264_enc_close_encoder
 * @encoder:  Encoder which should close.
 *
 * Close h264 encoder.
 */
static void
gst_h264_enc_close_encoder (GstH264Enc * encoder)
{
  
  if (encoder) GST_DEBUG_OBJECT (encoder,"gst_h264_enc_close_encoder");
  if (encoder->h264enc != NULL) {
    encoder->h264enc->Close();
    delete encoder->h264enc;
    encoder->h264enc = NULL;
  }
  
 
}

/*
 * Returns: Buffer with the stream headers.
 */
static GstBuffer *
gst_h264_enc_header_buf (GstH264Enc * encoder)
{
  GstBuffer *buf = NULL;

  return buf;
}

/* gst_h264_enc_set_src_caps
 * Returns: TRUE on success.
 */
static gboolean
gst_h264_enc_set_src_caps (GstH264Enc * encoder, GstPad * pad, GstCaps * caps)
{
  GstBuffer *buf;
  GstCaps *outcaps;
  GstStructure *structure;
  gboolean res;
  GST_DEBUG_OBJECT (encoder,"gst_h264_enc_set_src_caps");
  outcaps = gst_caps_new_simple ("video/x-h264",
      "width", G_TYPE_INT, encoder->width,
      "height", G_TYPE_INT, encoder->height,
      "framerate", GST_TYPE_FRACTION, encoder->fps_num, encoder->fps_den,
      "pixel-aspect-ratio", GST_TYPE_FRACTION, encoder->par_num,
      encoder->par_den, NULL);

  structure = gst_caps_get_structure (outcaps, 0);

  if (!encoder->byte_stream) {
    buf = gst_h264_enc_header_buf (encoder);
    if (buf != NULL) {
      gst_caps_set_simple (outcaps, "codec_data", GST_TYPE_BUFFER, buf, NULL);
      gst_buffer_unref (buf);
    }
    gst_structure_set (structure, "stream-format", G_TYPE_STRING, "avc", NULL);
  } else {
    gst_structure_set (structure, "stream-format", G_TYPE_STRING, "byte-stream",
        NULL);
  }
  gst_structure_set (structure, "alignment", G_TYPE_STRING, "au", NULL);

  res = gst_pad_set_caps (pad, outcaps);
  gst_caps_unref (outcaps);

  return res;
}

static gboolean
gst_h264_enc_sink_set_caps (GstPad * pad, GstCaps * caps)
{
  GstH264Enc *encoder = GST_H264_ENC (GST_OBJECT_PARENT (pad));
  GstVideoFormat format;
  gint width, height;
  gint fps_num, fps_den;
  gint par_num, par_den;
  gint i;

GST_DEBUG_OBJECT (encoder,"gst_h264_enc_sink_set_caps");
  /* get info from caps */
  if (!gst_video_format_parse_caps (caps, &format, &width, &height))
    return FALSE;
  if (!gst_video_parse_caps_framerate (caps, &fps_num, &fps_den))
    return FALSE;
  if (!gst_video_parse_caps_pixel_aspect_ratio (caps, &par_num, &par_den)) {
    par_num = 1;
    par_den = 1;
  }

  /* If the encoder is initialized, do not reinitialize it again if not
   * necessary */
  if (encoder->h264enc) {
    if (width == encoder->width && height == encoder->height
        && fps_num == encoder->fps_num && fps_den == encoder->fps_den
        && par_num == encoder->par_num && par_den == encoder->par_den)
      return TRUE;

    /* clear out pending frames */
    gst_h264_enc_flush_frames (encoder, TRUE);

    
  }

  /* store input description */
  encoder->format = format;
  encoder->width = width;
  encoder->height = height;
  encoder->fps_num = fps_num;
  encoder->fps_den = fps_den;
  encoder->par_num = par_num;
  encoder->par_den = par_den;

  /* prepare a cached image description */
  encoder->image_size = gst_video_format_get_size (encoder->format, width,
      height);
  for (i = 0; i < 3; ++i) {
    /* only offsets now, is shifted later. Offsets will be for Y, U, V so we
     * can just feed YV12 as I420 to the decoder later */
    encoder->offset[i] = gst_video_format_get_component_offset (encoder->format,
        i, width, height);
    encoder->stride[i] = gst_video_format_get_row_stride (encoder->format,
        i, width);
  }

  if (!gst_h264_enc_init_encoder (encoder))
    return FALSE;

  if (!gst_h264_enc_set_src_caps (encoder, encoder->srcpad, caps)) {
    gst_h264_enc_close_encoder (encoder);
    return FALSE;
  }

  return TRUE;
}

static GstCaps *
gst_h264_enc_sink_get_caps (GstPad * pad)
{
  GstH264Enc *encoder;
  GstPad *peer;
  GstCaps *caps;

  /* If we already have caps return them */
  if (GST_PAD_CAPS (pad))
    return gst_caps_ref (GST_PAD_CAPS (pad));

  encoder = GST_H264_ENC (gst_pad_get_parent (pad));
  if (!encoder)
    return gst_caps_new_empty ();

  peer = gst_pad_get_peer (encoder->srcpad);
  if (peer) {
    const GstCaps *templcaps;
    GstCaps *peercaps;
    guint i, n;

    peercaps = gst_pad_get_caps (peer);

    /* Translate peercaps to YUV */
    peercaps = gst_caps_make_writable (peercaps);
    n = gst_caps_get_size (peercaps);
    for (i = 0; i < n; i++) {
      GstStructure *s = gst_caps_get_structure (peercaps, i);

      gst_structure_set_name (s, "video/x-raw-yuv");
      gst_structure_remove_field (s, "stream-format");
      gst_structure_remove_field (s, "alignment");
    }

    templcaps = gst_pad_get_pad_template_caps (pad);

    caps = gst_caps_intersect (peercaps, templcaps);
    gst_caps_unref (peercaps);
    gst_object_unref (peer);
    peer = NULL;
  } else {
    caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));
  }

  gst_object_unref (encoder);

  return caps;
}

static gboolean
gst_h264_enc_src_event (GstPad * pad, GstEvent * event)
{
  gboolean ret = TRUE;
  GstH264Enc *encoder;
  gboolean forward = TRUE;

  encoder = GST_H264_ENC (gst_pad_get_parent (pad));
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_UPSTREAM:{
      const GstStructure *s;
      s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "GstForceKeyUnit")) {
        /* Set I frame request */
        GST_OBJECT_LOCK (encoder);
        encoder->i_type = 2;
        encoder->forcekeyunit_event = gst_event_copy (event);
        GST_EVENT_TYPE (encoder->forcekeyunit_event) =
            GST_EVENT_CUSTOM_DOWNSTREAM;
        GST_OBJECT_UNLOCK (encoder);
        forward = FALSE;
        gst_event_unref (event);
      }
      break;
    }
    default:
      break;
  }

  if (forward)
    ret = gst_pad_push_event (encoder->sinkpad, event);

  gst_object_unref (encoder);
  return ret;
}

static gboolean
gst_h264_enc_sink_event (GstPad * pad, GstEvent * event)
{
  gboolean ret;
  GstH264Enc *encoder;

  encoder = GST_H264_ENC (gst_pad_get_parent (pad));
  
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_EOS:
      gst_h264_enc_flush_frames (encoder, TRUE);
      break;
    case GST_EVENT_TAG:{
      GstTagList *tags = NULL;

      event =
          GST_EVENT (gst_mini_object_make_writable (GST_MINI_OBJECT (event)));

      gst_event_parse_tag (event, &tags);
      /* drop codec/video-codec and replace encoder/encoder-version */
      gst_tag_list_remove_tag (tags, GST_TAG_VIDEO_CODEC);
      gst_tag_list_remove_tag (tags, GST_TAG_CODEC);
      gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "h264",
          GST_TAG_ENCODER_VERSION, "1.0", NULL);
      /* push is done below */
      break;
      /* no flushing if flush received,
       * buffers in encoder are considered (in the) past */
    }
    case GST_EVENT_CUSTOM_DOWNSTREAM:{
      const GstStructure *s;
      s = gst_event_get_structure (event);
      if (gst_structure_has_name (s, "GstForceKeyUnit")) {
        GST_OBJECT_LOCK (encoder);
         encoder->i_type = 2;
        GST_OBJECT_UNLOCK (encoder);
      }
      break;
    }
    default:
      break;
  }

  ret = gst_pad_push_event (encoder->srcpad, event);

  gst_object_unref (encoder);
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_h264_enc_chain (GstPad * pad, GstBuffer * buf)
{
  GstH264Enc *encoder = GST_H264_ENC (GST_OBJECT_PARENT (pad));
  GstFlowReturn ret;
  h264_picture_t pic_in;
 
  gint i_nal, i;
  
  Ipp32s mWidth = encoder->h264param->info.clip_info.width;
  Ipp32s mHeight = encoder->h264param->info.clip_info.height;
  UMC::VideoData *p_dataIn = &pic_in;
  UMC::VideoData *data; 
  UMC::VideoData::PlaneInfo plInfo;
  
  Ipp8u* ptr;
  int w, h, count;
  int numPlanes;
  // int alphaFlag = 0;
  int k, kk;
  FILE *f;
  Ipp32s mFrameSize;


  GST_LOG_OBJECT(encoder, "gst_h264_enc_chain()");
  if (G_UNLIKELY (encoder->h264enc == NULL))
    goto not_inited;



  p_dataIn->Init(mWidth, mHeight, 
  (encoder->h264param->chroma_format_idc == 0)? UMC::GRAY 
  : (encoder->h264param->chroma_format_idc == 2)? UMC::YUV422 
  : (encoder->h264param->chroma_format_idc == 3)? UMC::YUV444 :  UMC::YUV420,
  (IPP_MAX(encoder->h264param->bit_depth_luma, encoder->h264param->bit_depth_chroma) > 8)? 16 : 8);

//  if (mColorFormat == UMC::YUV420A || mColorFormat == UMC::YUV422A || mColorFormat == UMC::YUV444A) {
//   p_dataIn->SetPlaneBitDepth(mBitDepthAlpha, 3);
// }
   mFrameSize = (Ipp32s)p_dataIn->GetMappingSize();

  {
    p_dataIn->Alloc();
  }

  

  /* create h264_picture_t from the buffer */
  if (G_UNLIKELY (GST_BUFFER_SIZE (buf) < encoder->image_size))
    goto wrong_buffer_size;
	 
	 
  /* remember the timestamp and duration */
  g_queue_push_tail (encoder->delay, buf);
    
  
  data = DynamicCast<UMC::VideoData>(p_dataIn);
  numPlanes = data->GetNumPlanes();
  
    f=fmemopen(GST_BUFFER_DATA (buf), GST_BUFFER_SIZE(buf), "r");
    if (NULL == f)
     GST_ERROR_OBJECT(encoder, "fmemopen failed");
  
    
    for (k = 0; k < numPlanes; k++) {
    	kk = k;
    if (encoder->format ==  GST_VIDEO_FORMAT_YV12 && data->GetColorFormat() == UMC::YUV420) {
      if (k == 1) kk = 2;  else
      if (k == 2) kk = 1;
    }
    

    if (data->GetColorFormat() == UMC::YV12) {
      // for YV12 change planes to Y,U,V order
      if (kk == 1) kk = 2;
      if (kk == 2) kk = 1;
    }

    ptr = (Ipp8u*)data->GetPlanePointer(kk);
    data->GetPlaneInfo(&plInfo, kk);
    h = plInfo.m_ippSize.height;
    w = plInfo.m_ippSize.width;
    for (i = 0; i < h; i ++) {
     count = (Ipp32s)fread(ptr, plInfo.m_iSampleSize, w, f);
     ptr += plInfo.m_nPitch;
     if (count != w) {
      GST_ERROR_OBJECT(encoder, "fread %d != %d", count, w);
     }
    }
   }
   ret = gst_h264_enc_encode_frame (encoder, p_dataIn, &i_nal, TRUE);
  /* input buffer is released later on */
  return ret;

/* ERRORS */
not_inited:
  {
    GST_WARNING_OBJECT (encoder, "Got buffer before set_caps was called");
    gst_buffer_unref (buf);
    return GST_FLOW_NOT_NEGOTIATED;
  }
wrong_buffer_size:
  {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode h264 frame failed."),
        ("Wrong buffer size %d (should be %d)",
            GST_BUFFER_SIZE (buf), encoder->image_size));
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_h264_enc_encode_frame (GstH264Enc * encoder, h264_picture_t * pic_in,
    int *i_nal, gboolean send)
{
  GstBuffer *out_buf = NULL, *in_buf = NULL;
  UMC::Status    status = UMC::UMC_OK;
  UMC::VideoData *p_dataIn = pic_in;
  Ipp32s mWidth = encoder->h264param->info.clip_info.width;
  Ipp32s mHeight = encoder->h264param->info.clip_info.height;

  UMC::MediaData out(4*mWidth*mHeight);
 
  UMC::MediaData *p_dataOut = &out;

  h264_picture_t pic_out;

  GstFlowReturn ret;
  GstClockTime duration;
  GstEvent *forcekeyunit_event = NULL;
  GstClockTime  tstamp_out;
  static Ipp32s    mFramesEncoded = 0;
  
  

  if (G_UNLIKELY (encoder->h264enc == NULL))
    return GST_FLOW_NOT_NEGOTIATED;

  GST_OBJECT_LOCK (encoder);
  if (encoder->reconfig) {
    encoder->reconfig = FALSE;
	 if (encoder->h264enc->Reset() != UMC::UMC_OK) 
	 	GST_WARNING_OBJECT (encoder, "Could not reset encoder");
	 if (encoder->h264enc->SetParams(encoder->h264param) != UMC::UMC_OK)
	 	GST_WARNING_OBJECT (encoder, "Could not reconfigure encoder");
    
  }
   GST_OBJECT_UNLOCK (encoder);
	
	if (!p_dataIn) {
		*i_nal=0;
		mFramesEncoded++;
		return GST_FLOW_OK;
	}

	status = encoder->h264enc->GetFrame(p_dataIn, p_dataOut);
	if (status != UMC::UMC_OK && status != UMC::UMC_ERR_NOT_ENOUGH_DATA && status != UMC::UMC_ERR_END_OF_STREAM) {
      GST_ERROR_OBJECT(encoder, "Error: encoding failed at %d source frame (exit with %d)!", mFramesEncoded, status);
		return GST_FLOW_ERROR;
   } else {
		GST_LOG_OBJECT(encoder,"Encoded %d source frame", mFramesEncoded);
	}
	if (p_dataIn) {
      mFramesEncoded++;
   } 
 
  in_buf = (GstBuffer*)g_queue_pop_head (encoder->delay);
  if (in_buf) {
    duration = GST_BUFFER_DURATION (in_buf);
	 tstamp_out = GST_BUFFER_TIMESTAMP (in_buf);
    gst_buffer_unref (in_buf);
  } else {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, (NULL),
        ("Timestamp queue empty."));
    return GST_FLOW_ERROR;
  }
  
  if (!send)
    return GST_FLOW_OK;
	 
  ret = gst_pad_alloc_buffer (encoder->srcpad, GST_BUFFER_OFFSET_NONE,
      p_dataOut->GetDataSize(), GST_PAD_CAPS (encoder->srcpad), &out_buf);
  if (ret != GST_FLOW_OK)
    return ret;

  memcpy (GST_BUFFER_DATA (out_buf),p_dataOut->GetDataPointer() , p_dataOut->GetDataSize());
  GST_BUFFER_SIZE (out_buf) = p_dataOut->GetDataSize();
  
  GST_BUFFER_TIMESTAMP (out_buf) = tstamp_out;
  GST_BUFFER_DURATION (out_buf) = duration;
  switch (p_dataOut->GetFrameType()) {
   	case UMC::I_PICTURE:
			GST_BUFFER_FLAG_UNSET (out_buf, GST_BUFFER_FLAG_DELTA_UNIT);
			break;
		default :
			 GST_BUFFER_FLAG_SET (out_buf, GST_BUFFER_FLAG_DELTA_UNIT);
			 break;
  }
 
 
  GST_OBJECT_LOCK (encoder);
  forcekeyunit_event = encoder->forcekeyunit_event;
  encoder->forcekeyunit_event = NULL;
  GST_OBJECT_UNLOCK (encoder);
  if (forcekeyunit_event) {
    gst_structure_set (forcekeyunit_event->structure,
        "timestamp", G_TYPE_UINT64, GST_BUFFER_TIMESTAMP (out_buf), NULL);
    gst_pad_push_event (encoder->srcpad, forcekeyunit_event);
  }

  return gst_pad_push (encoder->srcpad, out_buf);

	
}



static void
gst_h264_enc_flush_frames (GstH264Enc * encoder, gboolean send)
{
  GstFlowReturn flow_ret;
  gint i_nal;
  GST_DEBUG_OBJECT (encoder,"gst_h264_enc_flush_frames");

  /* first send the remaining frames */
  if (encoder->h264enc)
    do {
      flow_ret = gst_h264_enc_encode_frame (encoder, NULL, &i_nal, send);
      /* note that this doesn't flush all frames for > 1 delayed frame */
    } while (flow_ret == GST_FLOW_OK && i_nal > 0);

  /* in any case, make sure the delay queue is emptied */
  while (!g_queue_is_empty (encoder->delay))
    gst_buffer_unref ((GstBuffer*)g_queue_pop_head (encoder->delay));
}

static GstStateChangeReturn
gst_h264_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstH264Enc *encoder = GST_H264_ENC (element);
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GST_DEBUG_OBJECT (encoder,"GstStateChangeReturn");
  ret = parent_class->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    goto out;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_h264_enc_flush_frames (encoder, FALSE);
      gst_h264_enc_close_encoder (encoder);
     //  gst_h264_enc_reset (encoder);
      break;
    default:
      break;
  }

out:
  return ret;
}



static void
gst_h264_enc_reconfig (GstH264Enc * encoder)
{
  if (encoder) GST_DEBUG_OBJECT (encoder,"gst_h264_enc_reconfig");
 
  encoder->h264param->info.clip_info.width = encoder->width;
  encoder->h264param->info.clip_info.height = encoder->height;
  encoder->h264param->info.framerate = encoder->fps_num;
  encoder->h264param->info.bitrate = (encoder->bitrate * 1024);
  encoder->h264param->numThreads = encoder->threads;
  encoder->h264param->B_frame_rate = encoder->bframes;
  encoder->h264param->num_ref_frames = encoder->ref;
  
  encoder->h264param->key_frame_controls.interval = encoder->keyint;
  encoder->h264param->key_frame_controls.method = 1;
  encoder->h264param->key_frame_controls.idr_interval = 1;
  
  encoder->h264param->profile_idc = (UMC::H264_PROFILE_IDC)100;
  encoder->h264param->chroma_format_idc = 1;
  encoder->h264param->bit_depth_luma = 8;
  encoder->h264param->bit_depth_chroma = 8;
  encoder->reconfig = TRUE;
}

static void
gst_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstH264Enc *encoder;
  GstState state;


  encoder = GST_H264_ENC (object);
  GST_DEBUG_OBJECT (encoder,"gst_h264_enc_set_property");

  GST_OBJECT_LOCK (encoder);
 
  state = GST_STATE (encoder);
  if ((state != GST_STATE_READY && state != GST_STATE_NULL) &&
      !(pspec->flags & GST_PARAM_MUTABLE_PLAYING))
    goto wrong_state;

  switch (prop_id) {
    case ARG_BITRATE:
      encoder->bitrate = g_value_get_uint (value);
      gst_h264_enc_reconfig (encoder);
      break;
    case ARG_THREADS:
      encoder->threads = g_value_get_uint (value);
      g_string_append_printf (encoder->option_string, ":threads=%d",
          encoder->threads);
      break;
    case ARG_BYTE_STREAM:
      encoder->byte_stream = g_value_get_boolean (value);
      g_string_append_printf (encoder->option_string, ":annexb=%d",
          encoder->byte_stream);
      break;
	 case ARG_REF:
      encoder->ref = g_value_get_uint (value);
      g_string_append_printf (encoder->option_string, ":ref=%d", encoder->ref);
      break;
    case ARG_BFRAMES:
      encoder->bframes = g_value_get_uint (value);
      g_string_append_printf (encoder->option_string, ":bframes=%d",
          encoder->bframes);
      break;
    case ARG_KEYINT:
      encoder->keyint = g_value_get_uint (value);
      g_string_append_printf (encoder->option_string, ":keyint=%d",
          encoder->keyint);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
  return;

  /* ERROR */
wrong_state:
  {
    GST_WARNING_OBJECT (encoder, "setting property in wrong state");
    GST_OBJECT_UNLOCK (encoder);
  }
}

static void
gst_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstH264Enc *encoder;

  encoder = GST_H264_ENC (object);
  GST_DEBUG_OBJECT (encoder,"gst_h264_enc_get_property");

  GST_OBJECT_LOCK (encoder);
  switch (prop_id) {
    case ARG_THREADS:
      g_value_set_uint (value, encoder->threads);
      break;
    case ARG_BYTE_STREAM:
      g_value_set_boolean (value, encoder->byte_stream);
      break;
    case ARG_BITRATE:
      g_value_set_uint (value, encoder->bitrate);
      break;    
    case ARG_REF:
      g_value_set_uint (value, encoder->ref);
      break;
    case ARG_BFRAMES:
      g_value_set_uint (value, encoder->bframes);
      break;
    case ARG_KEYINT:
      g_value_set_uint (value, encoder->keyint);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
}

gboolean plugin_init_ipp_h264(GstPlugin *plugin) {
         GST_DEBUG_CATEGORY_INIT (h264_enc_debug, "h264enc", 0,
      "h264 encoding element");

        return gst_element_register(plugin, "h264enc", GST_RANK_NONE, GST_TYPE_H264_ENC);
}



#if 0
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (h264_enc_debug, "h264enc", 0,
      "h264 encoding element");

  return gst_element_register (plugin, "h264enc",
      GST_RANK_PRIMARY, GST_TYPE_H264_ENC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "h264",
    "Intel IPP based H264 encoder plugin",
    plugin_init, "1.0", GST_LICENSE_UNKNOWN , PACKAGE, GST_PACKAGE_ORIGIN)

#endif
