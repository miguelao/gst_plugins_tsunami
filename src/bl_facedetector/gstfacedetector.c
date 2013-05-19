/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * Filter:
 * Copyright (C) 2000 Donald A. Graft
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
/**
 * SECTION:element-facedetectorV3
 *
 * Detects faces in video
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string.h>
#include <gst/gst.h>
#include "gstfacedetector.h"
#include <gst/video/video.h>
#include <cv.h>
#include <highgui.h>
#include "FaceDetectLib.h"


void DrawBoxesAroundFaces(IplImage *frame, char *str);


GST_DEBUG_CATEGORY_STATIC (facedetectorV3_debug);
#define GST_CAT_DEFAULT facedetectorV3_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

#define DEFAULT_DRAWBOX                 FALSE
#define DEFAULT_CLASSIFIER_FILE		"haarcascade_frontalface_default.xml"
char classifier_file[] = DEFAULT_CLASSIFIER_FILE;
#define DEFAULT_MIN_FACE_SIZE		80
/* #define DEFAULT_NEWMEDIA             FALSE */
#define DEFAULT_COMPRESSION_LEVEL       6
#define IMAGE_NUM_CHANNELS 3


enum
{
  ARG_0,
  ARG_DRAWBOX,
  ARG_CLASSIFIER,
  ARG_MINFACESIZE
};

static GstStaticPadTemplate facedetectorV3_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb")
    );

static GstStaticPadTemplate facedetectorV3_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw-rgb")
    );


GST_BOILERPLATE (GstFaceDetectorV3, gst_facedetectorV3, GstElement, GST_TYPE_ELEMENT);

static void gst_facedetectorV3_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_facedetectorV3_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_facedetectorV3_chain (GstPad * pad, GstBuffer * data);

 

static void
gst_facedetectorV3_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);


  gst_element_class_set_details_simple (element_class,
                                        "Bell Labs OpenCV FaceDetector",
                                        "Filter/Effect/Video",
                                        "Detects faces in videos, no tracking",
                                        "Senthil Kumar <senthil@alcatel-lucent.com>\n"
					"Miguel Casas-Sanchez <miguel.casas-sanchez@alcatel-lucent.com>");

  gst_element_class_add_pad_template
      (element_class, gst_static_pad_template_get (&facedetectorV3_sink_template));
  gst_element_class_add_pad_template
      (element_class, gst_static_pad_template_get (&facedetectorV3_src_template));

}


