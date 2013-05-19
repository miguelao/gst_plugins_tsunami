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
 *
 * example: gst-launch filesrc location="xx" ! decodebin2 ! alpha ! ghostmapper blabla ! ximagesink
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstghostmapper.h"
#include <gst/controller/gstcontroller.h>

#include <png.h>


GST_DEBUG_CATEGORY_STATIC (gst_ghostmapper_debug);
#define GST_CAT_DEFAULT gst_ghostmapper_debug

#define DEFAULT_GHOSTFILENAME  "file:///apps/devnfs/mcasassa/mask_320x240.png"

enum {
	PROP_0,
	PROP_PASSTHROUGH,
	PROP_FPS,
	PROP_STATS,
	PROP_APPLY_CUTOUT,
	PROP_GREEN,
	PROP_DEBUG_VIEW,
	PROP_GHOST,
	PROP_LAST
};

static GstStaticPadTemplate gst_ghostmapper_src_template =
  GST_STATIC_PAD_TEMPLATE ("src",
                           GST_PAD_SRC,
                           GST_PAD_ALWAYS,
                           GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("AYUV"))
  );
static GstStaticPadTemplate gst_ghostmapper_sink_template =
  GST_STATIC_PAD_TEMPLATE ("sink",
                           GST_PAD_SINK,
                           GST_PAD_ALWAYS,
                           GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV ("AYUV")) 
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
static GstFlowReturn gst_ghostmapper_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_ghostmapper_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_ghostmapper_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_ghostmapper_finalize(GObject * object);

static gboolean gst_ghostmapper_sink_event(GstPad *pad, GstEvent * event);



gboolean read_png(curlMemoryStruct *chunk, unsigned char **raw_image, picSrcImageInfo *info, char *errBuf);
#include <curl/curl.h>
gboolean curl_download(const char *uri, const char *accessToken, curlMemoryStruct *chunk, char *curlErrBuf);
size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);










GST_BOILERPLATE (GstGhostmapper, gst_ghostmapper, GstVideoFilter, GST_TYPE_VIDEO_FILTER);


static void gst_ghostmapper_base_init(gpointer g_class) {
	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_set_details_simple(element_class, "Ghostmapper filter", "Filter/Effect/Video",
			"Maps+scales a ghost to the face bbox, adding to an existing an alpha channel",
			"Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>" );

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_ghostmapper_sink_template));
	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_ghostmapper_src_template));

	GST_DEBUG_CATEGORY_INIT (gst_ghostmapper_debug, "ghostmapper", 0, "ghostmapper - Element for craeting a ghost-alpha channel to streams");
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
	g_object_class_install_property (gobject_class, PROP_GREEN,
                                         g_param_spec_boolean (
                                          "green", "green",
                                          "Substitute alpha channel with a green colour, to be encoded later on",
                                          FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
	g_object_class_install_property(gobject_class, PROP_DEBUG_VIEW,
                                        g_param_spec_boolean (
                                         "debugview", "DebugView",
                                         "Activate passthrough option: no cutout performed, alpha channel to 1",
                                         FALSE, (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS) ) );
	g_object_class_install_property(gobject_class, PROP_GHOST,
                                        g_param_spec_string(
                                         "ghost", "ghost", "Ghost file name (png!)",
                                         DEFAULT_GHOSTFILENAME,	
                                         (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

	btrans_class->passthrough_on_same_caps = TRUE;
	btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_ghostmapper_transform_ip);

	btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_ghostmapper_get_unit_size);
	btrans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_ghostmapper_transform_caps);
	btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_ghostmapper_set_caps);
}

