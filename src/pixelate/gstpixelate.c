/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2003> David Schleef <ds@schleef.org>
 * Copyright (C) <2010> Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include "gstpixelate.h"

#include <string.h>
#include <gst/gst.h>
#include <gst/controller/gstcontroller.h>
#include <gst/video/video.h>
#include <cv.h>

/* GstPixelate properties */
enum {
	PROP_0,
	PROP_HEIGHT,
	PROP_WIDTH,
	PROP_X,
	PROP_Y,
	PROP_XF,
	PROP_YF,
	PROP_SCALE,
	PROP_SMOOTH
/* FILL ME */
};

#define PROP_METHOD_DEFAULT 0

GST_DEBUG_CATEGORY_STATIC( pixelate_debug);
#define GST_CAT_DEFAULT pixelate_debug

static GstStaticPadTemplate gst_pixelate_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (
				GST_VIDEO_CAPS_ARGB ";" GST_VIDEO_CAPS_BGRA ";"
				GST_VIDEO_CAPS_ABGR ";" GST_VIDEO_CAPS_RGBA ";"
		)
);

static GstStaticPadTemplate gst_pixelate_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (
				GST_VIDEO_CAPS_ARGB ";" GST_VIDEO_CAPS_BGRA ";"
				GST_VIDEO_CAPS_ABGR ";" GST_VIDEO_CAPS_RGBA ";"
		)
);


GST_BOILERPLATE(GstPixelate, gst_pixelate, GstVideoFilter, GST_TYPE_VIDEO_FILTER);


static GstCaps *gst_pixelate_transform_caps (GstBaseTransform * trans, GstPadDirection direction, GstCaps * caps)
{
  GstCaps *ret;
  GstStructure *structure;

  /* this function is always called with a simple caps */
  g_return_val_if_fail (GST_CAPS_IS_SIMPLE (caps), NULL);

  ret = gst_caps_copy (caps);
  structure = gst_structure_copy (gst_caps_get_structure (ret, 0));

  gst_structure_set (structure,
      "width", GST_TYPE_INT_RANGE, 1, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);

  /* if pixel aspect ratio, make a range of it */
  if (gst_structure_has_field (structure, "pixel-aspect-ratio")) {
    gst_structure_set (structure,
        "pixel-aspect-ratio", GST_TYPE_FRACTION_RANGE, 1, G_MAXINT, G_MAXINT, 1,
        NULL);
  }
  gst_caps_merge_structure (ret, gst_structure_copy (structure));
  gst_structure_free (structure);

  GST_DEBUG_OBJECT (trans, "returning caps: %" GST_PTR_FORMAT, ret);

  return ret;
}

static gboolean gst_pixelate_get_unit_size(GstBaseTransform * btrans,
		GstCaps * caps, guint * size) {
	GstVideoFormat format;
	gint width, height;

	if (!gst_video_format_parse_caps(caps, &format, &width, &height))
		return FALSE;

	*size = gst_video_format_get_size(format, width, height);

	GST_DEBUG_OBJECT(btrans, "our frame size is %d bytes (%dx%d)", *size,
			width, height);

	return TRUE;
}

static gboolean gst_pixelate_set_caps(GstBaseTransform * btrans, GstCaps * incaps,
		GstCaps * outcaps) {
	GstPixelate *vf = GST_PIXELATE (btrans);
	GstVideoFormat in_format, out_format;
	gboolean ret = FALSE;

	vf->process = NULL;

	if (!gst_video_format_parse_caps(incaps, &in_format, &vf->from_width,
			&vf->from_height) || !gst_video_format_parse_caps(outcaps,
			&out_format, &vf->to_width, &vf->to_height))
		goto invalid_caps;

	printf("to_size: %d,%d\n",vf->to_width, vf->to_height);
	return TRUE;

	invalid_caps:
	GST_ERROR_OBJECT (vf, "Invalid caps: %" GST_PTR_FORMAT " -> %" GST_PTR_FORMAT, incaps, outcaps);
	return FALSE;
}

