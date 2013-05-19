/* GStreamer
 */
/**
 * SECTION:element- tsm
 *
 * This element is a wrapper over TSM set of algorithm creating a temporal stable mask
 * that segments FG/BG of an image based on a graphcut expansion of OF tracked feats.
 *
 * Dense and Sparse Optic Flows Aggregation for Accurate Motion Segmentation in Monocular Video Sequences
 * Mihai Fǎgǎdar-Cosma, Vladimir-Ioan Creţu and Mihai Victor Micea
 * Lecture Notes in Computer Science, 2012, Volume 7324, Image Analysis and Recognition, Pages 208-215
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gsttsm.h"

#include "grabcut_wrapper.hpp"


GST_DEBUG_CATEGORY_STATIC (gst_tsm_debug);
#define GST_CAT_DEFAULT gst_tsm_debug


enum {
	PROP_0,
        PROP_DISPLAY,
        PROP_DEBUG,
	PROP_LAST
};

static GstStaticPadTemplate gst_tsm_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_tsm_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_TSM_LOCK(tsm) G_STMT_START { \
	GST_LOG_OBJECT (tsm, "Locking tsm from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&tsm->lock); \
	GST_LOG_OBJECT (tsm, "Locked tsm from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_TSM_UNLOCK(tsm) G_STMT_START { \
	GST_LOG_OBJECT (tsm, "Unlocking tsm from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&tsm->lock); \
} G_STMT_END

static gboolean gst_tsm_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_tsm_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_tsm_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_tsm_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_tsm_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_tsm_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_tsm_finalize(GObject * object);
static gboolean gst_tsm_sink_event(GstPad *pad, GstEvent * event);

static gboolean get_frame_difference( IplImage* in, IplImage* inprev, IplImage* output);

static void local_postprocess_dof( CvMat* flowin, IplImage* output, int contour_minsize);
static  int local_calculate_pyrlk(IplImage *inputimage, IplImage *inputimage2, int num_vertexes, 
                                 CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error, CvPoint2D32f* vertexesB);
static void create_image_from_flow(IplImage *image, CvMat* flow );
static void create_image_from_features(IplImage *image, int num_vertexes, 
                                       CvPoint2D32f* vertexes, char *vertexes_found, float *vertexes_error);
static void create_labels_dictionary(CvMat* dictionary, IplImage* image, IplImage* labels);
static void local_calculate_block_statistics(IplImage *image, int blocksize, 
                                             CvMat* blockstat_avg, CvMat* blockstat_dev);
static void local_integrate_motion_priors(IplImage *motion, int blocksize, 
                                          CvMat* block_avg,   CvMat* block_dev,
                                          CvMat* block_avg_1, CvMat* block_dev_1,
                                          CvMat *tsm, CvMat* pfg, double threshold );

static void local_draw_dof_map(const CvMat* flow,IplImage* cflowmap, int step,
                               double scale, CvScalar color);
static void local_draw_pyrlk_vectors(IplImage *image, int num_vertexes, 
                                     CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error,  
                                     CvPoint2D32f* vertexesB  );

static double distance_between_distros(double mu0, double sig0, double mu1, double sig1);
static void prune_image( CvMat* image, double threshold);
static void compose_grabcut_seedmatrix(IplImage* fg, IplImage* pfg, CvMat* output, CvSubdiv2D* subdiv, bool inverted);
static void compose_grabcut_seedmatrix2(CvMat* output, CvRect facebox);
static  CvRect find_bbox_from_cloudofpoints(IplImage* fg);

static void draw_subdiv_edge( IplImage* img, CvSubdiv2DEdge edge, CvScalar color );
static void draw_subdiv( IplImage* img, CvSubdiv2D* subdiv,
                         CvScalar delaunay_color, CvScalar voronoi_color );



GST_BOILERPLATE (GstTsm, gst_tsm, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanTsm(GstTsm *tsm) 
{
  if (tsm->pImageRGBA)  cvReleaseImageHeader(&tsm->pImageRGBA);
}

static void gst_tsm_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Image Segmentation filter - Fagadar TSM model", "Filter/Effect/Video",
    "Creates a FG/BG mask based on Fagadar's TSM algorithm",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_tsm_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_tsm_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_tsm_debug, "tsm", 0, \
                           "tsm - Creates a FG/BG mask based on Fagadar's TSM algorithm");
}

static void gst_tsm_class_init(GstTsmClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_tsm_set_property;
  gobject_class->get_property = gst_tsm_get_property;
  gobject_class->finalize = gst_tsm_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_tsm_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_tsm_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_tsm_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_tsm_set_caps);

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
}

static void gst_tsm_init(GstTsm * tsm, GstTsmClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)tsm, TRUE);
  g_static_mutex_init(&tsm->lock);

  tsm->pImageRGBA        = NULL;

  tsm->pImgRGB           = NULL;
  tsm->pImgScratch       = NULL;
  tsm->pImgRGB_1         = NULL;

  tsm->pImgGRAY          = NULL;
  tsm->pImgGRAY_copy     = NULL;
  tsm->pImgGRAY_1        = NULL;
  tsm->pImgGRAY_1copy    = NULL;

  tsm->pImgEDGE          = NULL;
  tsm->pImgEDGE_1        = NULL;

  tsm->pImgChA           = NULL;
  tsm->pImgCh1           = NULL;
  tsm->pImgCh2           = NULL;
  tsm->pImgCh3           = NULL;

  tsm->display       = false;
  tsm->debug         = 0;
}

static void gst_tsm_finalize(GObject * object) 
{
  GstTsm *tsm = GST_TSM (object);
  
  GST_TSM_LOCK (tsm);
  CleanTsm(tsm);
  GST_TSM_UNLOCK (tsm);
  GST_INFO("Tsm destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&tsm->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_tsm_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstTsm *tsm = GST_TSM (object);
  
  GST_TSM_LOCK (tsm);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    tsm->display = g_value_get_boolean(value);
    break;    
  case PROP_DEBUG:
    tsm->debug = g_value_get_int(value);
    break;    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_TSM_UNLOCK (tsm);
}

static void gst_tsm_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstTsm *tsm = GST_TSM (object);

  switch (prop_id) {
  case PROP_DISPLAY:
    g_value_set_boolean(value, tsm->display);
    break; 
  case PROP_DEBUG:
    g_value_set_int(value, tsm->debug);
    break; 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_tsm_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}


static gboolean gst_tsm_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstTsm *tsm = GST_TSM (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_TSM_LOCK (tsm);
  
  gst_video_format_parse_caps(incaps, &tsm->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &tsm->out_format, &out_width, &out_height);
  if (!(tsm->in_format == tsm->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_TSM_UNLOCK (tsm);
    return FALSE;
  }
  
  tsm->width  = in_width;
  tsm->height = in_height;
  
  GST_INFO("Initialising Tsm...");
  gst_pad_set_event_function(GST_BASE_TRANSFORM_SINK_PAD(tsm),  gst_tsm_sink_event);

  const CvSize size = cvSize(tsm->width, tsm->height);
  GST_WARNING (" width %d, height %d", tsm->width, tsm->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  tsm->pImageRGBA    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  tsm->pImgRGB       = cvCreateImage(size, IPL_DEPTH_8U, 3);
  tsm->pImgScratch   = cvCreateImage(size, IPL_DEPTH_8U, 3);
  tsm->pImgRGB_1     = cvCreateImage(size, IPL_DEPTH_8U, 3);
  cvZero( tsm->pImgRGB_1 );

  tsm->pImgGRAY      = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgGRAY_copy = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgGRAY_diff = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgGRAY_1    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgGRAY_1copy= cvCreateImage(size, IPL_DEPTH_8U, 1);
  cvZero( tsm->pImgGRAY_1 );
  cvZero( tsm->pImgGRAY_1copy );
  tsm->pImgGRAY_flow = cvCreateMat(tsm->height, tsm->width, CV_32FC2);
  cvZero( tsm->pImgGRAY_flow );

  tsm->pImg_DOFMask  = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImg_DOFMap   = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImg_Codebook = cvCreateImage(size, IPL_DEPTH_8U, 1);

  tsm->pImg_DistMask = cvCreateImage(size, IPL_DEPTH_32F, 1);
  tsm->pImg_LablMask = cvCreateImage(size, IPL_DEPTH_32S, 1);

  tsm->pImgEDGE      = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgEDGE_1    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  cvZero( tsm->pImgEDGE_1 );

  tsm->pImgChA       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgCh1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgCh2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImgCh3       = cvCreateImage(size, IPL_DEPTH_8U, 1);

  tsm->num_corners   = 2000;
  tsm->cornersA      = (CvPoint2D32f*)malloc(sizeof(CvPoint2D32f)*tsm->num_corners);
  tsm->cornersB      = (CvPoint2D32f*)malloc(sizeof(CvPoint2D32f)*tsm->num_corners);
  tsm->cornersB_asMat= cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->corners_found = (char*)  malloc(sizeof(char)* tsm->num_corners );
  tsm->corners_error = (float*) malloc(sizeof(float)* tsm->num_corners );

  tsm->dictionary     = cvCreateMat( tsm->num_corners, 2, CV_32SC1 );
  tsm->dof_memstorage =  cvCreateMemStorage(0);
  tsm->dof_contours   = NULL;

  tsm->pImg_MergedMask= cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->dof_contours   = NULL;
  tsm->pImg_FinalMask = cvCreateImage(size, IPL_DEPTH_8U, 1);

  tsm->blocksize      = 4;
  tsm->blockstat_avg  = cvCreateMat( size.height/tsm->blocksize + 1, size.width/tsm->blocksize + 1, CV_8UC1);
  tsm->blockstat_dev  = cvCreateMat( size.height/tsm->blocksize + 1, size.width/tsm->blocksize + 1, CV_8UC1);
  tsm->blockstat_avg_1= cvCreateMat( size.height/tsm->blocksize + 1, size.width/tsm->blocksize + 1, CV_8UC1);
  tsm->blockstat_dev_1= cvCreateMat( size.height/tsm->blocksize + 1, size.width/tsm->blocksize + 1, CV_8UC1);
  tsm->pImg_TSM       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  tsm->pImg_PFG       = cvCreateMat( size.height/tsm->blocksize + 1, size.width/tsm->blocksize + 1, CV_8UC1);
  tsm->pImg_TSM_small = cvCreateMat( size.height/tsm->blocksize + 1, size.width/tsm->blocksize + 1, CV_8UC1);

  cvZero(tsm->pImg_PFG);
  cvZero(tsm->pImg_TSM_small);


  tsm->grabcut_mask   = cvCreateMat( size.height, size.width, CV_8UC1);
  cvZero(tsm->grabcut_mask);
  initialise_grabcut( tsm->pImgRGB, tsm->grabcut_mask );
  tsm->bbox_prev      = cvRect( 60,70, 210, 170 );

  tsm->subdivstorage  = cvCreateMemStorage(0);
  tsm->subdiv         = cvCreateSubdiv2D( CV_SEQ_KIND_SUBDIV2D, sizeof(*tsm->subdiv),
                                          sizeof(CvSubdiv2DPoint),
                                          sizeof(CvQuadEdge2D),
                                          tsm->subdivstorage );
  cvInitSubdivDelaunay2D( tsm->subdiv, cvRect(0,0, 600,600 ));

  tsm->numframes = 0;

  GST_INFO("Tsm initialized.");
  
  GST_TSM_UNLOCK (tsm);
  
  return TRUE;
}

static void gst_tsm_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_tsm_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstTsm *tsm = GST_TSM (btrans);

  GST_TSM_LOCK (tsm);

  //////////////////////////////////////////////////////////////////////////////
  // get image data from the input, which is RGBA or BGRA
  tsm->pImageRGBA->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(tsm->pImageRGBA,  tsm->pImgRGB, CV_BGRA2BGR);

  //////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////MOTION CUES INTEGR////
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
  // apply step 1. filtering using bilateral filter. Cannot happen in-place => scratch
  cvSmooth(tsm->pImgRGB, tsm->pImgScratch, CV_BILATERAL, 3, 50, 3, 0);
  // create GRAY image
  cvCvtColor(tsm->pImgScratch, tsm->pImgGRAY, CV_BGR2GRAY);
  // Canny the GRAY to obtain the EDGE image. 48 are both thresholds
  cvCanny    ( tsm->pImgGRAY, tsm->pImgEDGE, 48, 48);
  cvThreshold( tsm->pImgEDGE, tsm->pImgEDGE, 48, 255, CV_THRESH_BINARY);
  // Frame difference the GRAY and the previous one
  // not intuitive: first smooth frames, then 
  cvCopy( tsm->pImgGRAY,   tsm->pImgGRAY_copy,  NULL);
  cvCopy( tsm->pImgGRAY_1, tsm->pImgGRAY_1copy, NULL);
  get_frame_difference( tsm->pImgGRAY_copy, tsm->pImgGRAY_1copy, tsm->pImgGRAY_diff);
  //////////////////////////////////////////////////////////////////////////////
  

  //////////////////////////////////////////////////////////////////////////////
  // apply Dense Optical Flow on the Gray images (current and previous, original)
  cvCalcOpticalFlowFarneback( tsm->pImgGRAY_1, tsm->pImgGRAY, tsm->pImgGRAY_flow, 
                              0.25,   // pyr scale
                              3,      // levels
                              10,     // winsize
                              2,      // iterations
                              9,      // polyN
                              1.5,    // polysigma
                              0);     // flags, can be OPTFLOW_FARNEBACK_GAUSSIAN
  // now construct a dense OF mask from the results of the Farneback function
  local_postprocess_dof(tsm->pImgGRAY_flow, tsm->pImg_DOFMask, 80); // min pixel area 20


  //////////////////////////////////////////////////////////////////////////////
  // Calculate Sparse Optical Flow on the edge image
  int found_vertexes = local_calculate_pyrlk( tsm->pImgEDGE_1, tsm->pImgEDGE, 
                                              tsm->num_corners, tsm->cornersA, tsm->corners_found, tsm->corners_error, tsm->cornersB);

  // correct the LK OF with the frame difference: only keep the vectors that
  // coincide with it. For this, Fagadar uses tsm->cornersB, it is a vector
  // so we need to pass it first to a matrix, where a {1} means moved point
  // with a certain confidence (error<550), visually looks ok
  create_image_from_features(tsm->cornersB_asMat, tsm->num_corners, tsm->cornersB, tsm->corners_found, tsm->corners_error);
  cvAnd(tsm->cornersB_asMat, tsm->pImgGRAY_diff, tsm->cornersB_asMat, NULL);

  //////////////////////////////////////////////////////////////////////////////
  // now calculate the fg/bg mask using a simple codebook approach. 

  //todo
  cvZero( tsm->pImg_Codebook );


  //////////////////////////////////////////////////////////////////////////////
  // now we fuse the dense and sparse and codebook approaches
  cvZero( tsm->pImg_MergedMask );
  // first onestep: OR dense and codebooks
  cvOr( tsm->pImg_DOFMask, tsm->pImg_Codebook, tsm->pImg_DOFMask, NULL);
  // in tsm->cornersB_asMat, we already have the sparse feats as points, 
  // first swap black and white in the LK;
  cvThreshold(tsm->cornersB_asMat, tsm->cornersB_asMat, 127.0, 255.0, CV_THRESH_BINARY_INV);
  // apply distance transform on the Sparse OF features, get Distance and Label maps
  cvDistTransform( tsm->cornersB_asMat, tsm->pImg_DistMask, CV_DIST_L2, 3, NULL,  tsm->pImg_LablMask);

  // after the distance transform we need to know the label associated to each pixel, a "dictionary"
  create_labels_dictionary( tsm->dictionary, tsm->cornersB_asMat, tsm->pImg_LablMask);

  // now we need to reshape the Dense OF contours into the closest Sparse OF feature;
  int numcontours = cvFindContours( tsm->pImg_DOFMask, tsm->dof_memstorage, &(tsm->dof_contours), 
                                    sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE );  
  // --> for each contour, get the labels assigned to each contour control point
  // then create another contour with the point corresponding to that label
  for( CvSeq* c = tsm->dof_contours ; c!=NULL; c=c->h_next ){
    for( int i=0; i < c->total; i++ ){
      // retrieve the point i of contour c; NOTE: it is no copy, is THE element
      CvPoint *p = (CvPoint*) cvGetSeqElem( c, i);
      // get the associated label in the  tsm->pImg_LablMask. Note the swapped indexes!
      int label = cvGetReal2D( tsm->pImg_LablMask, p->y, p->x );
      if(label){
        // and back substitute the sparse point associated to that label, in the contour
        p->x = cvGetReal2D( tsm->dictionary, label, 1 );
        p->y = cvGetReal2D( tsm->dictionary, label, 0 );            
      }
    }
  }
  cvDrawContours( tsm->pImg_MergedMask, tsm->dof_contours, cvScalarAll(255), cvScalarAll(255), 100 );

  // Finally we find the contours on the remaining figure, which is otherwise too spiky
  int numcontours2= cvFindContours( tsm->pImg_MergedMask, tsm->dof_memstorage, &(tsm->mergedof_contours), 
                                    sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE );  

  cvZero( tsm->pImg_FinalMask );
  cvDrawContours( tsm->pImg_FinalMask, tsm->mergedof_contours, cvScalarAll(255), cvScalarAll(255), 100, CV_FILLED);

  
  //////////////////////////////////////////////////////////////////////////////
  ///////////////////////////////////////////////////////////////////////TSM////
  //////////////////////////////////////////////////////////////////////////////
  
  // calculate statistics (avg, dev) per block for the current and prev frame
  local_calculate_block_statistics(tsm->pImgGRAY_1, tsm->blocksize, 
                                   tsm->blockstat_avg_1, tsm->blockstat_dev_1);
  local_calculate_block_statistics(tsm->pImgGRAY,   tsm->blocksize, 
                                   tsm->blockstat_avg, tsm->blockstat_dev);
  if( tsm->numframes >10){
    
    create_image_from_flow( tsm->pImg_DOFMap, tsm->pImgGRAY_flow);
    local_integrate_motion_priors(tsm->pImg_FinalMask,   tsm->blocksize, 
                                  tsm->blockstat_avg,    tsm->blockstat_dev,
                                  tsm->blockstat_avg_1,  tsm->blockstat_dev_1,
                                  tsm->pImg_TSM_small,   tsm->pImg_PFG,       0.75);
  }
  //prune_image( tsm->pImg_TSM_small, 30);
  cvMorphologyEx( tsm->pImg_TSM_small, tsm->pImg_TSM_small, 0, 0, CV_MOP_OPEN, 1 );
  cvResize( tsm->pImg_TSM_small,  tsm->pImg_TSM);

  


  //////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  // GrabCut algo. 
  //compose_grabcut_seedmatrix(tsm->pImg_TSM, tsm->pImg_DOFMap, tsm->grabcut_mask, tsm->subdiv, false);

  if(1){
    compose_grabcut_seedmatrix2(tsm->grabcut_mask, tsm->facepos );
    if( (tsm->facepos.width * tsm->facepos.height) < 40 )
      tsm->bbox_prev = cvRect( 60,70, 210, 170 );
    else
      tsm->bbox_prev = cvRect( tsm->facepos.x - tsm->facepos.width*0.5, 
                               tsm->facepos.y, 
                               tsm->facepos.x + tsm->facepos.width*1.5,     
                               240 );

    run_graphcut_iteration( tsm->pImgRGB, tsm->grabcut_mask, &tsm->bbox_prev);
  }
  else{
    //tsm->bbox_now = find_bbox_from_cloudofpoints(tsm->pImg_TSM);
    tsm->bbox_now.x      = tsm->facepos.x;
    tsm->bbox_now.y      = tsm->facepos.y;
    tsm->bbox_now.width  = tsm->facepos.width;
    tsm->bbox_now.height = tsm->facepos.height + (240-tsm->facepos.y-tsm->facepos.height);
  
    if( (tsm->bbox_now.width * tsm->bbox_now.height) < 160 ){
      run_graphcut_iteration( tsm->pImgRGB, tsm->grabcut_mask, &tsm->bbox_prev);
    }
    else{
      run_graphcut_iteration( tsm->pImgRGB, tsm->grabcut_mask, &tsm->bbox_now);
      memcpy( &tsm->bbox_prev, &tsm->bbox_now, sizeof(CvRect));
    }
  }




  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( tsm->display ){
    int outputimage = tsm->debug;
    switch( outputimage ){
    case 1: // output the GRAY difference
      cvCvtColor( tsm->pImgGRAY_diff, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 2: // output the DENSE OPTICAL FLOW OUTPUT
      local_draw_dof_map( tsm->pImgGRAY_flow,tsm->pImgGRAY_copy, 10, 1.5, CV_RGB(0, 255, 0));
      cvCvtColor( tsm->pImgGRAY_copy, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 25:// output the DENSE OPTICAL FLOW OUTPUT, thresholded and filtered morphologically, as a Map
      cvCvtColor(  tsm->pImg_DOFMap, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 26:// output the DENSE OPTICAL FLOW contours recalculated before merging masks
      cvDrawContours( tsm->pImg_DOFMask, tsm->dof_contours, cvScalarAll(255), cvScalarAll(255), 100 );
      cvCvtColor( tsm->pImg_DOFMask, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 3: // output the SPARSE OPTICAL FLOW OUTPUT
      local_draw_pyrlk_vectors( tsm->pImgGRAY_copy, found_vertexes, tsm->cornersA, tsm->corners_found, tsm->corners_error, tsm->cornersB );
      cvCvtColor( tsm->pImgGRAY_copy, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 35:// output the SPARSE OF ouput filtered with the Frame difference
      cvCvtColor( tsm->cornersB_asMat, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 36:// output the SPARSE OF ouput filtered with the Frame difference + DENSE contours
      cvCvtColor( tsm->cornersB_asMat, tsm->pImgRGB, CV_GRAY2BGR );
      cvDrawContours( tsm->pImgRGB, tsm->dof_contours, cvScalarAll(0), cvScalarAll(0), 100 );
      break;
    case 37:// output the SPARSE OF ouput filtered with the Frame difference + distance transformed
      cvCvtColor( tsm->pImg_MergedMask, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 40:// ActiveSnake/Contours on the output of the merged masks
      cvCvtColor( tsm->pImg_FinalMask, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 41:// Final mask applied on the input
      cvAnd( tsm->pImg_FinalMask,  tsm->pImgGRAY_copy,  tsm->pImgGRAY_copy, NULL);
      cvCvtColor( tsm->pImgGRAY_copy, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 50:// Block average
      cvSub( tsm->blockstat_avg, tsm->blockstat_avg_1, tsm->blockstat_avg);
      cvResize( tsm->blockstat_avg,  tsm->pImgGRAY_copy);
      cvCvtColor(  tsm->pImgGRAY_copy, tsm->pImgRGB, CV_GRAY2BGR );
      break;    
    case 51://  TSM
      cvCvtColor(  tsm->pImg_TSM, tsm->pImgRGB, CV_GRAY2BGR );
      break;    
    case 52:// TSM mask applied on the input
      cvAnd( tsm->pImg_TSM,  tsm->pImgGRAY_copy,  tsm->pImgGRAY_copy, NULL);
      cvCvtColor( tsm->pImgGRAY_copy, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 60:// Graphcut
      cvAndS(tsm->grabcut_mask, cvScalar(1), tsm->grabcut_mask, NULL);  // get only FG
      cvConvertScale( tsm->grabcut_mask, tsm->grabcut_mask, 127.0);
      cvCvtColor( tsm->grabcut_mask, tsm->pImgRGB, CV_GRAY2BGR );
      break;
    case 61:// Graphcut applied on input/output image
      cvAndS(tsm->grabcut_mask, cvScalar(1), tsm->grabcut_mask, NULL);  // get only FG and possible FG
      cvConvertScale( tsm->grabcut_mask, tsm->grabcut_mask, 255.0);
      cvAnd( tsm->grabcut_mask,  tsm->pImgGRAY_copy,  tsm->pImgGRAY_copy, NULL);
      cvCvtColor( tsm->pImgGRAY_copy, tsm->pImgRGB, CV_GRAY2BGR );

      cvRectangle(tsm->pImgRGB, cvPoint(tsm->bbox_now.x, tsm->bbox_now.y), 
                  cvPoint(tsm->bbox_now.x + tsm->bbox_now.width, tsm->bbox_now.y+tsm->bbox_now.height),
                  cvScalar(127,0.0), 1, 8, 0 );
      draw_subdiv( tsm->pImgRGB, tsm->subdiv, CV_RGB(0, 180, 0), CV_RGB(0, 0, 180) );
     break;
    default:
      break;
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // copy anyhow the fg/bg to the alpha channel in the output image alpha ch
  cvSplit(tsm->pImgRGB, tsm->pImgCh1, tsm->pImgCh2, tsm->pImgCh3, NULL        );
  cvMerge(              tsm->pImgCh1, tsm->pImgCh2, tsm->pImgCh3, tsm->pImgChA, tsm->pImageRGBA);

  // get a copy of RGB, GRAY and EDGE for the next iteration
  cvCopy(tsm->pImgRGB , tsm->pImgRGB_1 , NULL);
  cvCopy(tsm->pImgGRAY, tsm->pImgGRAY_1, NULL);
  cvCopy(tsm->pImgEDGE, tsm->pImgEDGE_1, NULL);
  
  tsm->numframes++;

  GST_TSM_UNLOCK (tsm);  
  
  return GST_FLOW_OK;
}


gboolean gst_tsm_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "tsm", GST_RANK_NONE, GST_TYPE_TSM);
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



////////////////////////////////////////////////////////////////////////////////
void local_postprocess_dof( CvMat* flowin, IplImage* output, int contour_minsize)
{
  // IDEA: intensity image based on the module of the found displacement;
  // we threshold it to get a map of moved points. Then we find contours 
  // on that, and filter out small ones (contour_minsize)
 #define DOF_THRESHOLD 0.05

  int x, y;
  for( y = 0; y < flowin->cols; y++ ){
    for( x = 0; x < flowin->rows; x++ ){
      // get sample on flowin[x,y], threshold it and write output[x,y]
      CvPoint2D32f fxy = CV_MAT_ELEM(*flowin, CvPoint2D32f, x, y);
      cvSet2D( output, x, y, 
               ((abs(fxy.x) + abs(fxy.y)) > DOF_THRESHOLD) ? cvScalar(255) : cvScalar(0) );
    }
  }
  
  bool flag_dont_use_convex_hull = false; // true => polygonal approx

  static CvMemStorage*   mem_storage = NULL;
  static CvSeq*          contours    = NULL;
  if( mem_storage==NULL ) {
    mem_storage = cvCreateMemStorage(0);
  } else {
    cvClearMemStorage(mem_storage);
  }

  CvContourScanner scanner = cvStartFindContours( output, mem_storage, sizeof(CvContour),
                                                  CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE );
  CvSeq* c;
  int numCont = 0;
  while( (c = cvFindNextContour( scanner )) != NULL ) {
    double len = cvContourArea(c);
    if( len < contour_minsize ) {
       cvSubstituteContour( scanner, NULL );
    } else {
      CvSeq* c_new;
      if( flag_dont_use_convex_hull ) {   // Polygonal approximation
        c_new = cvApproxPoly( c,  sizeof(CvContour), mem_storage, CV_POLY_APPROX_DP, 1, 0 );
      } 
      else{                              // Convex Hull of the segmentation
        c_new = cvConvexHull2( c, mem_storage, CV_CLOCKWISE, 1 );
      }
      cvSubstituteContour( scanner, c_new );
      numCont++;
    }
  }
  contours = cvEndFindContours( &scanner );

  cvDrawContours( output, contours, cvScalarAll(255), cvScalarAll(255), 100 );

}



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/// \function calculate_pyrlk
/// \param[in]  inputimage: single channel image (IplImage), is the frame t-1
/// \param[in]  inputimage2: single channel image (IplImage), is the frame in t
/// \param[in]  num_vertexes: amount of features to track
/// \param[out] vertexesA: emtpy array of CvPoint2D32f that will be filled in with the vertexes' coordinates
/// \param[out] vertexes_found: char array of num_vertexes length, output: 1=found, 0= not found
/// \param[out] vertexes_error: float array of num_vertexes length, with a number describing the tracking confidence
/// \param[out] vertexesB: emtpy array of CvPoint2D32f that will be filled in with the propagated vertexes' coordinates
///
int local_calculate_pyrlk(IplImage *inputimage, IplImage *inputimage2, int num_vertexes, 
                          CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error, CvPoint2D32f* vertexesB)
{

  CvSize img_sz = cvGetSize( inputimage );
  int win_size = 15; // I had 15; Mihai uses 3x3, output is much noisier!
  // Get the features for tracking
  static IplImage* eig_image = cvCreateImage( img_sz, IPL_DEPTH_32F, 1 );
  static IplImage* tmp_image = cvCreateImage( img_sz, IPL_DEPTH_32F, 1 );
  int corner_count = num_vertexes;

  // good features to track are taken from the Grey image in t (not in t+1 )
  cvGoodFeaturesToTrack( inputimage, eig_image, tmp_image, vertexesA, &corner_count,
                         0.05,   // Multiplier for the maxmin eigenvalue; minimal accepted q of image corners.
                         0.1,    // Limit, specifying minimum possible distance between returned corners; (GoodFeaturesQuality)
                         0,      // Region of interest.  (GoodFeaturesMinDist)
                         3,      // Size of the averaging block (GoodFeaturesBlockSize)
                         0,      // If nonzero, Harris operator 
                         0.04 ); // Free parameter of Harris detector; used only if use_harris≠0
#if 0
  cvFindCornerSubPix( inputimage, vertexesA, corner_count, cvSize( win_size, win_size ),
                      cvSize( -1, -1 ), 
                      cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.3  ) );
#endif

  CvSize pyr_sz = cvSize( inputimage->width+8, inputimage->height/3 );

  static IplImage* pyrA = cvCreateImage( pyr_sz, IPL_DEPTH_32F, 1 );
  static IplImage* pyrB = cvCreateImage( pyr_sz, IPL_DEPTH_32F, 1 );

  // Call Lucas Kanade algorithm
  cvCalcOpticalFlowPyrLK( inputimage, inputimage2, pyrA, pyrB, vertexesA, vertexesB, corner_count, 
                          cvSize( win_size, win_size ), 
                          3, // used to be 5, Mihai had 3. 
                          vertexes_found, vertexes_error,
                          cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 40, 0.3 ), 0 );

  return(corner_count);
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void create_image_from_flow(IplImage *image, CvMat* flow )
{
  cvZero( image);
  // Make an image of the results
  for( int row=0; row<image->height; row++){    // rows
    for( int col=0; col<image->width; col++){   // columns
      CvPoint2D32f fxy = CV_MAT_ELEM(*flow, CvPoint2D32f, row, col);
      CvPoint p0 = cvPoint( col, row  );
      int value = (int) sqrt(fxy.x*fxy.x + fxy.y*fxy.y );
      cvLine( image, p0, p0, CV_RGB( value, value, value), 1, 8, 0 );
    }
  }

}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void create_image_from_features(IplImage *image, int num_vertexes, 
                                CvPoint2D32f* vertexes, char *vertexes_found, float *vertexes_error )
{
  cvZero( image);
  // Make an image of the results
  for( int i=0; i < num_vertexes; i++ ){
    if( vertexes_found[i]==1 && vertexes_error[i]<550){
      CvPoint p0 = cvPoint( cvRound( vertexes[i].x ), cvRound( vertexes[i].y ) );
      cvLine( image, p0, p0, CV_RGB(255,255,255), 1, 8, 0 );
    }
  }

}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void create_labels_dictionary(CvMat* dictionary, IplImage* image, IplImage* labels)
{
  // we need to know the label associated to each pixel in the "image", to compose
  // a "dictionary". For this, traverse the image and read the label associated 
  // to every pixel whose value != 255 (Image is mostly white, a "negative")
  cvZero( dictionary );
  for( int row=0; row<image->height; row++){    // rows
    for( int col=0; col<image->width; col++){   // columns
      if( cvGetReal2D( image, row, col ) < 255 ){ // cornersB_asMat is white with black dots!
        // label for that very pixel location
        int label = (int)cvGetReal2D( labels, row, col );
        cvSetReal2D( dictionary,   label, 0,   row);
        cvSetReal2D( dictionary,   label, 1,   col);
      }
    }
  }
}




//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void local_calculate_block_statistics(IplImage *image, int blocksize, CvMat* blockstat_avg, CvMat* blockstat_dev)
{
  cvZero( blockstat_avg );
  cvZero( blockstat_dev );
  CvRect roi;
  CvSize size = cvGetSize(image);
  CvScalar average, deviation;

  for (int r = 0; r < size.height; r += blocksize) // row
    for (int c = 0; c < size.width; c += blocksize) { // column
      roi.x = c;
      roi.y = r;
      roi.width = (c + blocksize > size.width) ? (size.width - c) : blocksize;
      roi.height = (r + blocksize > size.height) ? (size.height - r) : blocksize;
      
      cvSetImageROI(image, roi);
      
      // do the processing on the ROI only
      cvAvgSdv( image, &average, &deviation, NULL);

      cvSetReal2D( blockstat_avg, r/blocksize, c/blocksize, average.val[0]);
      cvSetReal2D( blockstat_dev, r/blocksize, c/blocksize, deviation.val[0]);

    }
  cvResetImageROI(image);

}



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void local_integrate_motion_priors(IplImage *motion, int blocksize, 
                                   CvMat* block_avg,   CvMat* block_dev,
                                   CvMat* block_avg_1, CvMat* block_dev_1,
                                   CvMat *tsm, CvMat* pfg, double threshold )
{
#define BACKGROUND   0
#define FOREGROUND 255

  CvRect roi;
  CvSize size = cvGetSize(motion);
  int rb, cb;  
  CvScalar vec_avg, vec_dev;

  for (int r = 0; r < size.height; r += blocksize) // row
    for (int c = 0; c < size.width; c += blocksize) { // column
      roi.x = c;
      roi.y = r;
      roi.width = (c + blocksize > size.width) ? (size.width - c) : blocksize;
      roi.height = (r + blocksize > size.height) ? (size.height - r) : blocksize;
      
      cvSetImageROI(motion, roi);
      
      rb = r/blocksize;
      cb = c/blocksize;  // block-jumping versions of r and c

      cvAvgSdv( motion, &vec_avg, &vec_dev, NULL); // calculate avg motion flow


      // IDEA: if there is no movement, dont change the TSM. If there is, then
      //  if there is a lot of movement, mark FG, otherwise
      //   if there has been a change of statistics (hinting FG->BG or BG->FG change), since
      //    there would be no movement, assume is FG->BG, and mark BG, otherwise
      //   then keep it as FG just in case ;)
      if( cvCountNonZero(motion) ){
        if( cvCountNonZero(motion) > (blocksize*blocksize/3) ){ 
          cvSetReal2D( tsm, rb, cb, FOREGROUND);
        }
        else{ // not enough motion to make a clear decision: check statistics
          // time_comparison >> threshold, means DIFFERENT, and << threshold, SAME
          double time_comparison = distance_between_distros( cvGetReal2D(block_avg, rb,cb)  , cvGetReal2D(block_dev, rb,cb),
                                                             cvGetReal2D(block_avg_1, rb,cb), cvGetReal2D(block_dev_1, rb,cb));
          //printf(" --> %f\n", time_comparison);
          //cvSetReal2D( tsm, rb, cb, time_comparison*255);
          if( time_comparison < threshold )  // (Not enough motion and) very different from before: background
            cvSetReal2D( tsm, rb, cb, BACKGROUND);
          else
            cvSetReal2D( tsm, rb, cb, FOREGROUND);
        }
      }

//      if( cvCountNonZero(motion) ){
//        // do the processing on the ROI only: Check if there are motion q's in the block
//        // If there are movements: inmediately mark them as FG
//        if( cvCountNonZero(motion) > (blocksize*blocksize/3) ){ 
//          cvSetReal2D( tsm, rb, cb, FOREGROUND);  
//        }
//        else{
//          // see if the prev frame and current frame are dissimilar, indicating 
//          // a change which no motion acounts for: movement "trail"
//          // OR if the movement cues are leaving the current block
//          if( (time_comparison >= threshold) || (abs(vec_avg.val[0]) > 2) || 
//              (abs(cvGetReal2D(block_dev, rb,cb) - cvGetReal2D(block_dev_1, rb,cb))<0.01) ){
//            cvSetReal2D( tsm, rb, cb, BACKGROUND);  
//          }
//        }
//      }// if there is motion at all
    }

  cvResetImageROI(motion);

}















////////////////////////////////////////////////////////////////////////////////
void local_draw_dof_map(const CvMat* flow, IplImage* cflowmap, int step,
                    double scale, CvScalar color)
{
  int x, y;
  for( y = 0; y < cflowmap->height; y += step)
    for( x = 0; x < cflowmap->width; x += step)
    {
      CvPoint2D32f fxy = CV_MAT_ELEM(*flow, CvPoint2D32f, y, x);
      cvLine(cflowmap, cvPoint(x,y), cvPoint(cvRound(x+fxy.x), cvRound(y+fxy.y)),
             color, 1, 8, 0);
      cvCircle(cflowmap, cvPoint(x,y), 2, color, -1, 8, 0);
    }
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void local_draw_pyrlk_vectors(IplImage *image, int num_vertexes, 
                              CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error,  
                              CvPoint2D32f* vertexesB  )
{
  // Make an image of the results
  for( int i=0; i < num_vertexes; i++ ){
    if( vertexes_found[i]==0 || vertexes_error[i]>550){
      CvPoint p0 = cvPoint( cvRound( vertexesA[i].x ), cvRound( vertexesA[i].y ) );
      cvLine( image, p0, p0, CV_RGB(0,0,255), 1, 8, 0 );
    }else{
      CvPoint p0 = cvPoint( cvRound( vertexesA[i].x ), cvRound( vertexesA[i].y ) );
      CvPoint p1 = cvPoint( cvRound( vertexesB[i].x ), cvRound( vertexesB[i].y ) );
  
      // These lines below increase the size of the line by 3
      double angle;     angle = atan2( (double) p0.y - p1.y, (double) p0.x - p1.x );
      double modulus;	modulus = sqrt( (p0.y - p1.y)*(p0.y - p1.y) + (p0.x - p1.x)*(p0.x - p1.x) );
      p1.x = (int) (p1.x - 3 * modulus * cos(angle));
      p1.y = (int) (p1.y - 3 * modulus * sin(angle));
      // they can be commented out w/o any problem
      
      if( modulus < 10 && modulus > 0.5){
        cvLine( image, p0, p1, CV_RGB(0,255,0), 1, 8, 0 );      
      }
    }
  }

}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static double distance_between_distros(double mu0, double sig0, double mu1, double sig1)
{
  if( (abs(mu1-mu0) < 2) &&  (abs(sig1-sig0) < 2))
    return(0.0); // no need for special calculations ;)
  if( abs(sig0)<0.01 && abs(sig0)<0.01 ){
    // if both sigmas very low imply basically flat colour areas
    return((mu0==mu1)? 1.0: 0.0);
  }
  double diff = mu1 - mu0;
  double   mS = (sig1*sig1 + sig0*sig0);
  double  inv = 1/mS;
  double BhattacharyyaCoeff = sqrt(2*sig0*sig1*inv) * exp(-0.25*diff*diff*inv);

  // the more similar the distros are, the closer this number to 1
  return( sqrt( 1- BhattacharyyaCoeff) );
}



////////////////////////////////////////////////////////////////////////////////
void prune_image( CvMat* image, double threshold)
{
  static CvMemStorage*   mem_storage = NULL;
  static CvSeq*          contours    = NULL;
  if( mem_storage==NULL ) {
    mem_storage = cvCreateMemStorage(0);
  } else {
    cvClearMemStorage(mem_storage);
  }
  bool flag_dont_use_convex_hull = true; 
  CvContourScanner scanner = cvStartFindContours( image, mem_storage, sizeof(CvContour),
                                                  CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE );
  CvSeq* c;
  int numCont = 0;
  while( (c = cvFindNextContour( scanner )) != NULL ) {
    double len = cvContourArea(c);
    if( len < threshold ) {
       cvSubstituteContour( scanner, NULL );
    } else {
      CvSeq* c_new;
      if( flag_dont_use_convex_hull ) {   // Polygonal approximation
        c_new = cvApproxPoly( c,  sizeof(CvContour), mem_storage, CV_POLY_APPROX_DP, 1, 0 );
      } 
      else{                              // Convex Hull of the segmentation
        c_new = cvConvexHull2( c, mem_storage, CV_CLOCKWISE, 1 );
      }
      cvSubstituteContour( scanner, c_new );
      numCont++;
    }
  }
  contours = cvEndFindContours( &scanner );

  cvDrawContours( image, contours, cvScalarAll(255), cvScalarAll(255), 100, CV_FILLED );

}

// copied, otherwise only available in C++
enum{  
  GC_BGD    = 0,  //!< background
  GC_FGD    = 1,  //!< foreground
  GC_PR_BGD = 2,  //!< most probably background
  GC_PR_FGD = 3   //!< most probably foreground
};   

////////////////////////////////////////////////////////////////////////////////
void compose_grabcut_seedmatrix(IplImage* fg, IplImage* pfg, CvMat* output, CvSubdiv2D* subdiv, bool inverted )
{
  // fg is supposed to be the pyrlk selected features, black on white !!!
  // and pfg (probable fg) is coming from dense OF

  cvSet(output, cvScalar(GC_BGD), NULL);
  //cvClearSubdivVoronoi2D(subdiv);

  for( int row=0; row<fg->height; row++){    // rows
    for( int col=0; col<fg->width; col++){   // columns
      if( !inverted ){
        if( cvGetReal2D( fg, row, col ) > 1 ){ // "fg" is white areas on black canvas
          cvSetReal2D( output, row, col, GC_FGD);
          cvCircle( output, cvPoint(col,row), 4, cvScalar(GC_FGD), 1);
          
          //CvPoint2D32f fp = cvPoint2D32f( col, row);
          //cvSubdivDelaunay2DInsert( subdiv , fp );          
        }
      }
      else // inverted -> // cornersB_asMat is white with black dots!
        if( cvGetReal2D( fg, row, col ) < 255 ){ // "fg" is white with black dots!
          cvSetReal2D( output, row, col, GC_FGD);
          cvCircle( output, cvPoint(col,row), 4, cvScalar(GC_FGD), 1);
          
          //CvPoint2D32f fp = cvPoint2D32f( col, row);
          //cvSubdivDelaunay2DInsert( subdiv , fp );          
        }

      if( cvGetReal2D( pfg, row, col ) > 1 ){ //  is shades of grey over black!
        cvSetReal2D( output, row, col, GC_PR_FGD);
      }

    }
  }
  //cvCalcSubdivVoronoi2D( subdiv );
}

////////////////////////////////////////////////////////////////////////////////
void compose_grabcut_seedmatrix2(CvMat* output, CvRect facebox )
{
  cvSet(output, cvScalar(GC_BGD), NULL);

  double a=0.85;  //extra growing of body box region
  double b=1+a;

  int bodyx0 = ((facebox.x-facebox.width*   a) < 0 ) ? 0 : (facebox.x-facebox.width* a);
  int bodyy0 = ((facebox.y+facebox.height*a  ) > output->rows) ? output->rows : (facebox.y+facebox.height*a);
  int bodyx1 = ((facebox.x+facebox.width*   b) > output->cols) ? output->cols : (facebox.x+facebox.width*b);
  int bodyy1 = (output->rows);

  double c=0.15;  //extra growing of face bbox region
  double d=1+d;

  int facex0 = ((facebox.x-facebox.width  *c) < 0 ) ? 0 : (facebox.x - facebox.width*c);
  int facey0 = ((facebox.y-facebox.height *c) < 0)  ? 0 : (facebox.y - facebox.height*c);
  int facex1 = ((facebox.x+facebox.width*  d) > output->cols) ? output->cols : (facebox.x+facebox.width*d);
  int facey1 = ((facebox.y+facebox.height *d) > output->rows) ? output->rows : (facebox.y+facebox.height*d);

  double e = 0.25;
  double f = 1-e; // !! - and not +
  int corex0 = ((facebox.x+facebox.width  *e) < 0 ) ? 0 : (facebox.x + facebox.width *e);
  int corey0 = ((facebox.y+facebox.height *e) < 0)  ? 0 : (facebox.y + facebox.height*e);
  int corex1 = ((facebox.x+facebox.width*  f) > output->cols) ? output->cols : (facebox.x+facebox.width*f);
  int corey1 = ((facebox.y+facebox.height *f) > output->rows) ? output->rows : (facebox.y+facebox.height*f);
  
  printf(" 1[%d,%d, %d, %d]\n", facebox.x, facebox.y, facebox.width, facebox.height);
  printf(" 2[%d,%d, %d, %d] (%d,%d)\n", bodyx0, bodyy0, bodyx1, bodyy1, output->cols, output->rows);
  printf(" 3[%d,%d, %d, %d]\n", corex0, corey0,  corex1, corey1);

  int x, y;
  for( x = 0; x < output->cols; x++ ){
    for( y = 0; y < output->rows; y++ ){

      // large bbox around face
      if( ( x >= facex0 ) && ( x <= facex1) && ( y >= facey0 ) && ( y <= facey1))
        CV_MAT_ELEM(*output, uchar, y, x) = GC_PR_FGD;
      // small bbox INSIDE face
      if( ( x >= corex0 ) && ( x <= corex1) && ( y >= corey0 ) && ( y <= corey1))
        CV_MAT_ELEM(*output, uchar, y, x) = GC_FGD;
      // body bbox
      if( ( x >= bodyx0)  && ( x <= bodyx1) && ( y >= bodyy0)  && ( y <= bodyy1))
        CV_MAT_ELEM(*output, uchar, y, x) = GC_PR_FGD;

    }
  }

  
}

////////////////////////////////////////////////////////////////////////////////
 CvRect find_bbox_from_cloudofpoints(IplImage* fg)
{
  //int minx = fg->width-1, miny= fg->height-1, maxx=1, maxy=1;
  int minx = 1000, miny= 1000, maxx=1, maxy=1;
  bool found=false;

  for( int row=0; row<fg->height; row++){    // rows
    for( int col=0; col<fg->width; col++){   // columns
      if( cvGetReal2D( fg, row, col ) < 255 ){ // cornersB_asMat is white with black dots!
        maxx = (col > maxx) ?  col  : maxx;
        minx = (col < minx) ?  col  : minx ;

        maxy = (row > maxy) ?  row  : maxy;
        miny = (row < miny) ?  row  : miny;
        found = true;
      }
    }
  }
  if( ( miny == maxy ) || ( minx == maxx ) || !found){
    return( cvRect( 60,60, 210, 180) );  
  }
  return( cvRect( minx, miny, maxx-minx, maxy-miny) );
}



















void draw_subdiv_edge( IplImage* img, CvSubdiv2DEdge edge, CvScalar color )
{
    CvSubdiv2DPoint* org_pt;
    CvSubdiv2DPoint* dst_pt;
    CvPoint2D32f org;
    CvPoint2D32f dst;
    CvPoint iorg, idst;

    org_pt = cvSubdiv2DEdgeOrg(edge);
    dst_pt = cvSubdiv2DEdgeDst(edge);

    if( org_pt && dst_pt )
    {
        org = org_pt->pt;
        dst = dst_pt->pt;

        iorg = cvPoint( cvRound( org.x ), cvRound( org.y ));
        idst = cvPoint( cvRound( dst.x ), cvRound( dst.y ));

        cvLine( img, iorg, idst, color, 1, CV_AA, 0 );
    }
}


void draw_subdiv( IplImage* img, CvSubdiv2D* subdiv,
                  CvScalar delaunay_color, CvScalar voronoi_color )
{
    CvSeqReader  reader;
    int i, total = subdiv->edges->total;
    int elem_size = subdiv->edges->elem_size;

    cvStartReadSeq( (CvSeq*)(subdiv->edges), &reader, 0 );

    for( i = 0; i < total; i++ )
    {
        CvQuadEdge2D* edge = (CvQuadEdge2D*)(reader.ptr);

        if( CV_IS_SET_ELEM( edge ))
        {
          //draw_subdiv_edge( img, (CvSubdiv2DEdge)edge + 1, voronoi_color );
            draw_subdiv_edge( img, (CvSubdiv2DEdge)edge, delaunay_color );
        }

        CV_NEXT_SEQ_ELEM( elem_size, reader );
    }
}



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
static gboolean gst_tsm_sink_event(GstPad *pad, GstEvent * event)
{
  GstTsm *tsm = GST_TSM (gst_pad_get_parent( pad ));
  gboolean ret = FALSE;
  double x,y,w,h;

  switch (GST_EVENT_TYPE(event)) {
  case GST_EVENT_CUSTOM_DOWNSTREAM:
    if (gst_event_has_name(event, "facelocation")) {
      const GstStructure* str = gst_event_get_structure(event);
      gst_structure_get_double(str, "x", &x); // check bool return
      gst_structure_get_double(str, "y", &y); // check bool return
      gst_structure_get_double(str, "width", &w); // check bool return
      gst_structure_get_double(str, "height", &h);// check bool return
      
      w = w*1.5; h=h*1.5;
      tsm->facepos.x = (int)x-(w/2);
      tsm->facepos.y = (int)y-(h/2);
      tsm->facepos.width = (int)w;
      tsm->facepos.height = (int)h;

      gst_event_unref(event);
      ret = TRUE;
    }
    break;
  case GST_EVENT_EOS:
    GST_INFO("Received EOS");
  default:
    ret = gst_pad_event_default(pad, event);
  }
  
  gst_object_unref(tsm);
  return ret;
}
