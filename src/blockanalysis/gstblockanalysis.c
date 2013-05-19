/* GStreamer
 */
/**
 * SECTION:element- blockanalysis
 *
 * This element analyses the blockiness of frames passed in the input buffer
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "gstblockanalysis.h"
#include <math.h>

GST_DEBUG_CATEGORY_STATIC (gst_blockanalysis_debug);
#define GST_CAT_DEFAULT gst_blockanalysis_debug

#define N		8


enum {
	PROP_0,
	PROP_LAST
};

#if FACETRK_FORMAT == FACETRK_FORMAT_YUV || FACETRK_FORMAT == FACETRK_FORMAT_YUVA
static GstStaticPadTemplate gst_blockanalysis_src_template = GST_STATIC_PAD_TEMPLATE (
		"src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV("YUV3"))
);
static GstStaticPadTemplate gst_blockanalysis_sink_template = GST_STATIC_PAD_TEMPLATE (
		"sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_YUV("YUV3"))
);
#endif

#define GST_BLOCKANALYSIS_LOCK(blockanalysis) G_STMT_START { \
	GST_LOG_OBJECT (blockanalysis, "Locking blockanalysis from thread %p", g_thread_self ()); \
	g_static_mutex_lock (&blockanalysis->lock); \
	GST_LOG_OBJECT (blockanalysis, "Locked blockanalysis from thread %p", g_thread_self ()); \
} G_STMT_END

#define GST_BLOCKANALYSIS_UNLOCK(blockanalysis) G_STMT_START { \
	GST_LOG_OBJECT (blockanalysis, "Unlocking blockanalysis from thread %p", g_thread_self ()); \
	g_static_mutex_unlock (&blockanalysis->lock); \
} G_STMT_END

static gboolean gst_blockanalysis_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size);
static GstCaps *gst_blockanalysis_transform_caps(GstBaseTransform * btrans, GstPadDirection direction, GstCaps * caps);
static gboolean gst_blockanalysis_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps);
static void gst_blockanalysis_before_transform(GstBaseTransform * btrans, GstBuffer * buf);
static GstFlowReturn gst_blockanalysis_transform_ip(GstBaseTransform * btrans, GstBuffer * buf);

static void gst_blockanalysis_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_blockanalysis_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_blockanalysis_finalize(GObject * object);

GST_BOILERPLATE (GstBlockanalysis, gst_blockanalysis, GstVideoFilter, GST_TYPE_VIDEO_FILTER);

void CleanBlockanalysis(GstBlockanalysis *blockanalysis)
{
  if (blockanalysis->cvYUV)  cvReleaseImageHeader(&blockanalysis->cvYUV);
  if (blockanalysis->cvRGB)  cvReleaseImage(&blockanalysis->cvRGB);
  if (blockanalysis->cvGRAY)  cvReleaseImage(&blockanalysis->cvGRAY);
  if (blockanalysis->cvSobel_x) cvReleaseImage(&blockanalysis->cvSobel_x);
  if (blockanalysis->cvSobel_y) cvReleaseImage(&blockanalysis->cvSobel_y);
  if (blockanalysis->cvSobelSc) cvReleaseImage(&blockanalysis->cvSobelSc);
  if (blockanalysis->cvMatDx) cvReleaseMat(&blockanalysis->cvMatDx);
  if (blockanalysis->cvMatDy) cvReleaseMat(&blockanalysis->cvMatDy);

  //delete blockanalysis->test;
}

static void gst_blockanalysis_base_init(gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  
  gst_element_class_set_details_simple(element_class, "Blockiness analysis filter", "Filter/Effect/Video",
    "Analyse the blockiness of frames",
    "Paul Henrys <Paul.Henrys@alcatel-lucent.com>");
  
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_blockanalysis_sink_template));
  gst_element_class_add_pad_template(element_class, gst_static_pad_template_get(&gst_blockanalysis_src_template));
  
  GST_DEBUG_CATEGORY_INIT (gst_blockanalysis_debug, "blockanalysis", 0, \
                           "blockanalysis - Performs some image adjust operations");
}

static void gst_blockanalysis_class_init(GstBlockanalysisClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *)klass;
  GstBaseTransformClass *btrans_class = (GstBaseTransformClass *)klass;
  
  gobject_class->set_property = gst_blockanalysis_set_property;
  gobject_class->get_property = gst_blockanalysis_get_property;
  gobject_class->finalize = gst_blockanalysis_finalize;
  
  btrans_class->passthrough_on_same_caps = TRUE;
  //btrans_class->always_in_place = TRUE;
  btrans_class->transform_ip = GST_DEBUG_FUNCPTR (gst_blockanalysis_transform_ip);
  btrans_class->before_transform = GST_DEBUG_FUNCPTR (gst_blockanalysis_before_transform);
  btrans_class->get_unit_size = GST_DEBUG_FUNCPTR (gst_blockanalysis_get_unit_size);
  btrans_class->transform_caps = GST_DEBUG_FUNCPTR (gst_blockanalysis_transform_caps);
  btrans_class->set_caps = GST_DEBUG_FUNCPTR (gst_blockanalysis_set_caps);
}

static void gst_blockanalysis_init(GstBlockanalysis * blockanalysis, GstBlockanalysisClass * klass)
{
  gst_base_transform_set_in_place((GstBaseTransform *)blockanalysis, TRUE);
  g_static_mutex_init(&blockanalysis->lock);
  blockanalysis->cvYUV     = NULL;
  blockanalysis->cvRGB     = NULL;
  blockanalysis->cvGRAY    = NULL;
  blockanalysis->cvSobel_x = NULL;
  blockanalysis->cvSobel_y = NULL;
  blockanalysis->cvSobelSc = NULL;
  blockanalysis->cvMatDx   = NULL;
  blockanalysis->cvMatDy   = NULL;
  blockanalysis->res = 0;

  //blockanalysis->test = new bgMotionDetection();
}

static void gst_blockanalysis_finalize(GObject * object)
{
  GstBlockanalysis *blockanalysis = GST_BLOCKANALYSIS (object);
  
  GST_BLOCKANALYSIS_LOCK (blockanalysis);
  CleanBlockanalysis(blockanalysis);
  //cvDestroyWindow( "Test" );
  GST_BLOCKANALYSIS_UNLOCK (blockanalysis);
  GST_INFO("Blockanalysis destroyed (%s).", GST_OBJECT_NAME(object));
  
  g_static_mutex_free(&blockanalysis->lock);
  
  G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_blockanalysis_set_property(GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstBlockanalysis *blockanalysis = GST_BLOCKANALYSIS (object);
  
  GST_BLOCKANALYSIS_LOCK (blockanalysis);
  switch (prop_id) {
    
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
  GST_BLOCKANALYSIS_UNLOCK (blockanalysis);
}

static void gst_blockanalysis_get_property(GObject * object, guint prop_id, GValue * value, GParamSpec * pspec)
{
  //GstBlockanalysis *blockanalysis = GST_BLOCKANALYSIS (object);

  switch (prop_id) { 
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    break;
  }
}

static gboolean gst_blockanalysis_get_unit_size(GstBaseTransform * btrans, GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "unit size = %d for format %d w %d height %d", *size, format, width, height);

	return TRUE;
}

static GstCaps *
gst_blockanalysis_transform_caps(GstBaseTransform * btrans, GstPadDirection direction, GstCaps * caps)
{
  GstBlockanalysis *blockanalysis = GST_BLOCKANALYSIS (btrans);
  GstCaps *ret, *tmp, *tmplt;
  GstStructure *structure;
  gint i;
  
  tmp = gst_caps_new_empty();
  
  GST_BLOCKANALYSIS_LOCK (blockanalysis);
  
  for (i = 0; i < (int)gst_caps_get_size(caps); i++) {
    structure = gst_structure_copy(gst_caps_get_structure(caps, i));
    gst_structure_remove_fields(structure, "format", "endianness", "depth", "bpp", "red_mask", "green_mask", "blue_mask", "alpha_mask",
				"palette_data", "blockanalysis_mask", "color-matrix", "chroma-site", NULL);
    gst_structure_set_name(structure, "video/x-raw-yuv");
    gst_caps_append_structure(tmp, gst_structure_copy(structure));
    gst_structure_free(structure);
  }
  
  if (direction == GST_PAD_SINK) {
    tmplt = gst_static_pad_template_get_caps(&gst_blockanalysis_src_template);
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
  
  GST_BLOCKANALYSIS_UNLOCK (blockanalysis);
  
  return ret;
}

static gboolean gst_blockanalysis_set_caps(GstBaseTransform * btrans, GstCaps * incaps, GstCaps * outcaps)
{
  GstBlockanalysis *blockanalysis = GST_BLOCKANALYSIS (btrans);
  gint in_width, in_height;
  gint out_width, out_height;
  
  GST_BLOCKANALYSIS_LOCK (blockanalysis);
  
  gst_video_format_parse_caps(incaps, &blockanalysis->in_format, &in_width, &in_height);
  gst_video_format_parse_caps(outcaps, &blockanalysis->out_format, &out_width, &out_height);
  if (!(blockanalysis->in_format == blockanalysis->out_format) ||
      !(in_width == out_width && in_height == out_height)) {
    GST_WARNING("Failed to parse caps %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
    GST_BLOCKANALYSIS_UNLOCK (blockanalysis);
    return FALSE;
  }
  
  blockanalysis->width  = in_width;
  blockanalysis->height = in_height;
  
  GST_INFO("Initialising Blockanalysis...");

  const CvSize size = cvSize(blockanalysis->width, blockanalysis->height);
  GST_WARNING (" width %d, height %d", blockanalysis->width, blockanalysis->height);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in YUV3 ////////////////////////////////////////////
  blockanalysis->cvYUV = cvCreateImageHeader(size, IPL_DEPTH_8U, 3);

  //////////////////////////////////////////////////////////////////////////////
  // allocate image structs in BGR or RGB or similar ///////////////////////////
  // this is needed for internal manipulation, so the IplImage needs to be /////
  // fully allocated, as opposed to YUV, which data we take from gstreamer /////
  blockanalysis->cvRGB  = cvCreateImage(size, IPL_DEPTH_8U, 3);
  blockanalysis->cvGRAY = cvCreateImage(size, IPL_DEPTH_8U, 1);
  blockanalysis->cvSobel_x = cvCreateImage(size, IPL_DEPTH_16S, 1);
  blockanalysis->cvSobel_y = cvCreateImage(size, IPL_DEPTH_16S, 1);
  blockanalysis->cvSobelSc = cvCreateImage(size, IPL_DEPTH_8U, 1);
  blockanalysis->cvMatDx = cvCreateMat( N, N, CV_64FC1);
  blockanalysis->cvMatDy = cvCreateMat( N, N, CV_64FC1);

  //cvNamedWindow( "Test", CV_WINDOW_AUTOSIZE );

  GST_INFO("Blockanalysis initialized.");
  
  GST_BLOCKANALYSIS_UNLOCK (blockanalysis);
  
  return TRUE;
}

static void gst_blockanalysis_before_transform(GstBaseTransform * btrans, GstBuffer * buf)
{
}

static GstFlowReturn gst_blockanalysis_transform_ip(GstBaseTransform * btrans, GstBuffer * gstbuf)
{
  GstBlockanalysis *blockanalysis = GST_BLOCKANALYSIS (btrans);

  GST_BLOCKANALYSIS_LOCK (blockanalysis);

  // Testing purpose
  //sleep(1);

  //////////////////////////////////////////////////////////////////////////////
  // Image preprocessing: color space conversion etc
  // get image data from the input, which is YUV
  blockanalysis->cvYUV->imageData = (char*)GST_BUFFER_DATA(gstbuf);
  // usually here we'd pass to RGB which the preferred OpenCV colourspace
  cvCvtColor( blockanalysis->cvYUV, blockanalysis->cvRGB,  CV_YUV2RGB );
  cvCvtColor( blockanalysis->cvRGB, blockanalysis->cvGRAY, CV_RGB2GRAY );
  //blockanalysis->ShmIS.pushImage(blockanalysis->cvRGB, 5001, (char*)"blockanalysis RGB");
  //blockanalysis->ShmIS.pushImage(blockanalysis->cvGRAY, 5003, (char*)"blockanalysis GRAY");

//bool res = blockanalysis->test->bgMotionDetected(blockanalysis->cvYUV);
//if(res)
//        g_print("Background motion detected: %.2f%%\n", blockanalysis->test->getMotionDetectionResult());

#if 0
{
  float diff_avg=0;
  int N_x = (int)(blockanalysis->cvGRAY->width/N);
  int N_y = (int)(blockanalysis->cvGRAY->height/N);
  float s1=0, s1_x=0, s1_y=0, s2=0, max_x=0, max_y=0, max=0;
  int nbsets = 0;
  unsigned int tab[256] = { 0 };		// Init the table to 0
  //TODO: Add a check on diff_tab
  int* diff_tab = (int*)g_try_malloc(N_x*N_y*sizeof(int));
  memset ( diff_tab, -1, sizeof(int)*N_x*N_y );
  float sigma_avg = 0;
  int sigma_num = 0;

  CvMat* Sum = cvCreateMat(blockanalysis->cvGRAY->height+1, blockanalysis->cvGRAY->width+1, CV_32FC1);
  CvMat* Sqsum = cvCreateMat(blockanalysis->cvGRAY->height+1, blockanalysis->cvGRAY->width+1, CV_64FC1);
  CvMat* Sigma1 = cvCreateMat(N_y, N_x, CV_8UC1);
  //CvMat* Sigma2 = cvCreateMat(N_y, N_x, CV_8UC1);
  cv::Mat D( blockanalysis->cvGRAY );
  D.convertTo(D, CV_8UC1);

  cvIntegral(blockanalysis->cvGRAY, Sum, Sqsum);

  cvSobel( blockanalysis->cvGRAY, blockanalysis->cvSobel_x, 1, 0, 3 );
  cvSobel( blockanalysis->cvGRAY, blockanalysis->cvSobel_y, 0, 1, 3 );

  cv::Mat Dx( blockanalysis->cvSobel_x );
  Dx.convertTo(Dx, CV_32F);
  cv::Mat Dy( blockanalysis->cvSobel_y );
  Dy.convertTo(Dy, CV_32F);

  cvConvertScale(blockanalysis->cvSobel_x, blockanalysis->cvSobelSc, 1, 0);
  //blockanalysis->ShmIS.pushImage(blockanalysis->cvSobelSc, 5007, (char*)"blockanalysis cvSobel_x");
  cvConvertScale(blockanalysis->cvSobel_y, blockanalysis->cvSobelSc, 1, 0);
  //blockanalysis->ShmIS.pushImage(blockanalysis->cvSobelSc, 5009, (char*)"blockanalysis cvSobel_y");

  for( int i=0 ; i < N_x*N_y ; i++ ) {
	  float mean, sqmean, sigma;
	  mean = CV_MAT_ELEM( *Sum , float, ((int)(i/N_x))*N+N, (i%N_x)*N+N ) -		\
			 CV_MAT_ELEM( *Sum , float, ((int)(i/N_x))*N+N, (i%N_x)*N ) -		\
			 CV_MAT_ELEM( *Sum , float, ((int)(i/N_x))*N, (i%N_x)*N+N ) +		\
			 CV_MAT_ELEM( *Sum , float, ((int)(i/N_x))*N, (i%N_x)*N );
	  sqmean = CV_MAT_ELEM( *Sqsum , double, ((int)(i/N_x))*N+N, (i%N_x)*N+N ) -	\
			   CV_MAT_ELEM( *Sqsum , double, ((int)(i/N_x))*N+N, (i%N_x)*N ) -		\
			   CV_MAT_ELEM( *Sqsum , double, ((int)(i/N_x))*N, (i%N_x)*N+N ) +		\
			   CV_MAT_ELEM( *Sqsum , double, ((int)(i/N_x))*N, (i%N_x)*N );
	  sigma = sqrt( sqmean/(N*N) - pow(mean/(N*N), 2) );

	  //CV_MAT_ELEM( *Sigma1, unsigned char, (int)(i/N_x), i%N_x) = (unsigned char) sigma;

	  for( int k=0 ; k<N*N ; k++ )
		  tab[D.at<unsigned char>(((int)(i/N_x))*N+k/N, (i%N_x)*N+k%N)]++;

	  if( sigma>10 ) {
		  CV_MAT_ELEM( *Sigma1, unsigned char, (int)(i/N_x), i%N_x) = (unsigned char) sigma;
		  int diff=0;
		  for( int k=0; k<256 ; k++)
			  if(tab[k])
				  diff++;
		  diff_tab[i] = diff;
/*		  if (diff > 20) {
			  sigma_avg += sigma;
			  sigma_num++;
			  CV_MAT_ELEM( *Sigma2, unsigned char, (int)(i/N_x), i%N_x) = (unsigned char) sigma;
		  }
		  else
			  CV_MAT_ELEM( *Sigma2, unsigned char, (int)(i/N_x), i%N_x) = 255;
*/
		  for( int j=0 ; j<N*N ; j++ ) {
			  Dx.at<float>( ((int)(i/N_x))*N+j/N, (i%N_x)*N+j%N ) = 0;
			  Dy.at<float>( ((int)(i/N_x))*N+j/N, (i%N_x)*N+j%N ) = 0;
		  }
	  }
	  else {
		  CV_MAT_ELEM( *Sigma1, unsigned char, (int)(i/N_x), i%N_x) = 255;
		  //CV_MAT_ELEM( *Sigma2, unsigned char, (int)(i/N_x), i%N_x) = 255;
		  float Dx1, Dx2, Dy1, Dy2, D;
		  for( int j=0 ; j < N ; j++ ) {
			  Dx1 = Dx.at<float>( ((int)(i/N_x))*N+j, (i%N_x)*N );
			  Dx2 = Dx.at<float>( ((int)(i/N_x))*N+j, (i%N_x)*N+N-1 );
			  s1_x += abs(Dx1) + abs(Dx2);
			  max_x = (max_x > Dx1) ? max_x : Dx1;
			  max_x = (max_x > Dx2) ? max_x : Dx2;

			  Dy1 = Dy.at<float>(((int)(i/N_x))*N, (i%N_x)*N+j);
			  Dy2 = Dy.at<float>(((int)(i/N_x))*N+N-1, (i%N_x)*N+j);
			  s1_y += abs(Dy1) + abs(Dy2);
			  max_y = (max_y > Dy1) ? max_y : Dy1;
			  max_y = (max_y > Dy2) ? max_y : Dy2;
		  }
		  for( int j=0 ; j < (N-2)*(N-2) ; j++ ) {
			  D = sqrt(pow(Dx.at<float>( ((int)(i/N_x))*N+1+((int)(j/(N-2))), (i%N_x)*N+1+((int)(j%(N-2))) ), 2) +	\
					   pow(Dy.at<float>( ((int)(i/N_x))*N+1+((int)(j/(N-2))), (i%N_x)*N+1+((int)(j%(N-2))) ), 2));
			  max = (max > D) ? max : D;
			  s2 += D;
		  }
		  nbsets++;
	  }


	  memset ( tab, 0, sizeof(unsigned int)*256 );

	  /*
	  g_print("%3u ", (unsigned)sigma);

	  if( ((i+1)%N_x)==0 )
		  g_print("\n");
	  */
  }
  //g_print("\n\n");

  if (max_x && max_y && max) {
	  s1 = (s1_x/max_x + s1_y/max_y) / (4*N*nbsets);
	  s2 /= (max*pow(N-2, 2)*nbsets);
  }

  float S=0;
  if( s1 || s2 )
	  S = abs( (pow(s1,2)-pow(s2,2))/(pow(s1,2)+pow(s2,2)) );

  int num=0;
  for(int i=0; i<N_y*N_x ; i++)
  	  if( diff_tab[i] != -1 ) {
  		  num++;
  		  diff_avg+=diff_tab[i];
  		  //g_print("%3d ", diff_tab[i]);
  	  }
  if(num)
	  diff_avg/=num;
  if(sigma_num)
	  sigma_avg/=sigma_num;

  blockanalysis->res = blockanalysis->res + ((diff_avg-blockanalysis->res)/16);

  g_print("Diff mean = %.2f /64\tS = %.2f\r", blockanalysis->res, S);

  IplImage *img1, img_header1;
  img1 = cvGetImage(Sigma1, &img_header1);
  //blockanalysis->ShmIS.pushImage(img1, 5005, (char*)"blockanalysis Std dev 1");
  Dx.convertTo(Dx, CV_8UC1);
  IplImage dest = Dx;
  //blockanalysis->ShmIS.pushImage(&dest, 5011, (char*)"Sobel Dx");
  Dy.convertTo(Dy, CV_8UC1);
  dest = Dy;
  //blockanalysis->ShmIS.pushImage(&dest, 5013, (char*)"Sobel Dy");
  //img2 = cvGetImage(Sigma2, &img_header1);
  //blockanalysis->ShmIS.pushImage(img1, 5007, (char*)"blockanalysis Std dev 2");


  Dx.release();
  Dy.release();
  cvReleaseMat(&Sigma1);
  //cvReleaseMat(&Sigma2);
  cvReleaseMat(&Sum);
  cvReleaseMat(&Sqsum);
}
#endif