static void gst_ghostmapper_init(GstGhostmapper * ghostmapper, GstGhostmapperClass * klass) 
{
  //gst_base_transform_set_in_place((GstBaseTransform *)ghostmapper, TRUE);
  g_static_mutex_init(&ghostmapper->lock);
  ghostmapper->passthrough = false;
  ghostmapper->apply_cutout = false;
  ghostmapper->green        = true;
  ghostmapper->fps = false;
  ghostmapper->debug_view = 0;
  ghostmapper->id = (guint)-1;
  ghostmapper->findex = 0;
  ghostmapper->raw_image = NULL;

  ghostmapper->ghostfilename = g_strdup(DEFAULT_GHOSTFILENAME);
}
static void gst_ghostmapper_finalize(GObject * object) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (object);
  
  GST_GHOSTMAPPER_LOCK (ghostmapper);

  if (ghostmapper->cvImgIn)           cvReleaseImageHeader(&ghostmapper->cvImgIn);
  if (ghostmapper->cvGhost)           cvReleaseImageHeader(&ghostmapper->cvGhost);
  if (ghostmapper->cvGhostBw)         cvReleaseImage(&ghostmapper->cvGhostBw);
  if (ghostmapper->cvGhostBwResized)  cvReleaseImage(&ghostmapper->cvGhostBwResized);

  g_free(ghostmapper->ghostfilename);

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
  case PROP_GREEN:
    ghostmapper->green = g_value_get_boolean(value);
    break;
  case PROP_DEBUG_VIEW:
    ghostmapper->debug_view = g_value_get_boolean(value);
  case PROP_GHOST:
    g_free(ghostmapper->ghostfilename);
    ghostmapper->ghostfilename = g_value_dup_string(value);
    break;
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
  case PROP_GREEN:
    g_value_set_boolean(value, ghostmapper->green);
    break;
  case PROP_DEBUG_VIEW:
    g_value_set_boolean(value, ghostmapper->debug_view);
    break;
  case PROP_GHOST:
    g_value_set_string(value, ghostmapper->ghostfilename);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_ghostmapper_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) 
{
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
  GST_LOG("Transforming caps ");
    
  GST_GHOSTMAPPER_LOCK (ghostmapper);
  
  for (i = 0; i < (int)gst_caps_get_size(caps); i++) {
    structure = gst_structure_copy(gst_caps_get_structure(caps, i));

    //gst_structure_remove_fields(structure, 
    //  "format", "endianness", "depth", "bpp", "red_mask", "green_mask", "blue_mask", "alpha_mask",
    //  "palette_data", "color-matrix", "chroma-site", NULL);
    //gst_structure_set_name(structure, "video/x-raw-yuv");
    //gst_structure_set_name(structure, "video/x-raw-yuv");
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
  
  GST_LOG("Transformed %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, caps, ret);  
  GST_GHOSTMAPPER_UNLOCK (ghostmapper);
  
  return ret;
}


static gboolean gst_ghostmapper_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_GHOSTMAPPER_LOCK (ghostmapper);
  
  GST_DEBUG("Parsing input/output caps...");
  gst_video_format_parse_caps(incaps, &ghostmapper->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &ghostmapper->out_format, &out_width, &out_height);

  if ( !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_GHOSTMAPPER_UNLOCK (ghostmapper);
    return FALSE;
  }
  
  ghostmapper->width = in_width;
  ghostmapper->height = in_height;

  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(ghostmapper), gst_ghostmapper_sink_event); 
  GST_GHOSTMAPPER_UNLOCK (ghostmapper);
  
  // Init face pos /////////////////////////////////////////////////////////////
  GST_DEBUG("Initialising Ghostmapper...");
  ghostmapper->x = 0.5 * ghostmapper->width;
  ghostmapper->y = 0.4 * ghostmapper->height;
  ghostmapper->w = ghostmapper->h = 0.2 * ghostmapper->width;
  ghostmapper->isface = false;

  // Init ghost file ///////////////////////////////////////////////////////////
  curlMemoryStruct  chunk;
  //gchar url[]="file:///home/mcasassa/imco2/mods/gstreamer/cyclops/shaders/mask8.png";
  //gchar url[]="file:///apps/devnfs/mcasassa/mask_320x240.png";
  char curlErrBuf[255];

  if(FALSE == curl_download(ghostmapper->ghostfilename, "", &chunk, curlErrBuf)) {
    GST_ERROR("download failed, err: %s", curlErrBuf);
  }

  char errBuf[255];
  if( FALSE == read_png(&chunk, &(ghostmapper->raw_image), &(ghostmapper->info), errBuf)){
    GST_ERROR("png load failed, err: %s", errBuf);
  }

  // Init openCV structs ///////////////////////////////////////////////////////
  const CvSize sizein = cvSize(ghostmapper->width, ghostmapper->height);
  ghostmapper->cvImgIn = cvCreateImageHeader(sizein, IPL_DEPTH_8U, 4);

  const CvSize sizegh = cvSize(ghostmapper->info.width, ghostmapper->info.height);
  ghostmapper->cvGhost = cvCreateImageHeader(sizegh, IPL_DEPTH_8U, ghostmapper->info.channels);
  ghostmapper->cvGhost->imageData = (char*)ghostmapper->raw_image;

  ghostmapper->cvGhostBw = cvCreateImage(sizegh, IPL_DEPTH_8U, 1);
  if( ghostmapper->info.channels > 1){
    cvCvtColor( ghostmapper->cvGhost, ghostmapper->cvGhostBw, CV_RGB2GRAY );
  }
  else{
    cvCopy(ghostmapper->cvGhost, ghostmapper->cvGhostBw, NULL);
  }

  ghostmapper->cvGhostBwResized = cvCreateImage(sizein, IPL_DEPTH_8U, 1);
  cvResize( ghostmapper->cvGhostBw, ghostmapper->cvGhostBwResized, CV_INTER_LINEAR);

  ghostmapper->cvGhostBwAffined = cvCreateImage(sizein, IPL_DEPTH_8U, 1);

  GST_INFO(" Collected caps, image in size (%dx%d), ghost size (%dx%d) %dch",ghostmapper->width, ghostmapper->height,
            ghostmapper->info.width, ghostmapper->info.height, ghostmapper->info.channels );

  // 3 points of the face bbox associated to the ghost.
  ghostmapper->srcTri[0].x = 145;
  ghostmapper->srcTri[0].y = 74;
  ghostmapper->srcTri[1].x = 145;
  ghostmapper->srcTri[1].y = 74+39;
  ghostmapper->srcTri[2].x = 145+34;
  ghostmapper->srcTri[2].y = 74+39;

  ghostmapper->warp_mat = cvCreateMat(2,3,CV_32FC1);

  // done //////////////////////////////////////////////////////////////////////
  GST_INFO("Ghostmapper initialized.");

  return TRUE;
}