static void
gst_facedetectorV3_finalize (GObject * object)
{
  GstFaceDetectorV3 *facedetectorV3 = GST_FACEDETECTORV3 (object);

  //cvReleaseImage(&img);
  ReleaseFaceDetector(facedetectorV3->fdBuf); 
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_facedetectorV3_class_init (GstFaceDetectorV3Class * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  parent_class = (GstElementClass*) g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_facedetectorV3_get_property;
  gobject_class->set_property = gst_facedetectorV3_set_property;


  
  g_object_class_install_property (gobject_class, ARG_DRAWBOX,
      g_param_spec_boolean ("drawbox", "DrawBox",
          "Draw boxes around detected faces",
          DEFAULT_DRAWBOX, (GParamFlags) G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, ARG_CLASSIFIER,
      g_param_spec_string ("classifier", "ClassifierFile",
          "Name (including full path) of the classifier xml file", 
          DEFAULT_CLASSIFIER_FILE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
 
  g_object_class_install_property(gobject_class, ARG_MINFACESIZE,
      g_param_spec_int ("minfacesize", "MinFaceSize",
          "Size of the smallest face to be detected. If set to 80, the smallest detected face will be 80x80 pixels. The smaller the value, the larger the compute time.",
          40, 100000,
          DEFAULT_MIN_FACE_SIZE, (GParamFlags) G_PARAM_READWRITE));

  gobject_class->finalize = gst_facedetectorV3_finalize;
  GST_DEBUG_CATEGORY_INIT (facedetectorV3_debug, "facedetector", 0, "face detector");
}


static gboolean
gst_facedetectorV3_setcaps (GstPad * pad, GstCaps * caps)
{
  GstFaceDetectorV3 *facedetectorV3;
  const GValue *fps;
  GstStructure *structure;
  gboolean ret = TRUE;

  facedetectorV3 = GST_FACEDETECTORV3 (gst_pad_get_parent (pad));

  structure = gst_caps_get_structure (caps, 0);
  gst_structure_get_int (structure, "width", &facedetectorV3->width);
  gst_structure_get_int (structure, "height", &facedetectorV3->height);
  fps = gst_structure_get_value (structure, "framerate");

  GST_INFO("negotiated caps, width=%d, height=%d\r\n", facedetectorV3->width, facedetectorV3->height);
  ret = gst_pad_set_caps (facedetectorV3->srcpad, caps); //copy the caps from the sink to the source
  
  gst_object_unref (facedetectorV3);

  return ret;
}

static void
gst_facedetectorV3_init (GstFaceDetectorV3 * facedetectorV3, GstFaceDetectorV3Class * g_class)
{
  /* sinkpad */
  facedetectorV3->sinkpad = gst_pad_new_from_static_template
      (&facedetectorV3_sink_template, "sink");
  gst_pad_set_chain_function (facedetectorV3->sinkpad, gst_facedetectorV3_chain);
 
  /*   gst_pad_set_link_function (facedetectorV3->sinkpad, gst_facedetectorV3_sinklink); */
  /*   gst_pad_set_getcaps_function (facedetectorV3->sinkpad, gst_facedetectorV3_sink_getcaps); */

  gst_pad_set_setcaps_function (facedetectorV3->sinkpad, gst_facedetectorV3_setcaps);
  gst_element_add_pad (GST_ELEMENT (facedetectorV3), facedetectorV3->sinkpad);

  /* srcpad */
  facedetectorV3->srcpad = gst_pad_new_from_static_template
      (&facedetectorV3_src_template, "src");
  /*   gst_pad_set_getcaps_function (facedetectorV3->srcpad, gst_facedetectorV3_src_getcaps); */
  /*   gst_pad_set_setcaps_function (facedetectorV3->srcpad, gst_facedetectorV3_setcaps); */
  gst_element_add_pad (GST_ELEMENT (facedetectorV3), facedetectorV3->srcpad);


  facedetectorV3->drawbox = DEFAULT_DRAWBOX;
  facedetectorV3->classifier_filename = classifier_file;
  facedetectorV3->min_face_size = DEFAULT_MIN_FACE_SIZE;

 
}

void DrawBoxesAroundFaces(IplImage *frame, char *str)
{

	int numFaces, x, y, width, height;
	char *strPtr = str;
	int i;

	sscanf(strPtr, "# of faces = %d", &numFaces); 
	//printf("Number of faces = %d\n", numFaces);
	strPtr = strchr(strPtr, ';') + 1;
	for(i=0; i<numFaces; ++i)
	{
		sscanf(strPtr, "%d %d %d %d", &x, &y, &width, &height);
		//printf("x, y, width, height = %d %d %d %d\n", x, y, width, height);
		cvRectangle(frame, cvPoint( x, y ), cvPoint( x+width, y+height ), cvScalar(0.0, 0.0, 255.0, 0.0), 3, 8, 0);
		strPtr = strchr(strPtr, ';') + 1;
	}
}

static GstFlowReturn
gst_facedetectorV3_chain (GstPad * pad, GstBuffer * buf)
{
  GstFaceDetectorV3 *facedetectorV3;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *output_video_buf = NULL;
  gint width, height;	
  IplImage *img = 0;
  char *str;
  //static int detector_lib_initialized = 0;
  GstClock *element_clock;
  GstClockTime timestampOut;
  GstStructure *s;
  GstMessage *msg;
  //GValue tstamp = {0};


  facedetectorV3 = GST_FACEDETECTORV3 (gst_pad_get_parent (pad));


//  if(!detector_lib_initialized){
//    //Ideally, this should be done in gst_facedetectorV3_init. 
//    //But, arguments passed in the command line are not yet known when that function gets called.
//    //What is the best place to call InitFaceDetector2 library function?
//    facedetectorV3->fdBuf=InitFaceDetector2(facedetectorV3->classifier_filename, facedetectorV3->min_face_size); //This is a facedetector library call. See FaceDetectLib.h 
//    detector_lib_initialized = 1;
//  }


  width = facedetectorV3->width;
  height = facedetectorV3->height;
  GST_DEBUG(" stream is %dx%d\r\n", width, height);
 
  //////////////////////////////////////////////////////////////////////////////
  img = cvCreateImage(cvSize(width,height), IPL_DEPTH_8U, IMAGE_NUM_CHANNELS);
  
  memcpy(img->imageData, GST_BUFFER_DATA(buf), width*height*IMAGE_NUM_CHANNELS);
  //memcpy(img->imageData, GST_BUFFER_DATA(buf), GST_BUFFER_SIZE(buf) );

  str = DetectFacesV2(img,facedetectorV3->fdBuf);
  if(facedetectorV3->drawbox)
	DrawBoxesAroundFaces(img, str);
  
  //Create output_video_buf and copy video data into it.
  output_video_buf = gst_buffer_copy(buf); //This creates output_video_buf as a copy of buf. Original image data is copied as well - that's a waste of time.
  gst_buffer_copy_metadata(output_video_buf, buf, (GstBufferCopyFlags)GST_BUFFER_COPY_ALL); 
  // todo: this is segment violation... use size of buffer?
  memcpy(GST_BUFFER_DATA(output_video_buf), img->imageData, width*height*IMAGE_NUM_CHANNELS);
 

  //We need to stamp the output buffer with a timestamp and copy that timestamp into the posted message so that 
  //elements down the pipeline can synchronize each output frame with its corresponding face metadata.
  //We could just copy the timestamp from the input buf to both places. However, if the facedetector
  //takes too much time, then the videosink will drop frames that carry really old timestamps (older than 20ms for dshowvideosink).
  //So, get the current time, adjust it for base_time and use it as the output timestamp for both the out buffer and the posted message.
  element_clock = gst_element_get_clock (GST_ELEMENT (facedetectorV3));
  if(element_clock) 
  {
      timestampOut = gst_clock_get_time (element_clock); //Get current time
      timestampOut -= gst_element_get_base_time (GST_ELEMENT (facedetectorV3)); //Subtract base time
      
      GST_BUFFER_TIMESTAMP(output_video_buf) = timestampOut;

      gst_object_unref (element_clock);
  }
  else //If you can't get the element's clock, copy input buffer's timestamp to both output buffers.
  {
     timestampOut = GST_BUFFER_TIMESTAMP(buf); //This is the timestamp on the input buf.
     GST_BUFFER_TIMESTAMP(output_video_buf) = timestampOut; //GST_CLOCK_TIME_NONE;
  }

  GST_INFO("[%s]", str);
  s = gst_structure_new ("facedetector_faceinfo", 
	  "facedata", G_TYPE_STRING, str, 
	  "timestamp", G_TYPE_UINT64, timestampOut, NULL);
 
  msg = gst_message_new_element(GST_OBJECT(facedetectorV3), s);
  if(gst_element_post_message (GST_ELEMENT (facedetectorV3), msg) == FALSE)
  {
 	  printf("Error. facedetectorV3: Could not send facedata message. gst_element_post_message() failed.");	
  }

  int x,y,w,h;
  if( 4 == sscanf(str,"# of faces = 1;%d %d %d %d", &x, &y, &w, &h)){
    //miguel casas, addendum, also send an inbound message downstream
    GST_INFO("sending custom DS event of face detection on (%d,%d)", x, y);
    GstStructure *str = gst_structure_new("facelocation", 
                                          "x", G_TYPE_DOUBLE, (float)x,
                                          "y", G_TYPE_DOUBLE, (float)y,
                                          "width", G_TYPE_DOUBLE, (float)w, 
                                          "height", G_TYPE_DOUBLE, (float)h * 0.85, NULL);
    GstEvent* ev = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, str);  
    gst_pad_push_event( facedetectorV3->srcpad , ev);
  }
  else{
    GstStructure *str = gst_structure_new("facelocation", 
                                          "x", G_TYPE_DOUBLE, (float)0,
                                          "y", G_TYPE_DOUBLE, (float)0,
                                          "width", G_TYPE_DOUBLE, (float)0, 
                                          "height", G_TYPE_DOUBLE, (float)0, NULL);
    GstEvent* ev = gst_event_new_custom(GST_EVENT_CUSTOM_DOWNSTREAM, str);  
    gst_pad_push_event( facedetectorV3->srcpad , ev);
  }


  //Now push the output video buffer through the srcpad.
  ret = gst_pad_push (facedetectorV3->srcpad, output_video_buf);
  
  if(ret != GST_FLOW_OK)
  {
	  GST_DEBUG ("push() failed, flow = %s", gst_flow_get_name (ret));
  }
 
  free(str);
  gst_buffer_unref (buf);
  gst_object_unref (facedetectorV3);
  cvReleaseImage(&img);

  return ret;
}


static void
gst_facedetectorV3_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstFaceDetectorV3 *facedetectorV3;

  facedetectorV3 = GST_FACEDETECTORV3 (object);

  switch (prop_id) {
    case ARG_DRAWBOX:
      g_value_set_boolean (value, facedetectorV3->drawbox);
      break;
    case ARG_CLASSIFIER:
      g_value_set_string (value, facedetectorV3->classifier_filename);
      break;
    case ARG_MINFACESIZE:
      g_value_set_int (value, facedetectorV3->min_face_size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


static void
gst_facedetectorV3_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstFaceDetectorV3 *facedetectorV3;

  facedetectorV3 = GST_FACEDETECTORV3 (object);

  switch (prop_id) {
    case ARG_DRAWBOX:
      facedetectorV3->drawbox = g_value_get_boolean (value);
      break;
    case ARG_CLASSIFIER:
      facedetectorV3->classifier_filename = g_strdup(g_value_get_string (value));
      if( facedetectorV3->fdBuf == NULL )
        facedetectorV3->fdBuf = InitFaceDetector2(facedetectorV3->classifier_filename, facedetectorV3->min_face_size);
     break;
    case ARG_MINFACESIZE:
      facedetectorV3->min_face_size = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}



gboolean gst_facedetector_plugin_init (GstPlugin * plugin)
{
  
  if (!gst_element_register (plugin, "facedetector", GST_RANK_PRIMARY,
          GST_TYPE_FACEDETECTORV3))
    return FALSE;

  return TRUE;
}

// this follows only if stand alone plug-in package
//GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
//    GST_VERSION_MINOR,
//    "facedetectorV3",
//    "Detects faces in video",
//    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
//


