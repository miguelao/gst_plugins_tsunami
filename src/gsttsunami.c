

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gst/gst.h>

#include "opencv/gstpyrlk.h"////////////////////////////////////////////////////////////////////////
#include "opencv/gsthisteq.h"///////////////////////////////////////////////////////////////////////
#include "opencv/gstcodebookfgbg.h"/////////////////////////////////////////////////////////////////
#include "opencv/gstskin.h"/////////////////////////////////////////////////////////////////////////
#include "opencv/gstcontours.h"/////////////////////////////////////////////////////////////////////
#include "opencv/gstdilate.h"///////////////////////////////////////////////////////////////////////
#include "opencv/gsterode.h"////////////////////////////////////////////////////////////////////////
#include "opencv/gstmorphology.h"///////////////////////////////////////////////////////////////////
#include "opencv/gstalphamix.h"/////////////////////////////////////////////////////////////////////
#include "vibe/gstvibe.h"///////////////////////////////////////////////////////////////////////////
#include "opencv/gsttsm.h"//////////////////////////////////////////////////////////////////////////
#include "opencv/gstgcs.h"//////////////////////////////////////////////////////////////////////////
#include "retinex/gstretinex.h"/////////////////////////////////////////////////////////////////////
#include "opencv/gstgc.h"///////////////////////////////////////////////////////////////////////////
#include "opencv/gstsnakes.h"///////////////////////////////////////////////////////////////////////
#include "opencv/gstdraweventbox.h"/////////////////////////////////////////////////////////////////

#include "facetracking/gstfacetracker3.h"///////////////////////////////////////////////////////////
#include "bl_facedetector/gstfacedetector.h"
#include "ipp_h264/gsth264enc.h"

#include "ghostmapper/gstghostmapper.h"
#include "imgsrc/gstimgsrc.h"
#include "blockanalysis/gstblockanalysis.h"
#include "ssim/gstssim2.h"
#include "gstfacetracker/gstfacetracker.h"
#include "pixelate/gstpixelate.h"
#include "templatematch/gsttemplatematch.h"
#include "objecttracker/gstobjecttracker.h"

static gboolean plugin_init (GstPlugin * plugin)
{
  // Image adjust plugin                                                                     //////
  if (!gst_pyrlk_plugin_init(plugin))                                                        //////
    return FALSE;                                                                            //////
  // Hist Eq CV plugin                                                                       //////
  if (!gst_histeq_plugin_init(plugin))                                                       //////
    return FALSE;                                                                            //////
  // Image movement detection plugin                                                         //////
  if (!gst_codebookfgbg_plugin_init(plugin))                                                 //////
    return FALSE;                                                                            //////
  // Skin detection plugin                                                                   //////
  if (!gst_skin_plugin_init(plugin))                                                         //////
    return FALSE;                                                                            //////
  // Contours detection plugin                                                               //////
  if (!gst_contours_plugin_init(plugin))                                                     //////
    return FALSE;                                                                            //////
  // opencv dilate plugin                                                                    //////
  //if (!gst_dilate_plugin_init(plugin))                                                       //////
  //  return FALSE;                                                                            //////
  // opencv erode plugin                                                                     //////
  //if (!gst_erode_plugin_init(plugin))                                                        //////
  //  return FALSE;                                                                            //////
  // opencv morphology plugin                                                                //////
  if (!gst_morphology_plugin_init(plugin))                                                   //////
    return FALSE;                                                                            //////
  // Alpha mix plugin                                                                        //////
  if (!gst_alphamix_plugin_init(plugin))                                                     //////
    return FALSE;                                                                            //////
  // Vibe motion detection plugin                                                            //////
  if (!gst_vibe_plugin_init(plugin))                                                         //////
    return FALSE;                                                                            //////
  // TSM segmentation plugin                                                                 //////
  //if (!gst_tsm_plugin_init(plugin))                                                          //////
  //  return FALSE;                                                                            //////
  //  gcs plugin                                                                               //////
  if (!gst_gcs_plugin_init(plugin))                                                          //////
    return FALSE;                                                                            //////
  //  retinex plugin                                                                         //////
  if (!gst_retinex_plugin_init(plugin))                                                      //////
    return FALSE;                                                                            //////
  //  gc plugin                                                                              //////
  if (!gst_gc_plugin_init(plugin))                                                           //////
    return FALSE;                                                                            //////
  // Snake calcualtion plugin                                                                //////
  if (!gst_snakes_plugin_init(plugin))                                                       //////
    return FALSE;                                                                            //////
  // Draw box plugin                                                                         //////
  if (!gst_draweventbox_plugin_init(plugin))                                                 //////
    return FALSE;                                                                            //////


  // Face tracker3 plugin                                                                    //////
  if (!gst_facetracker3_plugin_init(plugin))                                                 //////
    return FALSE;                                                                            //////
  // BL face detector init
  if (!gst_facedetector_plugin_init(plugin))
    return FALSE;
  // IPP based H264 encoder
  if (!plugin_init_ipp_h264(plugin))
    return FALSE;

  // ghostmapper (poor man's cutout) init
  if (!gst_ghostmapper_plugin_init(plugin))
    return FALSE;
  // imgsrc
  if (!gst_img_src_plugin_init(plugin))
    return FALSE;
  // blockanalysis
  if (!gst_blockanalysis_plugin_init(plugin))
    return FALSE;
  // ssim2
  if (!gst_ssim2_plugin_init(plugin))
    return FALSE;
  // facetracker
  if (!gst_facetracker_plugin_init(plugin))
    return FALSE;
  // pixelate
  if (!gst_pixelate_plugin_init(plugin))
    return FALSE;
  // template match
  if (!gst_template_match_plugin_init(plugin))
    return FALSE;
  // Object tracker
  if (!gst_objecttracker_plugin_init(plugin))
    return FALSE;


  return TRUE;
}

GST_PLUGIN_DEFINE ( GST_VERSION_MAJOR, GST_VERSION_MINOR,
    "tsunami", "Tsunami (experimental/unstable) Plugin",
    plugin_init, VERSION, "LGPL", "GStreamer", "http://www.mosami.com/")

