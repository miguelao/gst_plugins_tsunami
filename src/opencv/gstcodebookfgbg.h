/* GStreamer
 */

#ifndef __GST_CODEBOOKFGBG_H__
#define __GST_CODEBOOKFGBG_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_CODEBOOKFGBG \
	(gst_codebookfgbg_get_type())
#define GST_CODEBOOKFGBG(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CODEBOOKFGBG,GstCodebookfgbg))
#define GST_CODEBOOKFGBG_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CODEBOOKFGBG,GstCodebookfgbgClass))
#define GST_IS_CODEBOOKFGBG(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CODEBOOKFGBG))
#define GST_IS_CODEBOOKFGBG_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CODEBOOKFGBG))

typedef struct _GstCodebookfgbg GstCodebookfgbg;
typedef struct _GstCodebookfgbgClass GstCodebookfgbgClass;

#define CHANNELS 3
typedef struct ce {
  unsigned char learnHigh[CHANNELS]; //High side threshold for learning
  unsigned char learnLow[CHANNELS];  //Low side threshold for learning
  unsigned char max[CHANNELS];       //High side of box boundary
  unsigned char min[CHANNELS];       //Low side of box boundary
  int t_last_update;         //Allow us to kill stale entries
  int stale;             //max negative run (longest period of inactivity)
} code_element;


typedef struct code_book {
  code_element **cb; 
  int numEntries; 
  int t;               //count every access
} codeBook;

struct _GstCodebookfgbg {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height;
  

  IplImage* pFrame ;
  IplImage* pFrame2 ;
  IplImage* pCodeBookData;

  gboolean  normalize;
  IplImage* pFrameYUV;
  IplImage* pFrameY; IplImage* pFrameU; IplImage* pFrameV;

  IplImage* pFrImg ;  // used for the alpha BW 1ch image composition
  int       nFrmNum;
  codeBook* TcodeBook;

  IplImage* ch1;
  IplImage* ch2;
  IplImage* ch3;

  bool      display;  
  bool      posterize;  
  bool      experimental;  

#define CONNCOMPONENTS
 
#ifdef CONNCOMPONENTS
  IplImage* pFrameScratch ;
  CvMemStorage *storage;
#endif
};

struct _GstCodebookfgbgClass {
  GstVideoFilterClass parent_class;
};

GType gst_codebookfgbg_get_type(void);

G_END_DECLS

gboolean gst_codebookfgbg_plugin_init(GstPlugin * plugin);

#endif /* __GST_CODEBOOKFGBG_H__ */