#if 0
{
  int N_x = (int)(blockanalysis->cvGRAY->width/N);
  int N_y = (int)(blockanalysis->cvGRAY->height/N);
  int width = blockanalysis->cvGRAY->width;
  int height = blockanalysis->cvGRAY->height;
  long unsigned int sigma_x=0, max=0;
  int* diff_tab = (int*)g_try_malloc(N_x*N_y*sizeof(long int));
  memset ( diff_tab, -1, sizeof(long int)*N_x*N_y );
  //TODO: Add a check on diff_tab
  unsigned int tab[256] = { 0 };		// Init the table to 0
  int mean=0;
  int line, col;

  for( int i=0 ; i < N_y ; i++ ) {
  	  for( int j=0 ; j < N_x ; j++ ) {
  		  unsigned long int mean=0, sigma=0;
  		  cvSetImageROI(blockanalysis->cvGRAY, cvRect(j*N, i*N, N, N));

  		  cv::Mat D( blockanalysis->cvGRAY );
  		  D.convertTo(D, CV_8UC1);

  		  for( int k=0 ; k<N*N ; k++ ) {
  			  mean+=D.at<unsigned char>(k/N,k);

  			  //g_print("Value: %u\n", D.at<unsigned char>(k/N,k));
  		  }
  		  mean/=(N*N);

  		  for( int k=0 ; k<N*N ; k++ ) {
  			  sigma+=pow(D.at<unsigned char>(k/N,k)-mean, 2);
  			  tab[D.at<unsigned char>(k/N,k)]++;
  		  }
  		  sigma=sqrt(sigma/(N*N));
  		  if( sigma>max ) {
  			  max = sigma;
  			  line = i;
  			  col = j;
  		  }
  		  if( sigma>10 ) {
  			int diff=0;
  			for( int k=0; k<256 ; k++)
  				if(tab[k])
  					diff++;
  			diff_tab[i*N_x+j] = diff;
  		  }
  		  g_print("%3lu ", sigma);
  		  sigma_x+=sigma;

  		  memset ( tab, 0, sizeof(unsigned int)*256 );
  	  }
  	  g_print("\n");
  }
  g_print("\n");

  int num=0;
  for(int i=0; i<N_y*N_x ; i++)
	  if( diff_tab[i] != -1 ) {
		  num++;
		  mean+=diff_tab[i];
		  g_print("%3d ", diff_tab[i]);
	  }
  mean/=num;
  g_print("\n\nMean = %d\n", mean);

  /*
  cvSetImageROI(blockanalysis->cvGRAY, cvRect(col*N, line*N, N, N));
  cv::Mat D( blockanalysis->cvGRAY );
  D.convertTo(D, CV_8UC1);

  for( int k=0 ; k<N*N ; k++ ) {
	  tab[D.at<unsigned char>(k/N,k)]++;
	  g_print("%3u ", D.at<unsigned char>(k/N,k));
  }
  int diff=0;
  for( int i=0; i<256 ; i++)
	  if(tab[i])
		  diff++;

  g_print("\nDiff= %d\n", diff);
  */
  //g_print("\n");
  sigma_x/=(N_x*N_y);
  g_print("\nOverall sigma: %lu\n\n", sigma_x);
  cvResetImageROI(blockanalysis->cvGRAY);
}
#endif

