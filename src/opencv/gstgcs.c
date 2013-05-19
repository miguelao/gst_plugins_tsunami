/* GStreamer
 */
/**
 * SECTION:element- gcs
 *
 * This element is a segmenter FG/BG of an image based on a graphcut expansion of face-position ghost
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstgcs.h"

#include <png.h>


GST_DEBUG_CATEGORY_STATIC (gst_gcs_debug);
#define GST_CAT_DEFAULT gst_gcs_debug

#define DEFAULT_GHOSTFILENAME  "file:///apps/devnfs/mcasassa/mask_320x240.png"

enum {
	PROP_0,
        PROP_DISPLAY,
        PROP_DEBUG,
	PROP_GHOST,
	PROP_LAST
};

static GstStaticPadTemplate gst_gcs_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_gcs_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_GCS_LOCK(gcs) G_STMT_START { \
	GST_LOG_OBJECT (gcs, "Locking gcs from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&gcs->lock); \
	GST_LOG_OBJECT (gcs, "Locked gcs from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_GCS_UNLOCK(gcs) G_STMT_START { \
	GST_LOG_OBJECT (gcs, "Unlocking gcs from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&gcs->lock); \
} G_STMT_END

static gboolean gst_gcs_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_gcs_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_gcs_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_gcs_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_gcs_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_gcs_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_gcs_finalize(GObject * object);
static gboolean gst_gcs_sink_event(GstPad *pad, GstEvent * event);

static gboolean get_frame_difference( IplImage* in, IplImage* inprev, IplImage* output);

static void compose_grabcut_seedmatrix2(CvMat* output, CvRect facebox,  IplImage* seeds, gboolean confidence);
static void compose_grabcut_seedmatrix3(CvMat* output, IplImage* ghost,  IplImage* seeds );
static gint compose_skin_matrix(IplImage* rgbin, IplImage* gray_out);

#ifdef KMEANS
static void create_kmeans_clusters(IplImage* in, CvMat* points, CvMat* cluster, int numclusters, int numsamples);
static void adjust_bodybbox_w_clusters(CvMat* inout, IplImage* cluster, int numclusters, CvRect facebox);
static void posterize_image(IplImage* input);
#endif //KMEANS


gboolean read_png(curlMemoryStructGCS *chunk, unsigned char **raw_image, picSrcImageInfoGCS *info, char *errBuf);
#include <curl/curl.h>
gboolean curl_download(const char *uri, const char *accessToken, curlMemoryStructGCS *chunk, char *curlErrBuf);
static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);




GST_BOILERPLATE (GstGcs, gst_gcs, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanGcs(GstGcs *gcs) 
{
  if (gcs->pImageRGBA)  cvReleaseImageHeader(&gcs->pImageRGBA);

  finalise_grabcut( &gcs->GC );
}

static void gst_gcs_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Image Gcs filter - Grabcut Segmentation", "Filter/Effect/Video",
    "Creates a FG/BG mask based on Grabcut algorithm",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_gcs_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_gcs_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_gcs_debug, "gcs", 0, \
                           "gcs - Creates a FG/BG mask based on Grabcut algorithm");
}

static void gst_gcs_class_init(GstGcsClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_gcs_set_property;
  gobject_class->get_property = gst_gcs_get_property;
  gobject_class->finalize = gst_gcs_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_gcs_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_gcs_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_gcs_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_gcs_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output would be the actual bg/fg model", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, 
                                  PROP_DEBUG, g_param_spec_int(
                                  "debug", "debug",
                                  "output some debug intermediate representation", 0, 100, 0,
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, PROP_GHOST,
                                  g_param_spec_string(
                                    "ghost", "ghost", "Ghost file name (png!)",
                                    DEFAULT_GHOSTFILENAME,	
                                    (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_gcs_init(GstGcs * gcs, GstGcsClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)gcs, TRUE);
  g_static_mutex_init(&gcs->lock);

  gcs->pImageRGBA        = NULL;

  gcs->pImgRGB           = NULL;

  gcs->pImgGRAY          = NULL;
  gcs->pImgGRAY_copy     = NULL;
  gcs->pImgGRAY_diff     = NULL;
  gcs->pImgGRAY_1        = NULL;
  gcs->pImgGRAY_1copy    = NULL;

  gcs->pImgChA           = NULL;
  gcs->pImgCh1           = NULL;
  gcs->pImgCh2           = NULL;
  gcs->pImgCh3           = NULL;

  gcs->ghostfilename = NULL;
  gcs->display       = false;
  gcs->debug         = 0;
}

static void gst_gcs_finalize(GObject * object) 
{
  GstGcs *gcs = GST_GCS (object);
  
  GST_GCS_LOCK (gcs);
  CleanGcs(gcs);
  GST_GCS_UNLOCK (gcs);
  GST_INFO("Gcs destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&gcs->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_gcs_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstGcs *gcs = GST_GCS (object);
  
  GST_GCS_LOCK (gcs);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    gcs->display = g_value_get_boolean(value);
    break;    
  case PROP_DEBUG:
    gcs->debug = g_value_get_int(value);
    break;    
  case PROP_GHOST:
    g_free(gcs->ghostfilename);
    gcs->ghostfilename = g_value_dup_string(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_GCS_UNLOCK (gcs);
}

static void gst_gcs_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstGcs *gcs = GST_GCS (object);

  switch (prop_id) {
  case PROP_DISPLAY:
    g_value_set_boolean(value, gcs->display);
    break; 
  case PROP_DEBUG:
    g_value_set_int(value, gcs->debug);
    break; 
  case PROP_GHOST:
    g_value_set_string(value, gcs->ghostfilename);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_gcs_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) 
{
  GstVideoFormat format;
  gint width, height;
  
  if (!gst_video_format_parse_caps(caps, &format, &width, &height))
    return FALSE;
  
  *size = gst_video_format_get_size(format, width, height);
  
  GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);
  
  return TRUE;
}


static gboolean gst_gcs_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstGcs *gcs = GST_GCS (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_GCS_LOCK (gcs);
  
  gst_video_format_parse_caps(incaps, &gcs->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &gcs->out_format, &out_width, &out_height);
  if (!(gcs->in_format == gcs->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_GCS_UNLOCK (gcs);
    return FALSE;
  }
  
  gcs->width  = in_width;
  gcs->height = in_height;
  
  GST_INFO("Initialising Gcs...");
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(gcs),  gst_gcs_sink_event);

  const CvSize size = cvSize(gcs->width, gcs->height);
  GST_WARNING (" width %d, height %d", gcs->width, gcs->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  gcs->pImageRGBA    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  gcs->pImgRGB       = cvCreateImage(size, IPL_DEPTH_8U, 3);
  gcs->pImgScratch   = cvCreateImage(size, IPL_DEPTH_8U, 3);

  gcs->pImgGRAY      = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgGRAY_copy = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgGRAY_diff = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgGRAY_1    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgGRAY_1copy= cvCreateImage(size, IPL_DEPTH_8U, 1);
  cvZero( gcs->pImgGRAY_1 );
  cvZero( gcs->pImgGRAY_1copy );

  gcs->pImgChA       = cvCreateImageHeader(size, IPL_DEPTH_8U, 1);
  gcs->pImgCh1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgCh2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgCh3       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  gcs->pImgChX       = cvCreateImage(size, IPL_DEPTH_8U, 1);

  gcs->pImg_skin     = cvCreateImage(size, IPL_DEPTH_8U, 1);

  gcs->grabcut_mask   = cvCreateMat( size.height, size.width, CV_8UC1);
  cvZero(gcs->grabcut_mask);
  initialise_grabcut( &(gcs->GC), gcs->pImgRGB, gcs->grabcut_mask );
  gcs->bbox_prev      = cvRect( 60,70, 210, 170 );

  //////////////////////////////////////////////////////////////////////////////
#ifdef KMEANS
  // k-means allocation ////////////////////////////////////////////////////////
  gcs->pImgRGB_kmeans  = cvCreateImage(size, IPL_DEPTH_8U, 3);
  gcs->num_samples     = size.height * size.width;
  gcs->kmeans_points   = cvCreateMat( gcs->num_samples, 5, CV_32FC1);
  gcs->kmeans_clusters = cvCreateMat( gcs->num_samples, 1, CV_32SC1);
#endif //KMEANS

  //////////////////////////////////////////////////////////////////////////////
  // Init ghost file ///////////////////////////////////////////////////////////
  curlMemoryStructGCS  chunk;
  //gchar url[]="file:///home/mcasassa/imco2/mods/gstreamer/cyclops/shaders/mask8.png";
  //gchar url[]="file:///apps/devnfs/mcasassa/mask_320x240.png";
  char curlErrBuf[255];
  
  if( gcs->ghostfilename){
    if(FALSE == curl_download(gcs->ghostfilename, "", &chunk, curlErrBuf)) {
      GST_ERROR("download failed, err: %s", curlErrBuf);
    }
    
    char errBuf[255];
    if( FALSE == read_png(&chunk, &(gcs->raw_image), &(gcs->info), errBuf)){
      GST_ERROR("png load failed, err: %s", errBuf);
    }

    const CvSize sizegh = cvSize(gcs->info.width, gcs->info.height);
    gcs->cvGhost = cvCreateImageHeader(sizegh, IPL_DEPTH_8U, gcs->info.channels);
    gcs->cvGhost->imageData = (char*)gcs->raw_image;

    gcs->cvGhostBw = cvCreateImage(sizegh, IPL_DEPTH_8U, 1);
    if( gcs->info.channels > 1){
      cvCvtColor( gcs->cvGhost, gcs->cvGhostBw, CV_RGB2GRAY );
    }
    else{
      cvCopy(gcs->cvGhost, gcs->cvGhostBw, NULL);
    }

    gcs->cvGhostBwResized = cvCreateImage(size, IPL_DEPTH_8U, 1);
    cvResize( gcs->cvGhostBw, gcs->cvGhostBwResized, CV_INTER_LINEAR);

    gcs->cvGhostBwAffined = cvCreateImage(size, IPL_DEPTH_8U, 1);
  }

  GST_INFO(" Collected caps, image in size (%dx%d), ghost size (%dx%d) %dch",gcs->width, gcs->height,
            gcs->info.width, gcs->info.height, gcs->info.channels );

  // 3 points of the face bbox associated to the ghost.
  gcs->srcTri[0].x = 145;
  gcs->srcTri[0].y = 74;
  gcs->srcTri[1].x = 145;
  gcs->srcTri[1].y = 74+39;
  gcs->srcTri[2].x = 145+34;
  gcs->srcTri[2].y = 74+39;

  gcs->warp_mat = cvCreateMat(2,3,CV_32FC1);


  gcs->numframes = 0;

  GST_INFO("Gcs initialized.");
  
  GST_GCS_UNLOCK (gcs);
  
  return TRUE;
}

static void gst_gcs_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_gcs_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstGcs *gcs = GST_GCS (btrans);

  GST_GCS_LOCK (gcs);

  //////////////////////////////////////////////////////////////////////////////
  // get image data from the input, which is RGBA or BGRA
  gcs->pImageRGBA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvSplit(gcs->pImageRGBA,   gcs->pImgCh1, gcs->pImgCh2, gcs->pImgCh3, gcs->pImgChX );
  cvCvtColor(gcs->pImageRGBA,  gcs->pImgRGB, CV_BGRA2BGR);


  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////MOTION CUES INTEGR////
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  // apply step 1. filtering using bilateral filter. Cannot happen in-place => scratch
  cvSmooth(gcs->pImgRGB, gcs->pImgScratch, CV_BILATERAL, 3, 50, 3, 0);
  // create GRAY image
  cvCvtColor(gcs->pImgScratch, gcs->pImgGRAY, CV_BGR2GRAY);

  // Frame difference the GRAY and the previous one
  // not intuitive: first smooth frames, then 
  cvCopy( gcs->pImgGRAY,   gcs->pImgGRAY_copy,  NULL);
  cvCopy( gcs->pImgGRAY_1, gcs->pImgGRAY_1copy, NULL);
  get_frame_difference( gcs->pImgGRAY_copy, gcs->pImgGRAY_1copy, gcs->pImgGRAY_diff);
  cvErode( gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, NULL, 3);
  cvDilate( gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, NULL, 3);


  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  // ghost mapping
  gcs->dstTri[0].x = gcs->facepos.x - gcs->facepos.width/2 ;
  gcs->dstTri[0].y = gcs->facepos.y - gcs->facepos.height/2;
  gcs->dstTri[1].x = gcs->facepos.x - gcs->facepos.width/2;
  gcs->dstTri[1].y = gcs->facepos.y + gcs->facepos.height/2;
  gcs->dstTri[2].x = gcs->facepos.x + gcs->facepos.width/2;
  gcs->dstTri[2].y = gcs->facepos.y + gcs->facepos.height/2;

  if( gcs->ghostfilename){
    cvGetAffineTransform( gcs->srcTri, gcs->dstTri, gcs->warp_mat );
    cvWarpAffine( gcs->cvGhostBwResized, gcs->cvGhostBwAffined, gcs->warp_mat );
  }




  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  // GrabCut algorithm preparation and running

  gcs->facepos.x = gcs->facepos.x - gcs->facepos.width/2;
  gcs->facepos.y = gcs->facepos.y - gcs->facepos.height/2;

  // create an IplImage  with the skin colour pixels as 255
  compose_skin_matrix(gcs->pImgRGB, gcs->pImg_skin);
  // And the skin pixels with the movement mask
  cvAnd( gcs->pImg_skin,  gcs->pImgGRAY_diff,  gcs->pImgGRAY_diff);
  //cvErode( gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, cvCreateStructuringElementEx(5, 5, 3, 3, CV_SHAPE_RECT,NULL), 1);
  cvDilate(gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, cvCreateStructuringElementEx(7,7, 5,5, CV_SHAPE_RECT,NULL), 2);
  cvErode( gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, cvCreateStructuringElementEx(5,5, 3,3, CV_SHAPE_RECT,NULL), 2);

  // if there is alpha==all 1's coming in, then we ignore it: prevents from no vibe before us
  if((0.75*(gcs->width * gcs->height) <= cvCountNonZero(gcs->pImgChX)))
    cvZero(gcs->pImgChX);
  // OR the input Alpha
  cvOr( gcs->pImgChX,  gcs->pImgGRAY_diff,  gcs->pImgGRAY_diff);


  //////////////////////////////////////////////////////////////////////////////
  // try to consolidate a single mask from all the sub-patches
  cvDilate(gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, cvCreateStructuringElementEx(7,7, 5,5, CV_SHAPE_RECT,NULL), 3);
  cvErode( gcs->pImgGRAY_diff, gcs->pImgGRAY_diff, cvCreateStructuringElementEx(5,5, 3,3, CV_SHAPE_RECT,NULL), 4);

  //////////////////////////////////////////////////////////////////////////////
  // use either Ghost or boxes-model to create a PR foreground starting point in gcs->grabcut_mask
  if( gcs->ghostfilename)
    compose_grabcut_seedmatrix3(gcs->grabcut_mask, gcs->cvGhostBwAffined, gcs->pImgGRAY_diff  );
  else{
    // toss it all to the bbox creation function, together with the face position and size
    compose_grabcut_seedmatrix2(gcs->grabcut_mask, gcs->facepos, gcs->pImgGRAY_diff, gcs->facefound );
  }


  //////////////////////////////////////////////////////////////////////////////
#ifdef KMEANS
  gcs->num_clusters = 18; // keep it even to simplify integer arithmetics
  cvCopy(gcs->pImgRGB, gcs->pImgRGB_kmeans, NULL);
  posterize_image(gcs->pImgRGB_kmeans);
  create_kmeans_clusters(gcs->pImgRGB_kmeans, gcs->kmeans_points, gcs->kmeans_clusters, 
                         gcs->num_clusters, gcs->num_samples);
  adjust_bodybbox_w_clusters(gcs->grabcut_mask, gcs->pImgRGB_kmeans, gcs->num_clusters, gcs->facepos);
#endif //KMEANS


  //////////////////////////////////////////////////////////////////////////////
  if( gcs->debug < 70)
    run_graphcut_iteration( &(gcs->GC), gcs->pImgRGB, gcs->grabcut_mask, &gcs->bbox_prev);



  // get a copy of GRAY for the next iteration
  cvCopy(gcs->pImgGRAY, gcs->pImgGRAY_1, NULL);

  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( gcs->display ){
    int outputimage = gcs->debug;
    switch( outputimage ){
    case 1: // output the GRAY difference
      cvCvtColor( gcs->pImgGRAY_diff, gcs->pImgRGB, CV_GRAY2BGR );
      break;
    case 50:// Ghost remapped
      cvCvtColor( gcs->cvGhostBwAffined, gcs->pImgRGB, CV_GRAY2BGR );
      break;
    case 51:// Ghost applied
      cvAnd( gcs->cvGhostBwAffined, gcs->pImgGRAY, gcs->pImgGRAY, NULL );
      cvCvtColor( gcs->pImgGRAY, gcs->pImgRGB, CV_GRAY2BGR );
      break;
    case 60:// Graphcut
      cvAndS(gcs->grabcut_mask, cvScalar(1), gcs->grabcut_mask, NULL);  // get only FG
      cvConvertScale( gcs->grabcut_mask, gcs->grabcut_mask, 127.0);
      cvCvtColor( gcs->grabcut_mask, gcs->pImgRGB, CV_GRAY2BGR );
      break;
    case 61:// Graphcut applied on input/output image
      cvAndS(gcs->grabcut_mask, cvScalar(1), gcs->grabcut_mask, NULL);  // get only FG, PR_FG
      cvConvertScale( gcs->grabcut_mask, gcs->grabcut_mask, 255.0);
      cvAnd( gcs->grabcut_mask,  gcs->pImgGRAY,  gcs->pImgGRAY, NULL);
      cvCvtColor( gcs->pImgGRAY, gcs->pImgRGB, CV_GRAY2BGR );

      cvRectangle(gcs->pImgRGB, cvPoint(gcs->bbox_now.x, gcs->bbox_now.y), 
                  cvPoint(gcs->bbox_now.x + gcs->bbox_now.width, gcs->bbox_now.y+gcs->bbox_now.height),
                  cvScalar(127,0.0), 1, 8, 0 );
     break;
    case 70:// bboxes
      cvZero( gcs->pImgGRAY );
      cvMul( gcs->grabcut_mask,  gcs->grabcut_mask,  gcs->pImgGRAY, 40.0 );
      cvCvtColor( gcs->pImgGRAY, gcs->pImgRGB, CV_GRAY2BGR );
      break;
    case 71:// bboxes applied on the original image
      cvAndS(gcs->grabcut_mask, cvScalar(1), gcs->grabcut_mask, NULL);  // get only FG, PR_FG
      cvMul( gcs->grabcut_mask,  gcs->pImgGRAY,  gcs->pImgGRAY, 1.0 );
      cvCvtColor( gcs->pImgGRAY, gcs->pImgRGB, CV_GRAY2BGR );
      break;
    case 72: // input alpha channel mapped to output
      cvCvtColor( gcs->pImgChX, gcs->pImgRGB, CV_GRAY2BGR );
      break;
#ifdef KMEANS
    case 80:// k-means output
      cvCopy(gcs->pImgRGB_kmeans, gcs->pImgRGB, NULL);
      break;
    case 81:// k-means output filtered with bbox/ghost mask
      cvSplit(gcs->pImgRGB_kmeans, gcs->pImgCh1, gcs->pImgCh2, gcs->pImgCh3, NULL        );
      cvAndS(gcs->grabcut_mask, cvScalar(1), gcs->grabcut_mask, NULL);  // get FG and PR_FG
      cvConvertScale( gcs->grabcut_mask, gcs->grabcut_mask, 255.0);     // scale any to 255.

      cvAnd( gcs->grabcut_mask,  gcs->pImgCh1,  gcs->pImgCh1, NULL );
      cvAnd( gcs->grabcut_mask,  gcs->pImgCh2,  gcs->pImgCh2, NULL );
      cvAnd( gcs->grabcut_mask,  gcs->pImgCh3,  gcs->pImgCh3, NULL );

      cvMerge(              gcs->pImgCh1, gcs->pImgCh2, gcs->pImgCh3, NULL, gcs->pImgRGB);
      break;
#endif //KMEANS
    default:
      break;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // copy anyhow the fg/bg to the alpha channel in the output image alpha ch
  cvSplit(gcs->pImgRGB, gcs->pImgCh1, gcs->pImgCh2, gcs->pImgCh3, NULL        );
  cvAndS(gcs->grabcut_mask, cvScalar(1), gcs->grabcut_mask, NULL);  // get only FG and possible FG
  cvConvertScale( gcs->grabcut_mask, gcs->grabcut_mask, 255.0);
  gcs->pImgChA->imageData = (char*)gcs->grabcut_mask->data.ptr;

  cvMerge(              gcs->pImgCh1, gcs->pImgCh2, gcs->pImgCh3, gcs->pImgChA, gcs->pImageRGBA);

  gcs->numframes++;

  GST_GCS_UNLOCK (gcs);  
  
  return GST_FLOW_OK;
}


gboolean gst_gcs_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "gcs", GST_RANK_NONE, GST_TYPE_GCS);
}









////////////////////////////////////////////////////////////////////////////////
gboolean get_frame_difference( IplImage* in, IplImage* inprev, IplImage* output)
{
  cvSmooth(in,     in,     CV_GAUSSIAN, 5);
  cvSmooth(inprev, inprev, CV_GAUSSIAN, 5);

  cvAbsDiff( in, inprev, output);
  cvThreshold( output, output, 5, 255, CV_THRESH_BINARY);
  cvMorphologyEx( output, output, 0, 0, CV_MOP_CLOSE, 1 );
  return(TRUE);
}


// copied, otherwise only available in C++
enum{  
  GCS_BGD    = 0,  //!< background
  GCS_FGD    = 1,  //!< foreground
  GCS_PR_BGD = 2,  //!< most probably background
  GCS_PR_FGD = 3   //!< most probably foreground
};   


////////////////////////////////////////////////////////////////////////////////
void compose_grabcut_seedmatrix2(CvMat* output, CvRect facebox,  IplImage* seeds, gboolean confidence)
{
  cvSet(output, cvScalar(GCS_PR_BGD), NULL);

  double a=(confidence) ? 0.85 : 0.55;  //extra growing of body box region
  double A=1.20;  // body bbox is lower than face bbox (neck)
  double b=1+a;

  int bodyx0 = ((facebox.x-facebox.width*   a) < 0 ) ? 0 : (facebox.x-facebox.width* a);
  int bodyy0 = ((facebox.y+facebox.height * A) > output->rows) ? output->rows : (facebox.y+facebox.height*A );
  int bodyx1 = ((facebox.x+facebox.width*   b) > output->cols) ? output->cols : (facebox.x+facebox.width*b);
  int bodyy1 = (output->rows);

  double c=-0.10;  //extra growing of face bbox region, horizontal axis
  double C= 0.00;  //extra growing of face bbox region, vertical axis
  double d= 1+c;
  double D= 4+C;

  int facex0 = ((facebox.x-facebox.width  *c) < 0 ) ? 0 : (facebox.x - facebox.width *c);
  int facey0 = ((facebox.y-facebox.height *C) < 0)  ? 0 : (facebox.y - facebox.height*C);
  int facex1 = ((facebox.x+facebox.width  *d) > output->cols) ? output->cols : (facebox.x+facebox.width *d);
  int facey1 = ((facebox.y+facebox.height *D) > output->rows) ? output->rows : (facebox.y+facebox.height*D);

  int x, y;
  for( x = 0; x < output->cols; x++ ){
    for( y = 0; y < output->rows; y++ ){

      // large bbox around face
      if( ( x >= facex0 ) && ( x <= facex1) && ( y >= facey0 ) && ( y <= facey1))
        CV_MAT_ELEM(*output, uchar, y, x) = GCS_PR_FGD;
      // body bbox: ONLY IF WE DONT GET IT FROM CLUSTERING COLOURS (K-MEANS)
#ifndef KMEANS
#if 0
      if( ( x >= bodyx0)  && ( x <= bodyx1) && ( y >= bodyy0)  && ( y <= bodyy1))
        CV_MAT_ELEM(*output, uchar, y, x) = GCS_PR_FGD;
#else
      double delta= 0.30 * facebox.width;
      int bodyxdelta = (delta)*(y-bodyy0)/(bodyy1-bodyy0); //pyramid-like shape
      if( ( x >= bodyx0-bodyxdelta)  && ( x <= bodyx1+bodyxdelta) && 
          ( y >= bodyy0)             && ( y <= bodyy1))
        CV_MAT_ELEM(*output, uchar, y, x) = GCS_PR_FGD;

#endif
#endif //!KMEANS

      // seeds, usually coming from movement, could also be skin or a combination
      if( seeds && ( (cvGetReal2D( seeds, y, x) > 10) ) )
        CV_MAT_ELEM(*output, uchar, y, x) = GCS_PR_FGD ;
    }
  }

  
}

////////////////////////////////////////////////////////////////////////////////
void compose_grabcut_seedmatrix3(CvMat* output, IplImage* ghost ,  IplImage* seeds)
{
  cvSet(output, cvScalar(GCS_PR_BGD), NULL);

  int x, y;
  float  val;
  for( x = 0; x < output->cols; x++ ){
    for( y = 0; y < output->rows; y++ ){      
      val = cvGetReal2D( ghost, y, x);

      CV_MAT_ELEM(*output, uchar, y, x) = 
        (val < 100 ) ? GCS_BGD : \
         (val < 150 ) ? GCS_PR_BGD : \
          (val < 200 ) ? GCS_PR_FGD : \
                          GCS_FGD ;

      // seeds, usually coming from movement, could also be skin or a combination
      if( seeds && ( (cvGetReal2D( seeds, y, x) > 10) ) )
        CV_MAT_ELEM(*output, uchar, y, x) = (CV_MAT_ELEM(*output, uchar, y, x)== GCS_FGD) ? GCS_FGD : GCS_PR_FGD ;
    }
  }

  
}


////////////////////////////////////////////////////////////////////////////////
gint compose_skin_matrix(IplImage* rgbin, IplImage* gray_out)
{
/*
  int skin_under_seed = 0;

  static IplImage* imageHSV = cvCreateImage( cvSize(rgbin->width, rgbin->height), IPL_DEPTH_8U, 3);
  cvCvtColor(rgbin, imageHSV, CV_RGB2HSV);

  static IplImage* planeH = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Hue component.
  ///IplImage* planeH2= cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Hue component, 2nd threshold
  static IplImage* planeS = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Saturation component.
  static IplImage* planeV = cvCreateImage( cvGetSize(imageHSV), 8, 1);	// Brightness component.
  cvCvtPixToPlane(imageHSV, planeH, planeS, planeV, 0);	// Extract the 3 color components.

  // Detect which pixels in each of the H, S and V channels are probably skin pixels.
  // Assume that skin has a Hue between 0 to 18 (out of 180), and Saturation above 50, and Brightness above 80.
  ///cvThreshold(planeH , planeH2, 10, UCHAR_MAX, CV_THRESH_BINARY);         //(hue > 10)
  cvThreshold(planeH , planeH , 20, UCHAR_MAX, CV_THRESH_BINARY_INV);     //(hue < 20)
  cvThreshold(planeS , planeS , 48, UCHAR_MAX, CV_THRESH_BINARY);         //(sat > 48)
  cvThreshold(planeV , planeV , 80, UCHAR_MAX, CV_THRESH_BINARY);         //(val > 80)

  // erode the HUE to get rid of noise.
  cvErode(planeH, planeH, NULL, 1);

  // Combine all 3 thresholded color components, so that an output pixel will only
  // be white (255) if the H, S and V pixels were also white.

  // gray_out = (hue > 10) ^ (hue < 20) ^ (sat > 48) ^ (val > 80), where   ^ mean pixels-wise AND
  cvAnd(planeH  , planeS , gray_out);	
  //cvAnd(gray_out, planeH2, gray_out);	
  cvAnd(gray_out, planeV , gray_out);	

  return(skin_under_seed);
*/

  static IplImage* planeR  = cvCreateImage( cvGetSize(rgbin), 8, 1);	// R component.
  static IplImage* planeG  = cvCreateImage( cvGetSize(rgbin), 8, 1);	// G component.
  static IplImage* planeB  = cvCreateImage( cvGetSize(rgbin), 8, 1);	// B component.

  static IplImage* planeAll = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1);	// (R+G+B) component.
  static IplImage* planeR2  = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1);	// R component, 32bits
  static IplImage* planeRp  = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1);	// R' and >0.4
  static IplImage* planeGp  = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1);	// G' and > 0.28

  static IplImage* planeRp2 = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1);	// R' <0.6
  static IplImage* planeGp2 = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1);	// G' <0.4

  cvCvtPixToPlane(rgbin, planeR, planeG, planeB, 0);	// Extract the 3 color components.
  cvAdd( planeR, planeG,   planeAll, NULL);
  cvAdd( planeB, planeAll, planeAll, NULL);  // All = R + G + B
  cvDiv( planeR, planeAll, planeRp, 1.0);    // R' = R / ( R + G + B)
  cvDiv( planeG, planeAll, planeGp, 1.0);    // G' = G / ( R + G + B)

  cvConvertScale( planeR, planeR2, 1.0, 0.0);
  cvCopy(planeGp, planeGp2, NULL);
  cvCopy(planeRp, planeRp2, NULL);

  cvThreshold(planeR2 , planeR2,   60, UCHAR_MAX, CV_THRESH_BINARY);     //(R > 60)
  cvThreshold(planeRp , planeRp, 0.40, UCHAR_MAX, CV_THRESH_BINARY);     //(R'> 0.4)
  cvThreshold(planeRp2, planeRp2, 0.6, UCHAR_MAX, CV_THRESH_BINARY_INV); //(R'< 0.6)
  cvThreshold(planeGp , planeGp, 0.28, UCHAR_MAX, CV_THRESH_BINARY);     //(G'> 0.28)
  cvThreshold(planeGp2, planeGp2, 0.4, UCHAR_MAX, CV_THRESH_BINARY_INV); //(G'< 0.4)

  // R’ = R / (R+G+B), G’ = G / (R + G + B)
  //  Skin pixel if:
  // R > 60 AND R’ > 0.4 AND R’ < 0.6 AND G’ > 0.28 and G’ < 0.4  
  static IplImage* imageSkinPixels = cvCreateImage( cvGetSize(rgbin), IPL_DEPTH_32F, 1); 
  cvAnd( planeR2 ,         planeRp , imageSkinPixels);	
  cvAnd( planeRp , imageSkinPixels , imageSkinPixels);	
  cvAnd( planeRp2, imageSkinPixels , imageSkinPixels);	
  cvAnd( planeGp , imageSkinPixels , imageSkinPixels);	
  cvAnd( planeGp2, imageSkinPixels , imageSkinPixels);	
  
  cvConvertScale( imageSkinPixels, gray_out, 1.0, 0.0);
  return(0);
}




