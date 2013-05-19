/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
 */


#ifndef __GST_FACEDETECTORV3_H__
#define __GST_FACEDETECTORV3_H__

#include <gst/gst.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//#define VERSION "0.10.0.1"
//#define PACKAGE "gst-gc-facedetect"
#define GST_LICENSE "LGPL"
#define GST_PACKAGE_NAME "FaceDetector"
#define GST_PACKAGE_ORIGIN "http://www.alcatel-lucent.com/bell-labs"

#define GST_TYPE_FACEDETECTORV3            (gst_facedetectorV3_get_type())
#define GST_FACEDETECTORV3(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_FACEDETECTORV3,GstFaceDetectorV3))
#define GST_FACEDETECTORV3_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_FACEDETECTORV3,GstFaceDetectorV3Class))
#define GST_IS_FACEDETECTORV3(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_FACEDETECTORV3))
#define GST_IS_FACEDETECTORV3_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_FACEDETECTORV3))

typedef struct _GstFaceDetectorV3 GstFaceDetectorV3;
typedef struct _GstFaceDetectorV3Class GstFaceDetectorV3Class;

struct _GstFaceDetectorV3
{
  GstElement element;

  GstPad *sinkpad, *srcpad, *txtsrcpad;
  //GstBuffer *buffer_out;
  //guint written;

  //png_structp png_struct_ptr;
  //png_infop png_info_ptr;

  gint width;
  gint height;
  //gint bpp;
  //gint stride;
  //guint compression_level;

  gboolean drawbox;
  gchar    *classifier_filename;
  gint     min_face_size;
  gboolean updateTimestamp;
  gboolean scaleDown; //If true, the input image is scaled down to 320x240 (or a comparable size) to speed-up face detection.
  double   scaleFactor;
  gint	   scaledWidth;
  gint     scaledHeight;
  gint	   skipFrames;
  gint	   framesToSkip;
  char     *prevResult; //saved face coordinates from the previous frame.
  gboolean newmedia;

  int init;
  void *fdBuf; //pointer returned by facedetectInit();
};

struct _GstFaceDetectorV3Class
{
  GstElementClass parent_class;
};

GType gst_facedetectorV3_get_type(void);

gboolean gst_facedetector_plugin_init (GstPlugin * plugin);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GST_FACEDETECTORV3_H__ */
