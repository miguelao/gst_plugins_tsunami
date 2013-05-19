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

#include "gstalphamix.h"
#include <math.h>

#define GST_CAT_DEFAULT gst_alphamix_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


enum
{
  PROP_0,
  LUMA_PSNR,
  CHROMA_PSNR
};

#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_alphamix_debug, "alphamix", 0, "alphamix element");

GST_BOILERPLATE_FULL (GstALPHAMIX, gst_alphamix, GstElement,
    GST_TYPE_ELEMENT, DEBUG_INIT);

static void gst_alphamix_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_alphamix_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstFlowReturn gst_alphamix_chain_test (GstPad * pad, GstBuffer * buffer);
static GstFlowReturn gst_alphamix_chain_ref (GstPad * pad, GstBuffer * buffer);
static gboolean gst_alphamix_sink_event (GstPad * pad, GstEvent * event);
static void gst_alphamix_reset (GstALPHAMIX * filter);
static GstCaps *gst_alphamix_getcaps (GstPad * pad);
static gboolean gst_alphamix_set_caps (GstPad * pad, GstCaps * outcaps);
static void gst_alphamix_finalize (GObject * object);


static GstStaticPadTemplate gst_framestore_sink_ref_template =
GST_STATIC_PAD_TEMPLATE ("sink_ref",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
    );

static GstStaticPadTemplate gst_framestore_sink_test_template =
GST_STATIC_PAD_TEMPLATE ("sink_test",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
    );

static GstStaticPadTemplate gst_framestore_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_RGBA)
    );

static void
gst_alphamix_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_framestore_src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_framestore_sink_ref_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_framestore_sink_test_template));

  gst_element_class_set_details_simple (element_class,
                                       "Alpha mixer",
                                       "Filter/Effect/Video",
                                       "Mixes the alpha channels of both inputs, \n\
frame by frame, both must be same framesize and rate, since they are matched\n\
one-to-one. The input @ref is forwarded to the output",
                                       "miguel casas-sanchez@alcatel-lucent.com");
}

static void
gst_alphamix_class_init (GstALPHAMIXClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_alphamix_set_property;
  gobject_class->get_property = gst_alphamix_get_property;

  gobject_class->finalize = gst_alphamix_finalize;

  g_object_class_install_property (gobject_class, LUMA_PSNR,
      g_param_spec_double ("luma-psnr", "luma-psnr", "luma-psnr",
          0, 70, 40, G_PARAM_READABLE));
  g_object_class_install_property (gobject_class, CHROMA_PSNR,
      g_param_spec_double ("chroma-psnr", "chroma-psnr", "chroma-psnr",
          0, 70, 40, G_PARAM_READABLE));

}

static void
gst_alphamix_init (GstALPHAMIX * filter, GstALPHAMIXClass * klass)
{
  gst_element_create_all_pads (GST_ELEMENT (filter));

  filter->srcpad = gst_element_get_static_pad (GST_ELEMENT (filter), "src");

  gst_pad_set_getcaps_function (filter->srcpad, gst_alphamix_getcaps);

  filter->sinkpad_ref =
      gst_element_get_static_pad (GST_ELEMENT (filter), "sink_ref");

  gst_pad_set_chain_function (filter->sinkpad_ref, gst_alphamix_chain_ref);
  gst_pad_set_event_function (filter->sinkpad_ref, gst_alphamix_sink_event);
  gst_pad_set_getcaps_function (filter->sinkpad_ref, gst_alphamix_getcaps);
  gst_pad_set_setcaps_function (filter->sinkpad_ref, gst_alphamix_set_caps);

  filter->sinkpad_test =
      gst_element_get_static_pad (GST_ELEMENT (filter), "sink_test");

  gst_pad_set_chain_function (filter->sinkpad_test, gst_alphamix_chain_test);
  gst_pad_set_event_function (filter->sinkpad_test, gst_alphamix_sink_event);
  gst_pad_set_getcaps_function (filter->sinkpad_test, gst_alphamix_getcaps);
  gst_pad_set_setcaps_function (filter->sinkpad_test, gst_alphamix_set_caps);

  gst_alphamix_reset (filter);

  filter->cond = g_cond_new ();
  filter->lock = g_mutex_new ();
}

static void
gst_alphamix_finalize (GObject * object)
{
  GstALPHAMIX *fs = GST_ALPHAMIX (object);

  g_mutex_free (fs->lock);
  g_cond_free (fs->cond);
}

