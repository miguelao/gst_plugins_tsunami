/*
 */

#ifndef __GST_SSIM2_H__
#define __GST_SSIM2_H__

#include <gst/gst.h>
#include <cv.h>

G_BEGIN_DECLS

#define GST_TYPE_SSIM2            (gst_ssim2_get_type())
#define GST_SSIM2(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SSIM2,GstSSIM2))
#define GST_IS_SSIM2(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SSIM2))
#define GST_SSIM2_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_SSIM2,GstSSIM2Class))
#define GST_IS_SSIM2_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_SSIM2))
#define GST_SSIM2_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_SSIM2,GstSSIM2Class))
typedef struct _GstSSIM2 GstSSIM2;
typedef struct _GstSSIM2Class GstSSIM2Class;

typedef void (*GstSSIM2ProcessFunc) (GstSSIM2 *, guint8 *, guint);

struct _GstSSIM2
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

  double luma_ssim2_sum;
  double chroma_ssim2_sum;
  int n_frames;

  CvSize imgSize;
  int actualChannels;
  IplImage* test_img;
  IplImage* ref_img;

  float accu_mssim;

};

struct _GstSSIM2Class
{
  GstElementClass parent;
};

GType gst_ssim2_get_type (void);

gboolean gst_ssim2_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __GST_SSIM2_H__ */
