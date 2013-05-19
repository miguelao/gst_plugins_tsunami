/* GStreamer H264 encoder plugin
 */

#ifndef __GST_H264_ENC_H__
#define __GST_H264_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>

#include "umc_h264_video_encoder.h"
#include "vm_time.h"
#include "vm_strings.h"
#include "umc_sys_info.h"
#include "vm_sys_info.h"

#include "ippvc.h"


G_BEGIN_DECLS

#define GST_TYPE_H264_ENC \
  (gst_h264_enc_get_type())
#define GST_H264_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_H264_ENC,GstH264Enc))
#define GST_H264_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_H264_ENC,GstH264EncClass))
#define GST_IS_H264_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_H264_ENC))
#define GST_IS_H264_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_H264_ENC))

typedef struct _GstH264Enc GstH264Enc;
typedef struct _GstH264EncClass GstH264EncClass;

struct _GstH264Enc
{
  GstElement element;

  /*< private >*/
  GstPad *sinkpad;
  GstPad *srcpad;

 UMC::H264VideoEncoder *h264enc;
 UMC::H264EncoderParams *h264param;

  /* properties */
  guint threads;
  gboolean byte_stream;
  guint bitrate;
  guint ref;
  guint bframes;
  guint keyint;

  GString *option_string; /* used by set prop */

  /* input description */
  GstVideoFormat format;
  gint width, height;
  gint fps_num, fps_den;
  gint par_num, par_den;
  /* cache some format properties */
  gint stride[4], offset[4];
  guint image_size;

  /* for b-frame delay handling */
  GQueue *delay;

  guint8 *buffer;
  gulong buffer_size;

  gint i_type;
  GstEvent *forcekeyunit_event;

  /* configuration changed  while playing */
  gboolean reconfig;
};

struct _GstH264EncClass
{
  GstElementClass parent_class;
};

GType gst_h264_enc_get_type (void);

G_END_DECLS

gboolean plugin_init_ipp_h264(GstPlugin *plugin);

#endif /* __GST_H264_ENC_H__ */
