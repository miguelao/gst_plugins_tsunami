/* 
 * GStreamer
 * Copyright (C) 2007,2009 David Schleef <ds@schleef.org>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <string.h>

#include "gstssim2.h"
//#ifdef HAVE_ORC
//#include <orc/orc.h>
//#endif
#include <math.h>

#include "ssim.h"

#define GST_CAT_DEFAULT gst_ssim2_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


enum
{
  PROP_0,
  LUMA_PSNR,
  CHROMA_PSNR
};

#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_ssim2_debug, "ssim2", 0, "cogssim2 element");

GST_BOILERPLATE_FULL (GstSSIM2, gst_ssim2, GstElement,
    GST_TYPE_ELEMENT, DEBUG_INIT);

static void gst_ssim2_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ssim2_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_ssim2_chain_test (GstPad * pad, GstBuffer * buffer);
static GstFlowReturn gst_ssim2_chain_ref (GstPad * pad, GstBuffer * buffer);
static gboolean gst_ssim2_sink_event (GstPad * pad, GstEvent * event);
static void gst_ssim2_reset (GstSSIM2 * filter);
static GstCaps *gst_ssim2_getcaps (GstPad * pad);
static gboolean gst_ssim2_set_caps (GstPad * pad, GstCaps * outcaps);
static void gst_ssim2_finalize (GObject * object);


static GstStaticPadTemplate gst_framestore_sink_ref_template =
GST_STATIC_PAD_TEMPLATE ("sink_ref",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB)
    );

static GstStaticPadTemplate gst_framestore_sink_test_template =
GST_STATIC_PAD_TEMPLATE ("sink_test",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB)
    );

static GstStaticPadTemplate gst_framestore_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGB)
    );

static void
gst_ssim2_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_framestore_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_framestore_sink_ref_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_framestore_sink_test_template));

  gst_element_class_set_details_simple (element_class,
                                       "Structural Similarity calculation 2",
                                       "Filter/Effect/Video",
                                       "Calculates Structural SIMilarity bw two image streams compared \n\
frame vs frame, both must be same framesize and rate, since they are matched\n\
one-to-one. The input @test is forwarded to the output",
                                       "miguel casas-sanchez@alcatel-lucent.com");
}

static void
gst_ssim2_class_init (GstSSIM2Class * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_ssim2_set_property;
  gobject_class->get_property = gst_ssim2_get_property;

  gobject_class->finalize = gst_ssim2_finalize;

  g_object_class_install_property (gobject_class, LUMA_PSNR,
      g_param_spec_double ("luma-psnr", "luma-psnr", "luma-psnr",
          0, 70, 40, G_PARAM_READABLE));
  g_object_class_install_property (gobject_class, CHROMA_PSNR,
      g_param_spec_double ("chroma-psnr", "chroma-psnr", "chroma-psnr",
          0, 70, 40, G_PARAM_READABLE));

}

static void
gst_ssim2_init (GstSSIM2 * filter, GstSSIM2Class * klass)
{
  gst_element_create_all_pads (GST_ELEMENT (filter));

  filter->srcpad = gst_element_get_static_pad (GST_ELEMENT (filter), "src");

  gst_pad_set_getcaps_function (filter->srcpad, gst_ssim2_getcaps);

  filter->sinkpad_ref =
      gst_element_get_static_pad (GST_ELEMENT (filter), "sink_ref");

  gst_pad_set_chain_function (filter->sinkpad_ref, gst_ssim2_chain_ref);
  gst_pad_set_event_function (filter->sinkpad_ref, gst_ssim2_sink_event);
  gst_pad_set_getcaps_function (filter->sinkpad_ref, gst_ssim2_getcaps);
  gst_pad_set_setcaps_function (filter->sinkpad_ref, gst_ssim2_set_caps);

  filter->sinkpad_test =
      gst_element_get_static_pad (GST_ELEMENT (filter), "sink_test");

  gst_pad_set_chain_function (filter->sinkpad_test, gst_ssim2_chain_test);
  gst_pad_set_event_function (filter->sinkpad_test, gst_ssim2_sink_event);
  gst_pad_set_getcaps_function (filter->sinkpad_test, gst_ssim2_getcaps);
  gst_pad_set_setcaps_function (filter->sinkpad_test, gst_ssim2_set_caps);

  gst_ssim2_reset (filter);

  filter->cond = g_cond_new ();
  filter->lock = g_mutex_new ();
}

static void
gst_ssim2_finalize (GObject * object)
{
  GstSSIM2 *fs = GST_SSIM2 (object);

  g_mutex_free (fs->lock);
  g_cond_free (fs->cond);
}

static GstCaps *
gst_ssim2_getcaps (GstPad * pad)
{
  GstSSIM2 *fs;
  GstCaps *caps;
  GstCaps *icaps;
  GstCaps *peercaps;

  fs = GST_SSIM2 (gst_pad_get_parent (pad));

  caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));

  if (pad != fs->srcpad) {
    peercaps = gst_pad_peer_get_caps (fs->srcpad);
    if (peercaps) {
      icaps = gst_caps_intersect (caps, peercaps);
      gst_caps_unref (caps);
      gst_caps_unref (peercaps);
      caps = icaps;
    }
  }

  if (pad != fs->sinkpad_ref) {
    peercaps = gst_pad_peer_get_caps (fs->sinkpad_ref);
    if (peercaps) {
      icaps = gst_caps_intersect (caps, peercaps);
      gst_caps_unref (caps);
      gst_caps_unref (peercaps);
      caps = icaps;
    }
  }

  if (pad != fs->sinkpad_test) {
    peercaps = gst_pad_peer_get_caps (fs->sinkpad_ref);
    if (peercaps) {
      icaps = gst_caps_intersect (caps, peercaps);
      gst_caps_unref (caps);
      gst_caps_unref (peercaps);
      caps = icaps;
    }
  }

  gst_object_unref (fs);

  return caps;
}

static gboolean
gst_ssim2_set_caps (GstPad * pad, GstCaps * caps)
{
  GstSSIM2 *fs;

  fs = GST_SSIM2 (gst_pad_get_parent (pad));

  gst_video_format_parse_caps (caps, &fs->format, &fs->width, &fs->height);

  fs->imgSize = cvSize(fs->width, fs->height);
  fs->actualChannels = 3;
  fs->test_img = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, fs->actualChannels);
  fs->ref_img  = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, fs->actualChannels);
  
  initialise_ssim( fs->width, fs->height, fs->actualChannels );

  fs->ref_img->imageData = (char*)malloc( fs->width * fs->height * fs->actualChannels);

  GST_WARNING( " Negotiated caps, width=%dp height=%dp channels=%d",
               fs->width, fs->height, fs->actualChannels);

  gst_object_unref (fs);

  return TRUE;
}

static void
gst_ssim2_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ssim2_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstSSIM2 *fs = GST_SSIM2 (object);

  switch (prop_id) {
    case LUMA_PSNR:
      break;
    case CHROMA_PSNR:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_ssim2_reset (GstSSIM2 * fs)
{
  fs->luma_ssim2_sum = 0;
  fs->chroma_ssim2_sum = 0;
  fs->n_frames = 0;
  fs->accu_mssim = 0.0;

  if (fs->buffer_ref) {
    gst_buffer_unref (fs->buffer_ref);
    fs->buffer_ref = NULL;
  }
}


static GstFlowReturn
gst_ssim2_chain_ref (GstPad * pad, GstBuffer * buffer)
{
  GstSSIM2 *fs;

  fs = GST_SSIM2 (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (fs,"chain ref");

  g_mutex_lock (fs->lock);
  while (fs->buffer_ref) {
    GST_DEBUG_OBJECT (fs, "waiting for ref buffer clear");
    g_cond_wait (fs->cond, fs->lock);
    if (fs->cancel) {
      g_mutex_unlock (fs->lock);
      gst_object_unref (fs);
      return GST_FLOW_WRONG_STATE;
    }
  }

  fs->buffer_ref = buffer;
  GST_DEBUG_OBJECT (fs, "signalling condition variable to test (%d,%d)",
                    GST_BUFFER_SIZE(buffer), fs->width * fs->height * fs->actualChannels );
  fs->ref_img->imageData = (char*) GST_BUFFER_DATA(fs->buffer_ref);  

  // i have my doubts that fs->ref_img->imageData still would point to sth reasonable
  // after unref'erencing the buffer (by alias of fs->buffer_ref)...
  gst_buffer_unref (buffer);
  g_cond_signal (fs->cond);
  g_mutex_unlock (fs->lock);

  gst_object_unref (fs);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_ssim2_chain_test (GstPad * pad, GstBuffer * buffer)
{
  GstSSIM2 *fs;
  GstFlowReturn ret;
  GstBuffer *buffer_ref;

  fs = GST_SSIM2 (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (fs, "chain test");

  g_mutex_lock (fs->lock);
  while (fs->buffer_ref == NULL) {
    GST_DEBUG_OBJECT (fs, "waiting for ref buffer");
    g_cond_wait (fs->cond, fs->lock);
    GST_DEBUG_OBJECT (fs, "ref buffer found, cancel flag: %d", fs->cancel);
    if (fs->cancel) {
      g_mutex_unlock (fs->lock);
      gst_object_unref (fs);
      return GST_FLOW_WRONG_STATE;
    }
  }
  
  buffer_ref = fs->buffer_ref;
  fs->buffer_ref = NULL;
  g_cond_signal (fs->cond);  
  g_mutex_unlock (fs->lock);

  fs->test_img->imageData = (char*) GST_BUFFER_DATA(buffer);  
  
  GST_DEBUG_OBJECT (fs, "comparing frames");
  float mssim = calculate_ssim_from_iplimages( fs->test_img, fs->ref_img );
  fs->accu_mssim = ((mssim/3) + fs->accu_mssim );
  fs->n_frames++;
  GST_INFO_OBJECT(fs, "SSIM index %f", fs->accu_mssim/((float)fs->n_frames));

  ret = gst_pad_push (fs->srcpad, buffer);

  gst_object_unref (fs);

  return ret;
}

static gboolean
gst_ssim2_sink_event (GstPad * pad, GstEvent * event)
{
  GstSSIM2 *fs;

  fs = GST_SSIM2 (gst_pad_get_parent (pad));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_NEWSEGMENT:
    {
      gboolean update;
      double rate;
      double applied_rate;
      GstFormat format;
      gint64 start, stop, position;

      gst_event_parse_new_segment_full (event, &update, &rate, &applied_rate,
          &format, &start, &stop, &position);

      GST_DEBUG ("new_segment %d %g %g %d %" G_GINT64_FORMAT
          " %" G_GINT64_FORMAT " %" G_GINT64_FORMAT,
          update, rate, applied_rate, format, start, stop, position);

    }
      break;
    case GST_EVENT_FLUSH_START:
      GST_DEBUG ("flush start");
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG ("flush stop");
      break;
    case GST_EVENT_EOS:
      GST_WARNING ("SSIM overall %f", fs->accu_mssim/((float)fs->n_frames));
      GST_WARNING ("got EOS");
      break;
    default:
      break;
  }

  gst_pad_push_event (fs->srcpad, event);
  gst_object_unref (fs);

  return TRUE;
}




gboolean gst_ssim2_plugin_init(GstPlugin * plugin) {
	GST_DEBUG_CATEGORY_INIT (gst_ssim2_debug, "ssim2", 0, "Structural SIMilarity2");
	return gst_element_register(plugin, "ssim2", GST_RANK_NONE, GST_TYPE_SSIM2);
}
