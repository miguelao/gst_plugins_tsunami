/*
 */

#ifndef __GST_ALPHAMIX_H__
#define __GST_ALPHAMIX_H__

#include <gst/gst.h>
#include <cv.h>

G_BEGIN_DECLS

#define GST_TYPE_ALPHAMIX            (gst_alphamix_get_type())
#define GST_ALPHAMIX(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_ALPHAMIX,GstALPHAMIX))
#define GST_IS_ALPHAMIX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_ALPHAMIX))
#define GST_ALPHAMIX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_ALPHAMIX,GstALPHAMIXClass))
#define GST_IS_ALPHAMIX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_ALPHAMIX))
#define GST_ALPHAMIX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_ALPHAMIX,GstALPHAMIXClass))
typedef struct _GstALPHAMIX GstALPHAMIX;
typedef struct _GstALPHAMIXClass GstALPHAMIXClass;

typedef void (*GstALPHAMIXProcessFunc) (GstALPHAMIX *, guint8 *, guint);

struct _GstALPHAMIX
{
  GstElement element;

  /* < private > */
  GstPad *srcpad;
  GstPad *sinkpad_ref;
  GstPad *sinkpad_test;

  GstBuffer *buffer_ref;

  GMutex *lock;
  GCond *cond;
  gboolean cancel;

  GstVideoFormat format;
  int width;
  int height;

  double luma_alphamix_sum;
  double chroma_alphamix_sum;
  int n_frames;

  CvSize imgSize;
  int actualChannels;
  IplImage* test_img;
  IplImage* ref_img;

  IplImage* test_ch1;
  IplImage* test_ch2;
  IplImage* test_ch3;
  IplImage* test_chA;
  IplImage* ref_ch1;
  IplImage* ref_ch2;
  IplImage* ref_ch3;
  IplImage* ref_chA;

  float accu_mssim;

};

struct _GstALPHAMIXClass
{
  GstElementClass parent;
};

GType gst_alphamix_get_type (void);

gboolean gst_alphamix_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __GST_ALPHAMIX_H__ */