static GstFlowReturn gst_ghostmapper_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) {

  GstGhostmapper *ghostmapper = GST_GHOSTMAPPER (btrans);
  
  ghostmapper->findex++;
  //////////////////////////////////////////////////////PASSTHROUGH/////////////
  if (ghostmapper->passthrough)
    return GST_FLOW_OK;

  GST_GHOSTMAPPER_LOCK (ghostmapper);

  //////////////////////////////////////////////////////get face bbox///////////
  GST_DEBUG("Position of the face is %f,%f, w %f, h %f", 
            ghostmapper->x, ghostmapper->y, ghostmapper->w, ghostmapper->h);
  ghostmapper->dstTri[0].x = ghostmapper->x - ghostmapper->w/2 ;
  ghostmapper->dstTri[0].y = ghostmapper->y - ghostmapper->h/2;
  ghostmapper->dstTri[1].x = ghostmapper->x - ghostmapper->w/2;
  ghostmapper->dstTri[1].y = ghostmapper->y + ghostmapper->h/2;
  ghostmapper->dstTri[2].x = ghostmapper->x + ghostmapper->w/2;
  ghostmapper->dstTri[2].y = ghostmapper->y + ghostmapper->h/2;

  cvGetAffineTransform( ghostmapper->srcTri, ghostmapper->dstTri, ghostmapper->warp_mat );
  cvWarpAffine( ghostmapper->cvGhostBwResized, ghostmapper->cvGhostBwAffined, ghostmapper->warp_mat );

  //////////////////////////////////////////////////////BUSINESS////////////////
  ghostmapper->cvImgIn->imageData = (char*)GST_BUFFER_DATA(gstbuf);

  //ghostmapper->cvGhostBwResized has the resized bw alpha channel, just copy overwrite the channel 1 in input

  cvSetImageCOI(ghostmapper->cvImgIn, 1);
  cvCopy( ghostmapper->cvGhostBwAffined, ghostmapper->cvImgIn, NULL); //last null is the mask
  cvResetImageROI(ghostmapper->cvImgIn);

  /////////// make background green /////////////////////
  if(ghostmapper->green){
    uint8_t* data = (uint8_t*)ghostmapper->cvImgIn->imageData; //cutoutglsl->imgDebug->data[0];
    for (gint i = 0; i < ghostmapper->cvImgIn->height; i++) {
      for (gint j = 0; j < ghostmapper->cvImgIn->width; j++) {
        if(data[0] <40){  // in case of transparent, set to bright green (in YUV)
          data[1] = 149;
          data[2] = 43;
          data[3] = 21;
        }
        data += 4;
      }
    }
  }
  ///////////////////////////////////////////////////////

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
      //GST_INFO("Received custom event with face detection, (%.2f,%.2f)", ghostmapper->x, ghostmapper->y);
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









static void png_warning_fn(png_structp png_ptr, png_const_charp warning_msg) {
//		  gst_debug_log(gst_ghostmapper_debug, GST_LEVEL_LOG, __FILE__, __FUNCTION__ , __LINE__, NULL, "libpng warning: %s",  warning_msg);
		  
}
static void png_error_fn(png_structp png_ptr,  png_const_charp error_msg) {
//		  gst_debug_log(gst_picsrcjpeg_debug, GST_LEVEL_WARNING, __FILE__, __FUNCTION__ , __LINE__, NULL, "libpng error: %s",  error_msg);
}

gboolean read_png(curlMemoryStruct *chunk, unsigned char **raw_image, picSrcImageInfo *info, char *errBuf) 
{
		
  png_structp png_ptr;
  png_infop info_ptr;
  /* initialize stuff */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr) {
    GST_DEBUG("png_create_read_struct failed");
    strcpy(errBuf, "read_png - Error : png_create_read_struct failed");
    return FALSE;
  }
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    GST_DEBUG("png_create_info_struct failed"); 
    strcpy(errBuf, "read_png - Error : png_create_info_struct failed");
    return FALSE; 
  }
  FILE* memstream = fmemopen((void *)chunk->memory, chunk->size, "rb");
  if (!memstream) {
    GST_DEBUG("fmemopen failed"); 
    strcpy(errBuf, "read_imgheader - Error : fmemopen failed");
    return FALSE; 
  }		  
  png_init_io(png_ptr, memstream);
		  
  png_set_error_fn(png_ptr, (png_voidp)NULL,  png_error_fn, png_warning_fn);

		 
  /* read file */
  if (setjmp(png_jmpbuf(png_ptr))) {
    sprintf(errBuf, "read_png - Error : %s", "undetermined"); 
    fclose(memstream);
    return FALSE;
  }
		  
       
  png_uint_32 imgWidth, imgHeight;
  int bitdepth, color_type;
		 
  png_read_info(png_ptr, info_ptr);
  png_get_IHDR( png_ptr, info_ptr, &imgWidth, &imgHeight,
                &bitdepth, &color_type, 0, 0, 0 );		  
  //Number of channels
  int channels   = png_get_channels(png_ptr, info_ptr);
		 
  switch (color_type) {
  case PNG_COLOR_TYPE_PALETTE:
    png_set_palette_to_rgb(png_ptr);
    channels = 3;
    info->colorspace =  COLOR_TYPE_RGB;          
    break;
  case PNG_COLOR_TYPE_GRAY:
    if (bitdepth < 8)
      png_set_gray_1_2_4_to_8(png_ptr);
    //And the bitdepth info
    bitdepth = 8;
    png_set_gray_to_rgb(png_ptr);
    info->colorspace =  COLOR_TYPE_RGB;       
    break;
  }
  /*if the image has a transperancy set.. convert it to a full Alpha channel..*/
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_ptr);
    channels+=1;
    info->colorspace =  COLOR_TYPE_RGB_ALPHA;   
  }
  //We don't support 16 bit precision.. so if the image Has 16 bits per channel
  //precision... round it down to 8.
  if (bitdepth == 16)
    png_set_strip_16(png_ptr);
		  
  info->width	 = imgWidth;
  info->height   = imgHeight;
  info->channels = channels;
		   	
		  
  /* read file */
  if (setjmp(png_jmpbuf(png_ptr))) {
    sprintf(errBuf, "read_png - Error : %s", "undetermined");
    fclose(memstream);
    return FALSE;
  }
		  
		   
  png_bytep* rowPtrs[imgHeight];
         
			
         
			
  *raw_image = (unsigned char*)malloc(imgWidth * imgHeight * bitdepth * channels / 8);
  const unsigned int stride = imgWidth * bitdepth * channels / 8;
  GST_DEBUG("imgWidth:%d, imgHeight:%d, bitdepth:%d, channels:%d, stride:%d", imgWidth,imgHeight,bitdepth,channels, stride  );
  size_t i;
  for (i = 0; i < imgHeight; i++) {         	
    rowPtrs[i] = (png_bytep*)((png_bytep)(*raw_image) + ( i  * stride));

  }
  png_read_image(png_ptr, (png_bytepp)rowPtrs);
  fclose(memstream);
  return TRUE;
}