#if 0
  int N_x = (int)(blockanalysis->cvGRAY->width/N);
  int N_y = (int)(blockanalysis->cvGRAY->height/N);
  int width = blockanalysis->cvGRAY->width;
  int height = blockanalysis->cvGRAY->height;
  double s1=0, s1_x=0, s1_y=0, s2=0, max_x=0, max_y=0, max=0, tmp;

  cvSobel( blockanalysis->cvGRAY, blockanalysis->cvSobel_x, 1, 0, 3 );
  cvSobel( blockanalysis->cvGRAY, blockanalysis->cvSobel_y, 0, 1, 3 );

  cvConvertScale(blockanalysis->cvSobel_x, blockanalysis->cvSobelSc, 1, 0);
  //blockanalysis->ShmIS.pushImage(blockanalysis->cvSobelSc, 5009, (char*)"blockanalysis cvSobel_x");
  cvConvertScale(blockanalysis->cvSobel_y, blockanalysis->cvSobelSc, 1, 0);
  //blockanalysis->ShmIS.pushImage(blockanalysis->cvSobelSc, 5011, (char*)"blockanalysis cvSobel_y");

  {
  cv::Mat Dx( blockanalysis->cvSobel_x );
  Dx.convertTo(Dx, CV_64F);

  cv::Mat Dy( blockanalysis->cvSobel_y );
  Dy.convertTo(Dy, CV_64F);

  int test = N*N_x*N_y;

  for( int i,j,k=0 ; k < width*height ; k++ ) {
	  if( ((k/width)%N) && ((k/width+1)%N) && (k%N) && ((k+1)%N) ) {
		  tmp = sqrt(pow(Dx.at<double>(k/width,k%width), 2) + pow(Dy.at<double>(k/width,k%width), 2));
		  max = (max < tmp) ? tmp : max;
		  s2 += tmp;
		  //g_print("Ligne: %d\tColonne: %d\n", k/width, k%width);
	  }
	  if( k < test ) {
		  i = k%height;
		  j = k/height;
		  s1_x += abs(Dx.at<double>(i,j*N)) + abs(Dx.at<double>(i,j*N+N-1));
		  max_x = (max_x>Dx.at<double>(i,j*N)) ? max_x : Dx.at<double>(i,j*N);
		  max_x = (max_x>Dx.at<double>(i,j*N+N-1)) ? max_x : Dx.at<double>(i,j*N+N-1);
		  i = k/width;
		  j = k%width;
		  s1_y += abs(Dy.at<double>(i*N,j)) + abs(Dy.at<double>(i*N+N-1,j));
		  max_y = (max_y>Dy.at<double>(i*N,j)) ? max_y : Dy.at<double>(i*N,j);
		  max_y = (max_y>Dy.at<double>(i*N+N-1,j)) ? max_y : Dy.at<double>(i*N+N-1,j);
	  }
  }

  if (max_x && max_y && max) {
  	  s1 = (s1_x/max_x + s1_y/max_y) / (4*N*N_x*N_y);
  	  s2 /= (max*pow(N-2, 2)*N_x*N_y);
  }

  Dx.release();
  Dy.release();

  double S;
    if( s1 || s2 )
  	  S = abs( (pow(s1,2)-pow(s2,2))/(pow(s1,2)+pow(s2,2)) );

  g_print("1st method: s1 = %f\ts2 = %f\tS = %f\n", s1, s2, S);
  }
