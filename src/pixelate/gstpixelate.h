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

#ifndef __GST_PIXELATE_H__
#define __GST_PIXELATE_H__

//#define VERSION "0.10.0.1"
//#define PACKAGE "gst-gc-pixelate"
//#define GST_LICENSE "LGPL"
//#define GST_PACKAGE_NAME "pixelate"
//#define GST_PACKAGE_ORIGIN "http://www.alcatel-lucent.com/bell-labs"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

/**
 * GstPixelateMethod:
 * @GST_PIXELATE_METHOD_IDENTITY: Identity (no rotation)
 * @GST_PIXELATE_METHOD_90R: Rotate clockwise 90 degrees
 * @GST_PIXELATE_METHOD_180: Rotate 180 degrees
 * @GST_PIXELATE_METHOD_90L: Rotate counter-clockwise 90 degrees
 * @GST_PIXELATE_METHOD_HORIZ: Flip horizontally
 * @GST_PIXELATE_METHOD_VERT: Flip vertically
 * @GST_PIXELATE_METHOD_TRANS: Flip across upper left/lower right diagonal
 * @GST_PIXELATE_METHOD_OTHER: Flip across upper right/lower left diagonal
 *
 * The different flip methods.
 */
typedef enum {
  GST_PIXELATE_METHOD_IDENTITY,
  GST_PIXELATE_METHOD_90R,
  GST_PIXELATE_METHOD_180,
  GST_PIXELATE_METHOD_90L,
  GST_PIXELATE_METHOD_HORIZ,
  GST_PIXELATE_METHOD_VERT,
  GST_PIXELATE_METHOD_TRANS,
  GST_PIXELATE_METHOD_OTHER
} GstPixelateMethod;

#define GST_TYPE_PIXELATE \
  (gst_pixelate_get_type())
#define GST_PIXELATE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PIXELATE,GstPixelate))
#define GST_PIXELATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PIXELATE,GstPixelateClass))
#define GST_IS_PIXELATE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PIXELATE))
#define GST_IS_PIXELATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PIXELATE))

typedef struct _GstPixelate GstPixelate;
typedef struct _GstPixelateClass GstPixelateClass;

/**
 * GstPixelate:
 *
 * Opaque datastructure.
 */
struct _GstPixelate {
  GstVideoFilter videofilter;
  
  /* < private > */
  GstVideoFormat format;
  gint from_width, from_height;
  gint to_width, to_height;
  gint roi_x,roi_y,roi_width,roi_height;
  gfloat roi_scale,roi_xf,roi_yf;
  gfloat current_roi_scale;
  gfloat last_roi_scale;
  gint cv_roi_y, cv_roi_x;
  gint cv_roi_height, cv_roi_width;
  gfloat smooth;

  GstPixelateMethod method;
  void (*process) (GstPixelate *pixelate, guint8 *dest, const guint8 *src);
};

struct _GstPixelateClass {
  GstVideoFilterClass parent_class;
};

GType gst_pixelate_get_type (void);

gboolean gst_pixelate_plugin_init(GstPlugin * plugin);
 
G_END_DECLS

#endif /* __GST_PIXELATE_H__ */