#ifdef KMEANS

////////////////////////////////////////////////////////////////////////////////
void create_kmeans_clusters(IplImage* in, CvMat* points, CvMat* cluster, int numclusters, int numsamples)
{
  float weight = 0.25;

  CvScalar colour;
  int x,y, i=0;

  cvZero( points );
  cvZero( cluster);

  for( x = 0; x < in->width; x++ ){
    for( y = 0; y < in->height; y++ ){      
      colour = cvGet2D( in, y, x);
      
      //printf(" %d,%d, %d\n", x ,y, i);
      CV_MAT_ELEM( *points, float, i  , 0) = (1- weight) * colour.val[0]; // R or B
      CV_MAT_ELEM( *points, float, i  , 1) = (1- weight) * colour.val[1]; // G
      CV_MAT_ELEM( *points, float, i  , 2) = (1- weight) * colour.val[2]; // B or R
      CV_MAT_ELEM( *points, float, i  , 3) = weight * (x*255/in->width);  // x in [0,255]
      CV_MAT_ELEM( *points, float, i  , 4) = weight * (y*255/in->height); // y in [0,255]      

      i++; // dont put into the CV_MAT_ELEM !!!!
    }
  }
  
  // points is a matrix of N rows where each row has the points to cluster, here: RGBXY
  cvKMeans2( points, numclusters, cluster, cvTermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 10, 1.0 ));
  
  // back propagate colours to clusters
  cvZero( in );
  i = 0;
  for( x = 0; x < in->width; x++ ){
    for( y = 0; y < in->height; y++ ){      
      cvSet2D( in, y, x,  CV_RGB(127, (cluster->data.i[i++] + 1)*255 / numclusters, 127));
      // careful to put the moving value in the 2nd coordinate, is used in adjust_bodybbox_w_clusters()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
void adjust_bodybbox_w_clusters(CvMat* mask, IplImage* cluster, int numclusters, CvRect facebox )
{
  double a=1.15;  //extra growing of body box region
  double A=1.00;  // body bbox is lower than face bbox (neck)
  double b=1+a;

  int bodyx0 = ((facebox.x-facebox.width*   a) < 0 ) ? 0 : (facebox.x-facebox.width* a);
  int bodyy0 = ((facebox.y+facebox.height * A) > cluster->height) ? cluster->height : (facebox.y+facebox.height*A );
  int bodyx1 = ((facebox.x+facebox.width*   b) > cluster->width) ? cluster->width : (facebox.x+facebox.width*b);
  int bodyy1 = (cluster->height);


  int x,y, i=0;
  int *accu;
  int eqclass_1st;      // equivalence class chosen as most populated
  int tmp_eqclass;      // temp, to hold the equivalence class associated to a pixel
  int eqclass_2nd;      // equivalence class chosen as 2nd most populated

  accu = (int*)malloc( numclusters*sizeof(int));
  bzero( accu, numclusters*sizeof(int));
  eqclass_1st = 0;
  eqclass_2nd = numclusters-1; // just initialised to sth != eqclass_1st

  // 1st: get to know the amount of pixels per equivalence class ("cluster")
  // but not blindly: only those in the FG mask already
  for( x = 0; x < cluster->width; x++ ){
    for( y = 0; y < cluster->height; y++ ){      
      // filter the equ_classes using the mask
      if( ( x >= bodyx0)  && ( x <= bodyx1) && ( y >= bodyy0)  && ( y <= bodyy1)){
        tmp_eqclass = (int) ( round(cvGet2D( cluster, y, x).val[1]*numclusters/255.0) -1);
        accu[ tmp_eqclass ] ++;
      }
    }
  }

  // 2nd: get the most populated and the 2nd most populated cluster
  for( i = 0; i< numclusters; i++ ){
    eqclass_1st = ( accu[i] > accu[eqclass_1st] ) ? i : eqclass_1st;
    eqclass_2nd = ( accu[i] > accu[eqclass_2nd] ) ? ((accu[i] < accu[eqclass_1st]) ? i : eqclass_2nd ): eqclass_2nd;
    printf(" %.8d ", accu[i]);
  }
  printf(" (eqclass_1st %d  2nd %d) \n", eqclass_1st, eqclass_2nd);


  // 3rd: Using the pixels inside of a seed of the body bbox, calculated from the face box,
  // we calculate the (minx,miny)-(maxx,maxy) bounding rectangle due to the largest cluster
  int minx=10000, miny=10000, maxx=0, maxy=0;
  for( x = 0; x < cluster->width; x++ ){
    for( y = 0; y < cluster->height; y++ ){      

      if(!( ( x >= bodyx0)  && ( x <= bodyx1) && ( y >= bodyy0)  && ( y <= bodyy1) )){
        cvSet2D(cluster, y, x, cvScalar(0, 0, 0 ) );
        continue;
      }

      tmp_eqclass = (int) ( round(cvGet2D( cluster, y, x).val[1]*numclusters/255.0) -1);

      if(tmp_eqclass == eqclass_1st){
        cvSet2D(cluster, y, x, cvScalar(255, 0, 0 ) ); // for display purposes
        maxx = ( maxx > x ) ? maxx : x;
        maxy = ( maxy > y ) ? maxy : y;
        minx = ( minx < x ) ? minx : x;
        miny = ( miny < y ) ? miny : y;
      }
      else if (tmp_eqclass == eqclass_2nd) {
        cvSet2D(cluster, y, x, cvScalar(100, 0, 0 ) ); // for display purposes
      }
      else{
        cvSet2D(cluster, y, x, cvScalar(10, 0, 0 ) );  // for display purposes
      }
    }
  }

  cvRectangle(cluster, cvPoint(minx, miny), cvPoint(maxx, maxy), cvScalar(255, 0, 255), 1);
  // Last: compose in the mask a body-box based on the largest cluster bbox
  // the rectangle is needed otherwise the largest cluster has loads of holes
  cvRectangle(mask, cvPoint(minx, miny), cvPoint(maxx, cluster->height), cvScalar(GC_PR_FGD, 0, 0), CV_FILLED);

  free( accu );


}



////////////////////////////////////////////////////////////////////////////////
// [http://stackoverflow.com/questions/4831813/image-segmentation-using-mean-shift-explained]
// "The Mean Shift segmentation is a local homogenization technique that is 
// very useful for damping shading or tonality differences in localized objects. 
// [...] replaces each pixel with the mean of the pixels in a range-r 
// neighborhood and whose value is within a distance d."
static void  posterize_image(IplImage* input)
{
  // this line reduces the amount of colours
  cvAndS( input, cvScalar(0xF8, 0xF8, 0xF8), input);
  
  int colour_radius =  4; // number of colour classes ~= 256/colour_radius
  int pixel_radius  =  2;
  int levels        =  1;
  cvPyrMeanShiftFiltering(input, input, pixel_radius, colour_radius, levels);
  
}

#endif //KMEANS


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static gboolean gst_gcs_sink_event(GstPad *pad, GstEvent * event)
{
  GstGcs *gcs = GST_GCS (gst_pad_get_parent( pad ));
  gboolean ret = FALSE, facefound;
  double x,y,w,h;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &x); // check bool return
      gst_structure_get_double(str, "y", &y); // check bool return
      gst_structure_get_double(str, "width", &w); // check bool return
      gst_structure_get_double(str, "height", &h);// check bool return
      gst_structure_get_boolean(str,"facefound", &facefound);// check bool return

      gcs->facepos.x = (int)x;
      gcs->facepos.y = (int)y;
      gcs->facepos.width = (int)w;
      gcs->facepos.height = (int)h;
      gcs->facefound      = facefound;

      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(gcs);
  return ret;
}







static void png_warning_fn(png_structp png_ptr, png_const_charp warning_msg) {
//		  gst_debug_log(gst_ghostmapper_debug, GST_LEVEL_LOG, __FILE__, __FUNCTION__ , __LINE__, NULL, "libpng warning: %s",  warning_msg);
		  
}
static void png_error_fn(png_structp png_ptr,  png_const_charp error_msg) {
//		  gst_debug_log(gst_picsrcjpeg_debug, GST_LEVEL_WARNING, __FILE__, __FUNCTION__ , __LINE__, NULL, "libpng error: %s",  error_msg);
}

gboolean read_png(curlMemoryStructGCS *chunk, unsigned char **raw_image, picSrcImageInfoGCS *info, char *errBuf) 
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
    info->colorspace =  COLOR_TYPE_GCS_RGB;          
    break;
  case PNG_COLOR_TYPE_GRAY:
    if (bitdepth < 8)
      png_set_gray_1_2_4_to_8(png_ptr);
    //And the bitdepth info
    bitdepth = 8;
    png_set_gray_to_rgb(png_ptr);
    info->colorspace =  COLOR_TYPE_GCS_RGB;       
    break;
  }
  /*if the image has a transperancy set.. convert it to a full Alpha channel..*/
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png_ptr);
    channels+=1;
    info->colorspace =  COLOR_TYPE_GCS_RGB_ALPHA;   
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




gboolean curl_download(const char *uri, const char *accessToken, curlMemoryStructGCS *chunk, char *curlErrBuf) {
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
  struct curlMemoryStructGCS *mem = (struct curlMemoryStructGCS *)data;
 
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