static void gst_pixelate_before_transform(GstBaseTransform * trans, GstBuffer * in) {
	GstPixelate *pixelate = GST_PIXELATE (trans);
	GstClockTime timestamp, stream_time;

	timestamp = GST_BUFFER_TIMESTAMP(in);
	stream_time = gst_segment_to_stream_time(&trans->segment, GST_FORMAT_TIME, timestamp);
	GST_DEBUG_OBJECT (pixelate, "sync to %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));

	if (GST_CLOCK_TIME_IS_VALID(stream_time))
		gst_object_sync_values(G_OBJECT(pixelate), stream_time);
}

static GstFlowReturn gst_pixelate_transform(GstBaseTransform * trans,
		GstBuffer * in, GstBuffer * out) {
	GstPixelate *pixelate = GST_PIXELATE (trans);
	guint8 *dst;
	const guint8 *src;
	IplImage *srcImg;
	IplImage *dstImg;
	gint roi_x,roi_y,roi_width, roi_height;
	gfloat roi_scale,roi_xf,roi_yf;

	GST_OBJECT_LOCK(pixelate);
	roi_height = pixelate->roi_height;
	roi_width = pixelate->roi_width;
	roi_x = pixelate->roi_x;
	roi_y = pixelate->roi_y;
	roi_scale = pixelate->roi_scale;
	roi_xf = pixelate->roi_xf;
	roi_yf = pixelate->roi_yf;
	GST_OBJECT_UNLOCK(pixelate);

        gint chan=4;
        gint pixsize=5;
        if(roi_width>50){
          pixsize=roi_width/10;
        }

	if (roi_scale != pixelate->last_roi_scale) {
		pixelate->last_roi_scale = roi_scale;
	}

	if (fabs(pixelate->current_roi_scale-roi_scale) > 0.05)
	{
		if (pixelate->smooth!=0.0) {
		// move scale towards target
		if (roi_scale > pixelate->current_roi_scale) {
			pixelate->current_roi_scale += pixelate->smooth;
		}
		if (roi_scale < pixelate->current_roi_scale) {
			pixelate->current_roi_scale -= pixelate->smooth;
		}
		} else {
			pixelate->current_roi_scale = roi_scale;
		}
	}

	if (pixelate->current_roi_scale<0.05)
		pixelate->current_roi_scale=0.05;
#if 0
	if (fabs(pixelate->current_roi_scale-roi_scale) < .05) {
		pixelate->current_roi_scale = 1.0;
	}
#endif
	roi_scale = pixelate->current_roi_scale;

	src = GST_BUFFER_DATA(in);
	dst = GST_BUFFER_DATA(out);

	srcImg = cvCreateImageHeader(cvSize(pixelate->from_width,pixelate->from_height), IPL_DEPTH_8U, 4);
	srcImg->imageData = (char*)src;

	dstImg = cvCreateImageHeader(cvSize(pixelate->to_width,pixelate->to_height), IPL_DEPTH_8U, 4);
	dstImg->imageData = (char*)dst;

        gint step=pixelate->from_width*chan;
	for (int y = 0; y < pixelate->from_height; y++) {
	  for (int x = 0; x < pixelate->from_width; x++) {
	    if( ((x<roi_x) || (x>roi_x+roi_width)) || 
                ((y<roi_y) || (y>roi_y+roi_height)) ){
              gint coord=y*step+x*chan;
	      dst[coord  ]=src[coord  ];
	      dst[coord+1]=src[coord+1];
	      dst[coord+2]=src[coord+2];
	    } else {
              // pixelate
              gint src_coord=(y-y%pixsize)*step+(x-x%pixsize)*chan;
              gint dst_coord=y*step+x*chan;
	      dst[dst_coord  ]=src[src_coord  ];
	      dst[dst_coord+1]=src[src_coord+1];
	      dst[dst_coord+2]=src[src_coord+2];
            } 
	  }
	}

	cvReleaseImageHeader(&srcImg);
	cvReleaseImageHeader(&dstImg);

	return GST_FLOW_OK;

	not_negotiated: GST_ERROR_OBJECT(pixelate, "Not negotiated yet");
	return GST_FLOW_NOT_NEGOTIATED;
}

static gboolean gst_pixelate_src_event(GstBaseTransform * trans, GstEvent * event) {
	GstPixelate *vf = GST_PIXELATE (trans);
	gdouble new_x, new_y, x, y;
	GstStructure *structure;

	GST_DEBUG_OBJECT(vf, "handling %s event", GST_EVENT_TYPE_NAME(event));
	switch (GST_EVENT_TYPE(event)) {
	case GST_EVENT_NAVIGATION:
		event = GST_EVENT(gst_mini_object_make_writable(GST_MINI_OBJECT(event)));
		structure = (GstStructure *) gst_event_get_structure(event);
		break;
	default:
		break;
	}

	return TRUE;
}

static void gst_pixelate_set_property(GObject * object, guint prop_id,
		const GValue * value, GParamSpec * pspec) {
	GstPixelate *pixelate = GST_PIXELATE (object);
	gfloat new_x;
	gfloat new_y;

	GST_OBJECT_LOCK(pixelate);
	switch (prop_id) {
	case PROP_WIDTH:
		pixelate->roi_width = g_value_get_int (value);
		break;
	case PROP_HEIGHT:
		pixelate->roi_height = g_value_get_int (value);
		break;
	case PROP_X:
		pixelate->roi_x = g_value_get_int (value);
		break;
	case PROP_Y:
		pixelate->roi_y = g_value_get_int (value);
		break;
	case PROP_XF:
		new_x = g_value_get_float (value);
		pixelate->roi_xf = (float)(new_x);
//		pixelate->roi_xf =  ((float)pixelate->cv_roi_x  + (float)pixelate->cv_roi_width * new_x) / pixelate->from_width;
		break;
	case PROP_YF:
		new_y = g_value_get_float (value);
		pixelate->roi_yf = (float)(new_y);
//		pixelate->roi_yf =  ((float)pixelate->cv_roi_y  + (float)pixelate->cv_roi_height * new_y) / pixelate->from_height;
		break;

	case PROP_SCALE:
		pixelate->roi_scale = g_value_get_float (value);
		break;
	case PROP_SMOOTH:
		pixelate->smooth = g_value_get_float (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
	GST_OBJECT_UNLOCK(pixelate);

}

static void gst_pixelate_get_property(GObject * object, guint prop_id,
		GValue * value, GParamSpec * pspec) {
	GstPixelate *pixelate = GST_PIXELATE (object);

	switch (prop_id) {
	case PROP_WIDTH:
		g_value_set_int (value, pixelate->roi_width);
		break;
	case PROP_HEIGHT:
		g_value_set_int (value, pixelate->roi_height);
		break;
	case PROP_X:
		g_value_set_int (value, pixelate->roi_x);
		break;
	case PROP_Y:
		g_value_set_int (value, pixelate->roi_y);
		break;
	case PROP_XF:
		g_value_set_float (value, pixelate->roi_xf);
		break;
	case PROP_YF:
		g_value_set_float (value, pixelate->roi_yf);
		break;
	case PROP_SCALE:
		g_value_set_float(value, pixelate->roi_scale);
		break;
	case PROP_SMOOTH:
		g_value_set_float(value, pixelate->smooth);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gst_pixelate_base_init(gpointer g_class) {
	GstElementClass *element_class = GST_ELEMENT_CLASS(g_class);

	gst_element_class_set_details_simple(element_class, "Video flipper",
			"Filter/Effect/Video", "Flips and rotates video",
			"David Schleef <ds@schleef.org>");

	gst_element_class_add_pad_template(element_class,
			gst_static_pad_template_get(&gst_pixelate_sink_template));
	gst_element_class_add_pad_template(element_class,
			gst_static_pad_template_get(&gst_pixelate_src_template));
}

static void gst_pixelate_class_init(GstPixelateClass * klass) {
	GObjectClass *gobject_class = (GObjectClass *) klass;
	GstBaseTransformClass *trans_class = (GstBaseTransformClass *) klass;

	GST_DEBUG_CATEGORY_INIT(pixelate_debug, "pixelate", 0, "pixelate");

	gobject_class->set_property = gst_pixelate_set_property;
	gobject_class->get_property = gst_pixelate_get_property;

#if 0
	g_object_class_install_property(gobject_class, PROP_METHOD,
			g_param_spec_enum("method", "method", "method",
					GST_TYPE_PIXELATE_METHOD, PROP_METHOD_DEFAULT,
					GST_PARAM_CONTROLLABLE | G_PARAM_READWRITE
							| G_PARAM_STATIC_STRINGS));
#endif
	  g_object_class_install_property(gobject_class, PROP_WIDTH,
	      g_param_spec_int ("width", "width",
	          "New width of video",
	          -1, 100000,
	          -1, (GParamFlags) G_PARAM_READWRITE));

	  g_object_class_install_property(gobject_class, PROP_HEIGHT,
	      g_param_spec_int ("height", "height",
	          "New height of video",
	          -1, 100000,
	          -1, (GParamFlags) G_PARAM_READWRITE));

	  g_object_class_install_property(gobject_class, PROP_X,
	      g_param_spec_int ("x", "x",
	          "New x of video",
	          0, 100000,
	          0, (GParamFlags) G_PARAM_READWRITE));
	  g_object_class_install_property(gobject_class, PROP_Y,
	      g_param_spec_int ("y", "y",
	          "New y of video",
	          0, 100000,
	          0, (GParamFlags) G_PARAM_READWRITE));
	  g_object_class_install_property(gobject_class, PROP_XF,
	      g_param_spec_float ("xf", "xf",
	          "New x of video",
	          0, 100000,
	          0, (GParamFlags) G_PARAM_READWRITE));
	  g_object_class_install_property(gobject_class, PROP_YF,
	      g_param_spec_float ("yf", "yf",
	          "New y of video",
	          0, 100000,
	          0, (GParamFlags) G_PARAM_READWRITE));

	  g_object_class_install_property(gobject_class, PROP_SCALE,
	      g_param_spec_float ("scale", "scale",
	          "Scale (1.0=100%)",
	          0.0, 100000.0,
	          0.0, (GParamFlags) G_PARAM_READWRITE));
	  g_object_class_install_property(gobject_class, PROP_SMOOTH,
	      g_param_spec_float ("smooth", "smooth",
	          "Smooth pixelate transition amount",
	          0.0, 1.0,
	          0.0, (GParamFlags) G_PARAM_READWRITE));



	trans_class->transform_caps = GST_DEBUG_FUNCPTR(gst_pixelate_transform_caps);
	trans_class->set_caps = GST_DEBUG_FUNCPTR(gst_pixelate_set_caps);
	trans_class->get_unit_size = GST_DEBUG_FUNCPTR(gst_pixelate_get_unit_size);
	trans_class->transform = GST_DEBUG_FUNCPTR(gst_pixelate_transform);
	trans_class->before_transform
			= GST_DEBUG_FUNCPTR(gst_pixelate_before_transform);
	trans_class->src_event = GST_DEBUG_FUNCPTR(gst_pixelate_src_event);
}

static void gst_pixelate_init(GstPixelate * pixelate, GstPixelateClass * klass) {
	pixelate->method = (GstPixelateMethod)PROP_METHOD_DEFAULT;
	pixelate->current_roi_scale = 1.0;
	pixelate->last_roi_scale = 1.0;
	gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(pixelate), FALSE);
}

gboolean gst_pixelate_plugin_init(GstPlugin * plugin) {

	if (!gst_element_register(plugin, "pixelate", GST_RANK_PRIMARY, GST_TYPE_PIXELATE))
		return FALSE;

	return TRUE;
}

/*
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		"pixelate",
		"Pixelates in/out of video",
		gst_pixelate_plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
*/