static GstCaps *
gst_alphamix_getcaps (GstPad * pad)
{
  GstALPHAMIX *fs;
  GstCaps *caps;
  GstCaps *icaps;
  GstCaps *peercaps;

  fs = GST_ALPHAMIX (gst_pad_get_parent (pad));

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
gst_alphamix_set_caps (GstPad * pad, GstCaps * caps)
{
  GstALPHAMIX *fs;

  fs = GST_ALPHAMIX (gst_pad_get_parent (pad));

  gst_video_format_parse_caps (caps, &fs->format, &fs->width, &fs->height);

  fs->imgSize = cvSize(fs->width, fs->height);
  fs->actualChannels = 4;
  fs->test_img = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, fs->actualChannels);
  fs->ref_img  = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, fs->actualChannels);

  fs->ref_ch1  = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
  fs->ref_ch2  = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
  fs->ref_ch3  = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
  fs->ref_chA  = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);

  fs->test_ch1 = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
  fs->test_ch2 = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
  fs->test_ch3 = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
  fs->test_chA = cvCreateImage(fs->imgSize, IPL_DEPTH_8U, 1);
    
  //fs->ref_img->imageData = (char*)malloc( fs->width * fs->height * fs->actualChannels);

  GST_WARNING( " Negotiated caps, width=%dp height=%dp channels=%d",
               fs->width, fs->height, fs->actualChannels);

  gst_object_unref (fs);

  return TRUE;
}

static void
gst_alphamix_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_alphamix_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstALPHAMIX *fs = GST_ALPHAMIX (object);

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
gst_alphamix_reset (GstALPHAMIX * fs)
{
  fs->luma_alphamix_sum = 0;
  fs->chroma_alphamix_sum = 0;
  fs->n_frames = 0;
  fs->accu_mssim = 0.0;

  if (fs->buffer_ref) {
    gst_buffer_unref (fs->buffer_ref);
    fs->buffer_ref = NULL;
  }
}


static GstFlowReturn
gst_alphamix_chain_ref (GstPad * pad, GstBuffer * buffer)
{
  GstALPHAMIX *fs;

  fs = GST_ALPHAMIX (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (fs,"chain ref");

  //////////////////////////////////////////////////////////////////////////////
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


  //////////////////////////////////////////////////////////////////////////////
  fs->buffer_ref = buffer;
  memcpy(fs->ref_img->imageData, (char*) GST_BUFFER_DATA(fs->buffer_ref), GST_BUFFER_SIZE(buffer));
  GST_DEBUG_OBJECT (fs, "signalling condition variable to test (%d,%d)",
                    GST_BUFFER_SIZE(buffer), fs->width * fs->height * fs->actualChannels );


  //////////////////////////////////////////////////////////////////////////////
  // i have my doubts that fs->ref_img->imageData still would point to sth reasonable
  // after unref'erencing the buffer (by alias of fs->buffer_ref)...
  gst_buffer_unref (buffer);
  g_cond_signal (fs->cond);
  g_mutex_unlock (fs->lock);

  gst_object_unref (fs);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_alphamix_chain_test (GstPad * pad, GstBuffer * buffer)
{
  GstALPHAMIX *fs;
  GstBuffer *buffer_ref;
  GstFlowReturn ret;

  fs = GST_ALPHAMIX (gst_pad_get_parent (pad));

  GST_DEBUG_OBJECT (fs, "chain test");

  //////////////////////////////////////////////////////////////////////////////
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
  
  
  //////////////////////////////////////////////////////////////////////////////
  buffer_ref = fs->buffer_ref;
  fs->buffer_ref = NULL;
  g_cond_signal (fs->cond);  
  g_mutex_unlock (fs->lock);

  //////////////////////////////////////////////////////////////////////////////
  GST_DEBUG_OBJECT (fs, "merging alphas from frames");
  fs->test_img->imageData = (char*) GST_BUFFER_DATA(buffer);  
  fs->n_frames++;

  // just divide the ref frame into 4 channels, same for the test input, then merge 50/50
  // both alpha channels and reassemble the test as output
  cvSplit(fs->test_img, fs->test_ch1, fs->test_ch2, fs->test_ch3, fs->test_chA );
  cvSplit(fs->ref_img,  fs->ref_ch1,  fs->ref_ch2,  fs->ref_ch3,  fs->ref_chA );
  //cvAddWeighted(fs->test_chA, 0.5, fs->ref_chA, 0.5, 0.0, fs->test_chA);
  cvMax(fs->test_chA, fs->ref_chA, fs->test_chA);

  cvMerge(fs->test_ch1, fs->test_ch2, fs->test_ch3, fs->ref_chA, fs->test_img);

  // now push the "test" as output  
  ret = gst_pad_push (fs->srcpad, buffer);

  gst_object_unref (fs);

  return ret;
}

static gboolean
gst_alphamix_sink_event (GstPad * pad, GstEvent * event)
{
  GstALPHAMIX *fs;

  fs = GST_ALPHAMIX (gst_pad_get_parent (pad));

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
      //gst_pad_event_default (pad, event);
      break;
  }

  gst_pad_push_event (fs->srcpad, event);
  gst_object_unref (fs);

  return TRUE;
}




gboolean gst_alphamix_plugin_init(GstPlugin * plugin) {
	GST_DEBUG_CATEGORY_INIT (gst_alphamix_debug, "alphamix", 0, "Alpha channel mixing");
	return gst_element_register(plugin, "alphamix", GST_RANK_NONE, GST_TYPE_ALPHAMIX);
}