#endif
/*
  s1=s1_x=s1_y=s2=max_x=max_y=max=0;
  for( int i=0 ; i < N_y ; i++ ) {
	  for( int j=0 ; j < N_x ; j++ ) {
		  cvSetImageROI(blockanalysis->cvSobel_x, cvRect(j*N, i*N, N, N));
		  cvSetImageROI(blockanalysis->cvSobel_y, cvRect(j*N, i*N, N, N));

		  cv::Mat Dx( blockanalysis->cvSobel_x );
		  Dx.convertTo(Dx, CV_64F);

		  cv::Mat Dy( blockanalysis->cvSobel_y );
		  Dy.convertTo(Dy, CV_64F);

		  for( int k=0 ; k<N ; k++ ) {
			  s1_x += abs(Dx.at<double>(k,0)) + abs(Dx.at<double>(k,N-1));
			  max_x = (max_x>Dx.at<double>(k,0)) ? max_x : Dx.at<double>(k,0);
			  max_x = (max_x>Dx.at<double>(k,N-1)) ? max_x : Dx.at<double>(k,N-1);

			  s1_y += abs(Dy.at<double>(0,k)) + abs(Dy.at<double>(N-1,k));
			  max_y = (max_y>Dy.at<double>(0,k)) ? max_y : Dy.at<double>(0,k);
			  max_y = (max_y>Dy.at<double>(N-1,k)) ? max_y : Dy.at<double>(N-1,k);

			  for( int l=1 ; l<N-1 && k>0 && k<N-1 ; l++ ) {
				  tmp = sqrt(pow(Dx.at<double>(k,l), 2) + pow(Dy.at<double>(k,l), 2));
				  max = (max < tmp) ? tmp : max;
				  s2 += tmp;
			  }
		  }
		  Dx.release();
		  Dy.release();
	  }
  }
  if (max_x && max_y && max) {
	  s1 = (s1_x/max_x + s1_y/max_y) / (4*N*N_x*N_y);
	  s2 /= (max*pow(N-2, 2)*N_x*N_y);
  }
  cvResetImageROI(blockanalysis->cvSobel_x);
  cvResetImageROI(blockanalysis->cvSobel_y);

  double S;
  if( s1 || s2 )
	  S = abs( (pow(s1,2)-pow(s2,2))/(pow(s1,2)+pow(s2,2)) );

  g_print("2nd method: s1 = %f\ts2 = %f\tS = %f\n", s1, s2, S);
*/
  //cvResetImageROI(blockanalysis->cvGRAY);
  //cvSobel( blockanalysis->cvGRAY, blockanalysis->cvSobel, 1, 0, 3 );
  //cvConvertScaleAbs( blockanalysis->cvSobel, blockanalysis->cvSobelSc, 1, 0 );
  //cvCopy(blockanalysis->cvSobelSc, blockanalysis->cvGRAY);
  //cvResetImageROI(blockanalysis->cvGRAY);

  //////////////////////////////////////////////////////////////////////////////
  // here goes the business logic
  //////////////////////////////////////////////////////////////////////////////


  //////////////////////////////////////////////////////////////////////////////
  // normally here we would convert back RGB to YUV, overwriting the input
  // of this plugin element, which goes directly to the output
  //cvSobel( blockanalysis->cvGRAY, blockanalysis->cvSobel_x, 1, 0, 3 );
  //cvConvertScale(blockanalysis->cvSobel_x, blockanalysis->cvGRAY, 1, 0);
  //cvCvtColor( blockanalysis->cvSobel_y, blockanalysis->cvRGB, CV_GRAY2RGB );
  cvCvtColor( blockanalysis->cvGRAY, blockanalysis->cvRGB, CV_GRAY2RGB );
  cvCvtColor( blockanalysis->cvRGB, blockanalysis->cvYUV, CV_RGB2YUV );


 
  GST_BLOCKANALYSIS_UNLOCK (blockanalysis);
  
  return GST_FLOW_OK;
}


gboolean gst_blockanalysis_plugin_init(GstPlugin * plugin)
{
  //gst_controller_init(NULL, NULL);
  return gst_element_register(plugin, "blockanalysis", GST_RANK_NONE, GST_TYPE_BLOCKANALYSIS);
}
