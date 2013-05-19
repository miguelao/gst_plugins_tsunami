/* GStreamer
 */
/**
 * SECTION:element- codebookfgbg
 *
 * This element creates and updates a fg/bg model using a codebook approach and
 * following the opencv O'Reilly book implementation of the algo described in
 * K. Kim, T. H. Chalidabhongse, D. Harwood and L. Davis, 
 * "Real-time Foreground-Background Segmentation using Codebook Model", 
 * Real-time Imaging, Volume 11, Issue 3, Pages 167-256, June 2005.
 *
 * Previous to that, there is a posterize effect based on mean-shift segmentation
 * and posterior to that, a lot of effects enabled via "experimental" flag.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstcodebookfgbg.h"

#include "highgui.h"

GST_DEBUG_CATEGORY_STATIC (gst_codebookfgbg_debug);
#define GST_CAT_DEFAULT gst_codebookfgbg_debug


enum {
	PROP_0,
	PROP_DISPLAY,
	PROP_POSTERIZE,
	PROP_EXPERIMENTAL,
	PROP_NORMALIZE,
	PROP_LAST
};

static GstStaticPadTemplate gst_codebookfgbg_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);
static GstStaticPadTemplate gst_codebookfgbg_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
);

#define GST_CODEBOOKFGBG_LOCK(codebookfgbg) G_STMT_START { \
	GST_LOG_OBJECT (codebookfgbg, "Locking codebookfgbg from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&codebookfgbg->lock); \
	GST_LOG_OBJECT (codebookfgbg, "Locked codebookfgbg from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_CODEBOOKFGBG_UNLOCK(codebookfgbg) G_STMT_START { \
	GST_LOG_OBJECT (codebookfgbg, "Unlocking codebookfgbg from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&codebookfgbg->lock); \
} G_STMT_END

static gboolean gst_codebookfgbg_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static gboolean gst_codebookfgbg_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_codebookfgbg_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_codebookfgbg_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_codebookfgbg_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_codebookfgbg_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_codebookfgbg_finalize(GObject * object);

static           int update_codebook( unsigned char* p, codeBook* c, unsigned* cbBounds, int numChannels );
static           int clear_stale_entries(codeBook *c);
static unsigned char background_diff( unsigned char* p, codeBook* c, int numChannels,
                                      int* minMod, int* maxMod  );

static void  posterize_image(IplImage* img);

static void moulay_y_prenorm(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean);
static void moulay_y_postnorm(IplImage* img) ;

#ifdef MORPHOLOGICAL_FILTER
static void  morphological_filter(IplImage* frame);
#endif
#ifdef COLOURBINS
static void  classify_colours_as_fg(IplImage *img, IplImage *alpha, CvMatND *fg);
static void grow_regions_by_colour(IplImage *img, IplImage *alpha, CvMatND *fg);
#endif
#ifdef GRAYBINS   
static void  classify_gray_as_fg(IplImage *img, IplImage *alpha, CvMatND *fg);
static void grow_regions_by_gray(IplImage *img, IplImage *alpha, CvMatND *fg);
#endif
#ifdef CONNCOMPONENTS
static          void find_connected_components( IplImage* mask, int poly1_hull0, float perimScale,
                                                int* num, CvRect* bbs, CvPoint* centers );
static void find_pseudo_snake( int snakepoints, IplImage *mask, IplImage *output, CvMemStorage* storage);
#endif

GST_BOILERPLATE (GstCodebookfgbg, gst_codebookfgbg, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanCodebookfgbg(GstCodebookfgbg *codebookfgbg) 
{
  if (codebookfgbg->pFrame)        cvReleaseImageHeader(&codebookfgbg->pFrame);
  if (codebookfgbg->pCodeBookData) cvReleaseImage(&codebookfgbg->pCodeBookData);
  if (codebookfgbg->pFrImg)        cvReleaseImage(&codebookfgbg->pFrImg);
}

static void gst_codebookfgbg_base_init(gpointer g_class) 
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Image Segmentation filter - Simple Codebook FG/BG model", "Filter/Effect/Video",
    "Image segmentation filter -- Simple Codebook FG/BG model",
    "Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_codebookfgbg_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_codebookfgbg_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_codebookfgbg_debug, "codebookfgbg", 0, \
                           "codebookfgbg - Performs image segmentation using a Codebook FG/BG model");
}

static void gst_codebookfgbg_class_init(GstCodebookfgbgClass * klass) 
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_codebookfgbg_set_property;
  gobject_class->get_property = gst_codebookfgbg_get_property;
  gobject_class->finalize = gst_codebookfgbg_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_codebookfgbg_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_codebookfgbg_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_codebookfgbg_get_unit_size);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_codebookfgbg_set_caps);

  g_object_class_install_property(gobject_class, 
                                  PROP_DISPLAY, g_param_spec_boolean(
                                  "display", "Display",
                                  "if set, the output would be the actual bg/fg model", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, 
                                  PROP_POSTERIZE, g_param_spec_boolean(
                                  "posterize", "posterize",
                                  "If set (by def.), pre-posterize the image, to work with colour classes, highly recommendable", TRUE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property(gobject_class, 
                                  PROP_NORMALIZE, g_param_spec_boolean(
                                  "normalize", "normalize",
                                  "If set (by def. not), normalize image lightning, hence avoiding fg/bg confusion due to scene illumination changes", FALSE, 
                                  (GParamFlags)(G_PARAM_READWRITE)));
  g_object_class_install_property(gobject_class, 
                                  PROP_EXPERIMENTAL, g_param_spec_boolean(
                                  "experimental", "experimental",
                                  "If set, attemp some EXPERIMENTAL feature, expected to work poorly", FALSE, 
                                  (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gst_codebookfgbg_init(GstCodebookfgbg * codebookfgbg, GstCodebookfgbgClass * klass) 
{
  gst_base_transform_set_in_place((GstBaseTransform *)codebookfgbg, TRUE);
  g_static_mutex_init(&codebookfgbg->lock);
  codebookfgbg->pFrame        = NULL;
  codebookfgbg->pFrame2       = NULL;
  codebookfgbg->pCodeBookData = NULL;
  codebookfgbg->pFrImg        = NULL;
  codebookfgbg->TcodeBook     = NULL;
  codebookfgbg->nFrmNum       = 0;

  codebookfgbg->ch1           = NULL;
  codebookfgbg->ch2           = NULL;
  codebookfgbg->ch3           = NULL;

  codebookfgbg->display       = false;
  codebookfgbg->posterize     = true;
  codebookfgbg->experimental  = false;
  codebookfgbg->normalize     = false;
}

static void gst_codebookfgbg_finalize(GObject * object) 
{
  GstCodebookfgbg *codebookfgbg = GST_CODEBOOKFGBG (object);
  
  GST_CODEBOOKFGBG_LOCK (codebookfgbg);
  CleanCodebookfgbg(codebookfgbg);
  GST_CODEBOOKFGBG_UNLOCK (codebookfgbg);
  GST_INFO("Codebookfgbg destroyed (%s).", GST_OBJECT_NAME(object));
  
  
  g_static_mutex_free(&codebookfgbg->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_codebookfgbg_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec) 
{
  GstCodebookfgbg *codebookfgbg = GST_CODEBOOKFGBG (object);
  
  GST_CODEBOOKFGBG_LOCK (codebookfgbg);
  switch (prop_id) {
    
  case PROP_DISPLAY:
    codebookfgbg->display = g_value_get_boolean(value);
    break;
  case PROP_POSTERIZE:
    codebookfgbg->posterize = g_value_get_boolean(value);
    break;
  case PROP_EXPERIMENTAL:
    codebookfgbg->experimental = g_value_get_boolean(value);
    break;
  case PROP_NORMALIZE:
    codebookfgbg->normalize = g_value_get_boolean(value);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_CODEBOOKFGBG_UNLOCK (codebookfgbg);
}

static void gst_codebookfgbg_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec) 
{
  GstCodebookfgbg *codebookfgbg = GST_CODEBOOKFGBG (object);

  switch (prop_id) { 
  case PROP_DISPLAY:
    g_value_set_boolean(value, codebookfgbg->display);
    break;
  case PROP_POSTERIZE:
    g_value_set_boolean(value, codebookfgbg->posterize);
    break;
  case PROP_EXPERIMENTAL:
    g_value_set_boolean(value, codebookfgbg->experimental);
    break;
  case PROP_NORMALIZE:
    g_value_set_boolean(value, codebookfgbg->normalize);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_codebookfgbg_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) 
{
  GstVideoFormat format;
  gint width, height;
  
  if (!gst_video_format_parse_caps(caps, &format, &width, &height))
    return FALSE;
  
  *size = gst_video_format_get_size(format, width, height);
  
  GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);
  
  return TRUE;
}


static gboolean gst_codebookfgbg_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps) 
{
  GstCodebookfgbg *codebookfgbg = GST_CODEBOOKFGBG (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_CODEBOOKFGBG_LOCK (codebookfgbg);
  
  gst_video_format_parse_caps(incaps, &codebookfgbg->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &codebookfgbg->out_format, &out_width, &out_height);
  if (!(codebookfgbg->in_format == codebookfgbg->out_format) || 
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_CODEBOOKFGBG_UNLOCK (codebookfgbg);
    return FALSE;
  }
  
  codebookfgbg->width  = in_width;
  codebookfgbg->height = in_height;
  
  GST_INFO("Initialising Codebookfgbg...");

  const CvSize size = cvSize(codebookfgbg->width, codebookfgbg->height);
  GST_WARNING (" width %d, height %d", codebookfgbg->width, codebookfgbg->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in all spaces///////////////////////////////////////
  codebookfgbg->pFrame    = cvCreateImageHeader(size, IPL_DEPTH_8U, 4);

  codebookfgbg->pFrame2       = cvCreateImage(size, IPL_DEPTH_8U, 3);
  codebookfgbg->pCodeBookData = cvCreateImage(size, IPL_DEPTH_8U, 3);

  codebookfgbg->pFrameYUV = cvCreateImage(size, IPL_DEPTH_8U, 3);
  codebookfgbg->pFrameY   = cvCreateImage(size, IPL_DEPTH_8U, 1);
  codebookfgbg->pFrameU   = cvCreateImage(size, IPL_DEPTH_8U, 1);
  codebookfgbg->pFrameV   = cvCreateImage(size, IPL_DEPTH_8U, 1);

  codebookfgbg->pFrImg    = cvCreateImage(size, IPL_DEPTH_8U, 1);
  cvZero(codebookfgbg->pFrImg);

  codebookfgbg->ch1       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  codebookfgbg->ch2       = cvCreateImage(size, IPL_DEPTH_8U, 1);
  codebookfgbg->ch3       = cvCreateImage(size, IPL_DEPTH_8U, 1);

  codebookfgbg->TcodeBook = new codeBook[ codebookfgbg->width * codebookfgbg->height + 1 ];
  for(int j = 0; j < codebookfgbg->width * codebookfgbg->height; j++){
    codebookfgbg->TcodeBook[j].numEntries = 0;
    codebookfgbg->TcodeBook[j].t = 0;
  }

#ifdef CONNCOMPONENTS
  codebookfgbg->pFrameScratch  = cvCreateImage(size, IPL_DEPTH_8U, 1);
  codebookfgbg->storage        = cvCreateMemStorage(0);
#endif
#ifdef COLOURBINS
  int dimensions[3]; dimensions[0]=dimensions[1]=dimensions[2]=256;
  codebookfgbg->fg3d = cvCreateMatND(3, dimensions, CV_32SC2);
  cvZero( codebookfgbg->fg3d);
  codebookfgbg->pFrImg2  = cvCreateImage(size, IPL_DEPTH_8U, 1);
#endif
#ifdef GRAYBINS   
  int dimensions1[1]; dimensions[0]=256;
  codebookfgbg->fg1d = cvCreateMatND(1, dimensions, CV_32SC2);
  cvZero( codebookfgbg->fg1d);
  codebookfgbg->pFrameBW  = cvCreateImage(size, IPL_DEPTH_8U, 1);
#endif

  //////////////////////////////////////////////////////////////////////////////


  GST_WARNING("Codebookfgbg initialized.");
  
  GST_CODEBOOKFGBG_UNLOCK (codebookfgbg);
  
  return TRUE;
}

static void gst_codebookfgbg_before_transform(GstBaseTransform * btrans, GstBuffer * buf) 
{
}

static GstFlowReturn gst_codebookfgbg_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf) 
{
  GstCodebookfgbg *codebookfgbg = GST_CODEBOOKFGBG (btrans);
  unsigned cbBounds[3] = {10,5,5};
  int minMod[3]={20,20,20}, maxMod[3]={20,20,20};

  int j;
  GST_CODEBOOKFGBG_LOCK (codebookfgbg);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  codebookfgbg->nFrmNum++;

  // get image data from the input, which is RGBA
  codebookfgbg->pFrame->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  cvCvtColor(codebookfgbg->pFrame,  codebookfgbg->pFrame2, CV_RGBA2RGB);

  //////////////////////////////////////////////////////////////////////////////
  if( codebookfgbg->posterize ){
    posterize_image( codebookfgbg->pFrame2 );
  }

  //////////////////////////////////////////////////////////////////////////////
  if( codebookfgbg->normalize ){
    cvCvtColor(codebookfgbg->pFrame2,  codebookfgbg->pFrameYUV, CV_RGB2YCrCb);
    cvSplit(codebookfgbg->pFrameYUV, 
            codebookfgbg->pFrameY, codebookfgbg->pFrameU, codebookfgbg->pFrameV, NULL);

    moulay_y_prenorm((uint8_t*)codebookfgbg->pFrameY->imageData, 
                     codebookfgbg->pFrameYUV->width, 
                     codebookfgbg->pFrameYUV->height, 1, 85);
    moulay_y_postnorm(codebookfgbg->pFrameY);

    cvMerge(codebookfgbg->pFrameY, codebookfgbg->pFrameU, codebookfgbg->pFrameV, NULL, 
            codebookfgbg->pFrameYUV);
    cvCvtColor(codebookfgbg->pFrameYUV,  codebookfgbg->pFrame2, CV_YCrCb2RGB);

    cvCopy( codebookfgbg->pFrameYUV, codebookfgbg->pCodeBookData, NULL);
  }
  else{
    cvCvtColor(codebookfgbg->pFrame2, codebookfgbg->pCodeBookData, CV_RGB2HSV);
  }
  //////////////////////////////////////////////////////////////////////////////

  
  //////////////////////////////////////////////////////////////////////////////
  if( codebookfgbg->nFrmNum < 30 ){

    for(j = 0; j < codebookfgbg->width*codebookfgbg->height; j++)
      update_codebook((unsigned char*)codebookfgbg->pCodeBookData->imageData + j*3,
                      (codeBook*)&(codebookfgbg->TcodeBook[j]), cbBounds, 3);
  }
  else{
    if( codebookfgbg->nFrmNum < 31 ){
      for(j = 0; j < codebookfgbg->width*codebookfgbg->height; j++)
        update_codebook((unsigned char*)codebookfgbg->pCodeBookData->imageData + j*3,
                        (codeBook*)&(codebookfgbg->TcodeBook[j]), cbBounds, 3);
      for(j = 0; j < codebookfgbg->width*codebookfgbg->height; j++)
        clear_stale_entries( (codeBook*)&(codebookfgbg->TcodeBook[j]) );

    }
    // this updating is responsible for FG becoming BG again
    if(codebookfgbg->nFrmNum % 120 == 0){
      for(j = 0; j < codebookfgbg->width*codebookfgbg->height; j++) {
        update_codebook((uchar*)codebookfgbg->pCodeBookData->imageData+j*3,
                        (codeBook*)&(codebookfgbg->TcodeBook[j]), cbBounds, 3);
      }
    }
    if(codebookfgbg->nFrmNum%60 == 0){
      for(j = 0; j < codebookfgbg->width*codebookfgbg->height; j++)
        clear_stale_entries( (codeBook*)&(codebookfgbg->TcodeBook[j]) );
    }

    for(j = 0; j < codebookfgbg->width*codebookfgbg->height; j++){
      if(background_diff((uchar*)codebookfgbg->pCodeBookData->imageData+j*3,
                         (codeBook*)&(codebookfgbg->TcodeBook[j]), 3, minMod, maxMod)) {
        codebookfgbg->pFrImg->imageData[j] = 255;
      }
      else {
        codebookfgbg->pFrImg->imageData[j] = 0;
      }
    }
  }

  
  //////////////////////////////////////////////////////////////////////////////
  // Give the user a gun and let him/herself shoot right on the foot...

  if( codebookfgbg->experimental ){
#ifdef CONNCOMPONENTS
    // 45 is the smallest area to show: (w+h)/45 , in pixels
    find_connected_components( codebookfgbg->pFrImg, 0, 45, NULL, NULL, NULL );

    // check out the morphological gradient: this paints the external contour of the blobs
    //cvMorphologyEx( codebookfgbg->pFrImg, codebookfgbg->pFrImg, NULL, NULL, CV_MOP_GRADIENT, 1);

    find_pseudo_snake( 40, codebookfgbg->pFrImg, codebookfgbg->pFrameScratch, 
      codebookfgbg->storage);
    cvErode(codebookfgbg->pFrImg, codebookfgbg->pFrImg);
    cvDilate(codebookfgbg->pFrImg, codebookfgbg->pFrImg);
    
#endif
  }
  //////////////////////////////////////////////////////////////////////////////

  cvErode( codebookfgbg->pFrImg, codebookfgbg->pFrImg, 
           cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT,NULL), 1);
  cvErode( codebookfgbg->pFrImg, codebookfgbg->pFrImg, 
           cvCreateStructuringElementEx(3, 3, 1, 1, CV_SHAPE_RECT,NULL), 1);

  //////////////////////////////////////////////////////////////////////////////
  // if we want to display, just overwrite the output
  if( codebookfgbg->display ){
    cvCvtColor( codebookfgbg->pFrImg, codebookfgbg->pFrame2, CV_GRAY2RGB );
  }

  //////////////////////////////////////////////////////////////////////////////
  // copy anyhow the fg/bg to the alpha channel in the output image
  cvSplit(codebookfgbg->pFrame, 
           codebookfgbg->ch1, codebookfgbg->ch2, codebookfgbg->ch3, NULL);
   cvMerge(codebookfgbg->ch1, codebookfgbg->ch2, codebookfgbg->ch3, codebookfgbg->pFrImg, 
          codebookfgbg->pFrame);
  
  //////////////////////////////////////////////////////////////////////////////
  GST_CODEBOOKFGBG_UNLOCK (codebookfgbg);
  
  return GST_FLOW_OK;
}


gboolean gst_codebookfgbg_plugin_init(GstPlugin * plugin) 
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "codebookfgbg", GST_RANK_NONE, GST_TYPE_CODEBOOKFGBG);
}












//////////////////////////////////////////////////////////////
// int update_codebook(uchar *p, codeBook &c, unsigned cbBounds)
// Updates the codebook entry with a new data point
//
// p Pointer to a YUV or HSI pixel
// c Codebook for this pixel
// cbBounds Learning bounds for codebook (Rule of thumb: 10)
// numChannels Number of color channels we¡¯re learning
//
// NOTES:
// cvBounds must be of length equal to numChannels
//
// RETURN
// codebook index
//
int update_codebook( unsigned char* p, codeBook* c, unsigned* cbBounds, int numChannels )
{
//c->t+=1;
  unsigned int high[3],low[3];
  int n;
  for(n=0; n<numChannels; n++) {
    high[n] = *(p+n)+*(cbBounds+n);
    if(high[n] > 255) high[n] = 255;
    low[n] = *(p+n)-*(cbBounds+n);
    if(low[n] < 0) low[n] = 0;
  }

  int matchChannel;
// SEE IF THIS FITS AN EXISTING CODEWORD
//
  int i;
  for(i=0; i<c->numEntries; i++)  {
    matchChannel = 0;
    for(n=0; n<numChannels; n++)    {
      if((c->cb[i]->learnLow[n] <= *(p+n)) &&
//Found an entry for this channel
         (*(p+n) <= c->cb[i]->learnHigh[n])) {
        matchChannel++;
      }
    }
    if(matchChannel == numChannels) {//If an entry was found
      c->cb[i]->t_last_update = c->t;
//adjust this codeword for the first channel
      for(n=0; n<numChannels; n++){
        if(c->cb[i]->max[n] < *(p+n)){
          c->cb[i]->max[n] = *(p+n);
        }
        else if(c->cb[i]->min[n] > *(p+n)) {
          c->cb[i]->min[n] = *(p+n);
        }
      }
      break;
    }
  }
// OVERHEAD TO TRACK POTENTIAL STALE ENTRIES
//
  for(int s=0; s<c->numEntries; s++) {
// Track which codebook entries are going stale:
//
    int negRun = c->t - c->cb[s]->t_last_update;
    if(c->cb[s]->stale < negRun) c->cb[s]->stale = negRun;
  }
// ENTER A NEW CODEWORD IF NEEDED
//
  if(i == c->numEntries) {//if no existing codeword found, make one
    code_element **foo = new code_element* [c->numEntries+1];
    for(int ii=0; ii<c->numEntries; ii++) {
      foo[ii] = c->cb[ii];
    }
    foo[c->numEntries] = new code_element;
    if(c->numEntries) delete [] c->cb;
    c->cb = foo;
    for(n=0; n<numChannels; n++) {
      c->cb[c->numEntries]->learnHigh[n] = high[n];
      c->cb[c->numEntries]->learnLow[n] = low[n];
      c->cb[c->numEntries]->max[n] = *(p+n);
      c->cb[c->numEntries]->min[n] = *(p+n);
    }
    c->cb[c->numEntries]->t_last_update = c->t;
    c->cb[c->numEntries]->stale = 0;
    c->numEntries += 1;
  }
// SLOWLY ADJUST LEARNING BOUNDS
//
  for(n=0; n<numChannels; n++)  {
    if(c->cb[i]->learnHigh[n] < high[n]) c->cb[i]->learnHigh[n] += 1;
    if(c->cb[i]->learnLow[n] > low[n]) c->cb[i]->learnLow[n] -= 1;
  }
  return(i);
}





///////////////////////////////////////////////////////////////////
//int clear_stale_entries(codeBook &c)
// During learning, after you've learned for some period of time,
// periodically call this to clear out stale codebook entries
//
// c Codebook to clean up
//
// Return
// number of entries cleared
//
int clear_stale_entries(codeBook *c)
{  
  int staleThresh = c->t>>1;
  int *keep = new int [c->numEntries];
  int keepCnt = 0;
// SEE WHICH CODEBOOK ENTRIES ARE TOO STALE
//
  for(int i=0; i<c->numEntries; i++){
    if(c->cb[i]->stale > staleThresh)
      keep[i] = 0; //Mark for destruction
    else {
      keep[i] = 1; //Mark to keep
      keepCnt += 1;
    }
  }
// KEEP ONLY THE GOOD
//
  c->t = 0; //Full reset on stale tracking
  code_element **foo = new code_element* [keepCnt];
  int k=0;
  for(int ii=0; ii<c->numEntries; ii++){
    if(keep[ii]) {
      foo[k] = c->cb[ii];
//We have to refresh these entries for next clearStale
      foo[k]->t_last_update = 0;
      k++;
    }
  }
// CLEAN UP
//
  delete [] keep;
  delete [] c->cb;
  c->cb = foo;
  int numCleared = c->numEntries - keepCnt;
  c->numEntries = keepCnt;
  return(numCleared);
}



////////////////////////////////////////////////////////////
// uchar background_diff( uchar *p, codeBook &c,
// int minMod, int maxMod)
// Given a pixel and a codebook, determine if the pixel is
// covered by the codebook
//
// p Pixel pointer (YUV interleaved)
// c Codebook reference
// numChannels Number of channels we are testing
// maxMod Add this (possibly negative) number onto

// max level when determining if new pixel is foreground
// minMod Subract this (possibly negative) number from
// min level when determining if new pixel is foreground
//
// NOTES:
// minMod and maxMod must have length numChannels,
// e.g. 3 channels => minMod[3], maxMod[3]. There is one min and
// one max threshold per channel.
//
// Return
// 0 => background, 255 => foreground
//
unsigned char background_diff(unsigned   char* p, codeBook* c, int numChannels,
                              int* minMod, int* maxMod  )
{
  int matchChannel;
// SEE IF THIS FITS AN EXISTING CODEWORD
//
  int i;
  for(i=0; i<c->numEntries; i++) {
    matchChannel = 0;
    for(int n=0; n<numChannels; n++) {
      if((c->cb[i]->min[n] - minMod[n] <= *(p+n)) &&
         (*(p+n) <= c->cb[i]->max[n] + maxMod[n])) {
        matchChannel++; //Found an entry for this channel
      } else {
        break;
      }
    }
    if(matchChannel == numChannels) {
      break; //Found an entry that matched all channels
    }
  }
  if(i >= c->numEntries) return(255);
  return(0);
}






////////////////////////////////////////////////////////////////////////////////
// wrapper around the openCV appropriate function:
//
// "The function implements the filtering stage of meanshift segmentation, that 
// is, the output of the function is the filtered “posterized” image with color
// gradients and fine-grain texture flattened. At every pixel of the input image
// (or down-sized input image, see below) the function executes meanshift 
// iterations, that is, the pixel neighborhood in the joint space-color 
// hyperspace is considered."
//
// [http://stackoverflow.com/questions/4831813/image-segmentation-using-mean-shift-explained]
// "The Mean Shift segmentation is a local homogenization technique that is 
// very useful for damping shading or tonality differences in localized objects. 
// [...] replaces each pixel with the mean of the pixels in a range-r 
// neighborhood and whose value is within a distance d."
//
//
// If you ask me, the k-means is an algorithm to find the "equivalence classes"
// on the finite set of vectors in {0..255}^3 (note that this set is no group
// nor vector space).
// This step is the entry point for k-means clustering. But k-means clustering 
// by colour only, is essentially a vector quantisation of the image :D :D
static void  posterize_image(IplImage* input)
{
  // this line reduces the amount of colours
  cvAndS( input, cvScalar(0xF8, 0xF8, 0xF8), input);
  
  //cvSmooth( input, input, CV_BLUR, 4, 3);

  int colour_radius =  16; // number of colour classes ~= 256/colour_radius
  int pixel_radius  =  3;
  int levels        =  2;
  cvPyrMeanShiftFiltering(input, input, pixel_radius, colour_radius, levels);
  
}

////////////////////////////////////////////////////////////////////////////////
// W=width, H=height, C=number of channels (byte), 
// y_mean: y threshold not to touch the image
////////////////////////////////////////////////////////////////////////////////
void moulay_y_prenorm(uint8_t* data, uint32_t W, uint32_t H, uint32_t C, uint32_t y_mean) 
{
  
  const uint32_t S = W * H;
  
  uint32_t mean_global = 0;
  for (uint32_t k = 0; k < S; ++k)
    mean_global += data[k*C];
  mean_global /= S;
  if (mean_global == 0)
    return;
  
  for (uint32_t k = 0; k < S; ++k) {
    uint8_t& val = data[k*C];
    val = (((y_mean * val) / mean_global) > 255) ? 255 : ((y_mean * val) / mean_global);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Originally an openCL kernel, hence the /256.0 at the beginning and the 
// *255.0 at the end. (Might not be needed?)
////////////////////////////////////////////////////////////////////////////////
void moulay_y_postnorm(IplImage* img) 
{
#define INTERP(x,y,w) ((x)+(1/(w))*((y)-(x)))
  
  float val, hp1, hn1, vp1, vn1, mean, vmax, vmin, mean_mul_max, dmx_mean;

  for( int row=1; row<img->height-1; row++){    // rows
    for( int col=1; col<img->width-1; col++){   // columns

      val = cvGetReal2D( img, row, col)     /256.0;
      hp1 = cvGetReal2D( img, row + 1, col )/256.0;
      hn1 = cvGetReal2D( img, row - 1, col )/256.0;
      vp1 = cvGetReal2D( img, row , col + 1)/256.0;
      vn1 = cvGetReal2D( img, row , col + 1)/256.0;

      // interpolations
      hp1 = INTERP(val, hp1, img->width);
      hn1 = INTERP(val, hn1, img->width);
      vp1 = INTERP(val, vp1, img->height);
      vn1 = INTERP(val, vn1, img->height);

      mean = (2*val + hp1 + hn1 + vp1 + vn1)/6.0f;

      vmax = MAX(hn1, MAX( val, MAX( hp1, MAX( vp1, vn1))));
      vmin = MIN(hn1, MIN( val, MIN( hp1, MIN( vp1, vn1))));

      mean_mul_max = vmax * mean * 64.0f;
      dmx_mean = vmax - vmin;
      //printf(" %f ", 255 * (val + 8.0f * dmx_mean) / mean_mul_max);
      cvSetReal2D( img, row, col,
                   256 * ((val + 8.0f * dmx_mean) / mean_mul_max) );
    }
  }


}


#ifdef CONNCOMPONENTS

///////////////////////////////////////////////////////////////////
// void find_connected_components(IplImage *mask, int poly1_hull0,
//                            float perimScale, int *num,
//                            CvRect *bbs, CvPoint *centers)
// This cleans up the foreground segmentation mask derived from calls
// to backgroundDiff
//
// mask          Is a grayscale (8-bit depth) â€œrawâ€ mask image that
//               will be cleaned up
//
// OPTIONAL PARAMETERS:
// poly1_hull0   If set, approximate connected component by
//                 (DEFAULT) polygon, or else convex hull (0)
// perimScale    Len = image (width+height)/perimScale. If contour
//                 len < this, delete that contour (DEFAULT: 4)
// num           Maximum number of rectangles and/or centers to
//                 return; on return, will contain number filled
//                 (DEFAULT: NULL)
// bbs           Pointer to bounding box rectangle vector of
//                 length num. (DEFAULT SETTING: NULL)
// centers      Pointer to contour centers vector of length
//                 num (DEFAULT: NULL)
//


// For connected components:
// Approx.threshold - the bigger it is, the simpler is the boundary
//
#define CVCONTOUR_APPROX_LEVEL  1

// How many iterations of erosion and/or dilation there should be
//
#define CVCLOSE_ITR  1

void find_connected_components( IplImage *mask, int poly1_hull0, float perimScale,
                                int *num, CvRect *bbs,   CvPoint *centers) 
{
  static CvMemStorage*   mem_storage = NULL;
  static CvSeq*          contours    = NULL;
  //CLEAN UP RAW MASK
  //
  cvMorphologyEx( mask, mask, 0, 0, CV_MOP_OPEN,  CVCLOSE_ITR );
  cvMorphologyEx( mask, mask, 0, 0, CV_MOP_CLOSE, CVCLOSE_ITR );
 //FIND CONTOURS AROUND ONLY BIGGER REGIONS
  //
  if( mem_storage==NULL ) {
    mem_storage = cvCreateMemStorage(0);
  } else {
    cvClearMemStorage(mem_storage);
  }

  CvContourScanner scanner = cvStartFindContours( mask, mem_storage, sizeof(CvContour),
                                                  CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE );

  CvSeq* c;
  int numCont = 0;
  while( (c = cvFindNextContour( scanner )) != NULL ) {
    //double len = cvContourPerimeter( c );
    double len = cvContourArea(c);
    // calculate perimeter len threshold:
    //
    double q = (mask->height + mask->width)/perimScale;
    //Get rid of blob if its perimeter is too small:

    if( len < q ) {
       cvSubstituteContour( scanner, NULL );
    } else {
      //printf(" area: %f\n", len);
      // Smooth its edges if its large enough
      //
      CvSeq* c_new;
      if( poly1_hull0 ) {
        // Polygonal approximation
        //
        c_new = cvApproxPoly(
          c,  sizeof(CvContour), mem_storage, CV_POLY_APPROX_DP, CVCONTOUR_APPROX_LEVEL, 0 );
      } else {
        // Convex Hull of the segmentation
        //
        c_new = cvConvexHull2( c, mem_storage, CV_CLOCKWISE, 1 );
      }
      cvSubstituteContour( scanner, c_new );
      numCont++;
    }
  }
  contours = cvEndFindContours( &scanner );
  // Just some convenience variables
  const CvScalar CVX_WHITE = CV_RGB(0xff,0xff,0xff);
  const CvScalar CVX_BLACK = CV_RGB(0x00,0x00,0x00);

  // PAINT THE FOUND REGIONS BACK INTO THE IMAGE
  //
  cvZero( mask );
  IplImage *maskTemp;
 // CALC CENTER OF MASS AND/OR BOUNDING RECTANGLES
  //
  if(num != NULL) {
    //User wants to collect statistics
    //
    int N = *num, numFilled = 0, i=0;
    CvMoments moments;
    double M00, M01, M10;
    maskTemp = cvCloneImage(mask);
    for(i=0, c=contours; c != NULL; c = c->h_next,i++ ) {
      if(i < N) {
        // Only process up to *num of them
        //
        cvDrawContours(  maskTemp, c,
                         CVX_WHITE, CVX_WHITE, -1,
                         CV_FILLED, 8 );
        // Find the center of each contour
        //
        if(centers != NULL) {
          cvMoments(maskTemp,&moments,1);
          M00 = cvGetSpatialMoment(&moments,0,0);
          M10 = cvGetSpatialMoment(&moments,1,0);
          M01 = cvGetSpatialMoment(&moments,0,1);
          centers[i].x = (int)(M10/M00);
          centers[i].y = (int)(M01/M00);
        }
        //Bounding rectangles around blobs
        //
        if(bbs != NULL) {
          bbs[i] = cvBoundingRect(c);
        }
        cvZero(maskTemp);
        numFilled++;
      }
      // Draw filled contours into mask
      //
      cvDrawContours(
        mask,
        c,
        CVX_WHITE,
        CVX_WHITE,
        -1,
        CV_FILLED,
        8
        );
    }                               //end looping over contours
    *num = numFilled;
    cvReleaseImage( &maskTemp);
  } 
  // ELSE JUST DRAW PROCESSED CONTOURS INTO THE MASK
  //
  else {
    // The user doesn't want statistics, just draw the contours
    //
    for( c=contours; c != NULL; c = c->h_next ) {
      cvDrawContours( mask, c, CVX_WHITE, CVX_BLACK, -1, CV_FILLED, 8 );
  }
}
}


///////////////////////////////////////////////////////////////////
// void find_pseudo_snake( int snakepoints, IplImage *mask, IplImage *scratch) 
//
// This function creates a pseudo snake out of a binary mask (a ghost)
// Another 1-channel image of the same dimensions is needed as scratch
//
// mask          Is a grayscale (8-bit depth) mask image that
//               will be modified
// storage       created somewhere else, just like f.i.
//               CvMemStorage* storage = cvCreateMemStorage(0);

void find_pseudo_snake( int snakepoints, IplImage *mask, IplImage *scratch, CvMemStorage* storage) 
{
  CvScalar alpha, scratchp;
  CvSeq* seq = cvCreateSeq( CV_32SC2, sizeof(CvSeq), sizeof(CvPoint), storage );

  cvSetZero(scratch);

  // walk through every pixel of the image, idea is to get, for each column,
  // the lowest and highest transition from 0 to 1 and viceversa.
  // we start with the transition from above 0 -> 1
  int found;
  for( int x=0; x<mask->width; x++){
    found = 0;
    for( int y=0; y<mask->height; y++ ){

      alpha= cvGet2D( mask, y, x );
      if( (found == 0) && (alpha.val[0]>0) ){
        //we got the transition!
        cvSet2D(scratch, y, x, cvScalar(255));
        CvPoint coords = cvPoint(x,y);
        //cvCircle( scratch, coords, 2, CV_RGB(255,255,255));
        cvSeqPush( seq, &coords);
        break;
      }
    }
  }
  // continue with the transition from below 1 -> 0
  // x also goes backwards: not needed, but creates a better looking Seq 
  for( int x=mask->width-1; x>=0 ; x--){
    found=0;
    for( int y=mask->height-1; y>=0; y-- ){
  
      alpha= cvGet2D( mask, y,x);
      if(found == 0){
        if(alpha.val[0]>0){
          //we got the transition!
          CvPoint coords = cvPoint(x,y);
          cvSet2D(scratch, y, x, cvScalar(255) );
          //cvCircle( scratch, coords, 1, CV_RGB(255,255,255));
          cvSeqPush(seq, &coords);
          found = 1;
          //break;
        }
      } 
      else{ // we passed the transition 0->1
        scratchp = cvGet2D( scratch, y,x);
        //printf(" now the value is %d\n", scratchp.val[0]);
        if(scratchp.val[0] <= 0){
          cvSet2D(scratch, y, x, cvScalar(255) );
          //CvPoint coords = cvPoint(x,y);
          //cvCircle( scratch, coords, 2, CV_RGB(255,255,255));              
        }else
          break;
      } 
    }  // for y
  }// for x


  // PAINT THE FOUND REGIONS BACK INTO THE IMAGE
  cvZero( mask );
  cvCopy( scratch, mask);

}

#endif // CONNCOMPONENTS






// EOF//////////////////////////////////////////////////////////////////////////