gboolean curl_download(const char *uri, const char *accessToken, curlMemoryStruct *chunk, char *curlErrBuf) {
  CURL *curl_handle;
  struct curl_slist *headers=NULL;
#define MAX_CUSTHEADER_SIZE 1038 /* max token 1024 bytes + 14 bytes for the header info */
  char accTokHeader[MAX_CUSTHEADER_SIZE];
  
  
  if (accessToken && strlen(accessToken) > (MAX_CUSTHEADER_SIZE-14)) {
    snprintf(curlErrBuf, CURL_ERROR_SIZE, "accessToken length exceeds MAX_CUSTHEADER_SIZE");
    return FALSE;
  }
  memset(accTokHeader, 0x0, MAX_CUSTHEADER_SIZE);
  
  chunk->memory = (unsigned char*)malloc(1);  /* will be grown as needed by the realloc above */ 
  chunk->size = 0;            /* no data at this point */ 
 
  /**
   * \todo This function should only be called once per program
   */
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* init the curl session */ 
  curl_handle = curl_easy_init();
 
  /* specify URL to get */ 
  curl_easy_setopt(curl_handle, CURLOPT_URL, uri);
  
  /* make it fail on http error */
  curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, TRUE);

  /* follow redirections */
  curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, TRUE);
  
  curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER , curlErrBuf);

  /* send all data to this function  */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
  /* we pass our 'chunk' struct to the callback function */ 
  curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)chunk);
 
  /* some servers don't like requests that are made without a user-agent
     field, so we provide one */ 
  curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
 
  /* pass our list of custom made headers */
  if (accessToken) {
    snprintf(accTokHeader, MAX_CUSTHEADER_SIZE - 1, "AccessToken: %s", accessToken);
    headers = curl_slist_append(headers, accTokHeader);
  
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
  }
  
  /* get it! */ 
  CURLcode err = curl_easy_perform(curl_handle);
	
  if (headers)
    curl_slist_free_all(headers);
  
  if (0 != err) {
    /* oops */
    if(chunk->memory){
      free(chunk->memory);
      chunk->memory=NULL;
    }
    chunk->size = 0;
    long httperr;
    if (CURLE_OK ==  curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httperr))
      GST_DEBUG("httpcode : %ld \n", httperr);
				
    return FALSE;
  } 
 
  /* cleanup curl stuff */ 
  curl_easy_cleanup(curl_handle);
 
  /*
   * Now, our chunk.memory points to a memory block that is chunk.size
   * bytes big and contains the remote file.
   *
   * Do something nice with it!
   *
   * You should be aware of the fact that at this point we might have an
   * allocated data block, and nothing has yet deallocated that data. So when
   * you're done with it, you should free() it as a nice application.
   */ 
 
  
	
	
  /* we're done with libcurl, so clean it up */
  /**
   * \todo This function should only be called once per program
   */
  curl_global_cleanup();
  
  return TRUE; 

}


size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
  size_t realsize = size * nmemb;
  struct curlMemoryStruct *mem = (struct curlMemoryStruct *)data;
 
  mem->memory = (unsigned char*)realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory == NULL) {
    /* out of memory! */ 
    GST_ERROR("not enough memory (realloc returned NULL)");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), ptr, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
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
