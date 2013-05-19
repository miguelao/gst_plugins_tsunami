/* GStreamer
 */

#ifndef __GST_TSM_H__
#define __GST_TSM_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>
//#include <opencv/highgui.h>

G_BEGIN_DECLS

#define GST_TYPE_TSM \
	(gst_tsm_get_type())
#define GST_TSM(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TSM,GstTsm))
#define GST_TSM_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TSM,GstTsmClass))
#define GST_IS_TSM(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TSM))
#define GST_IS_TSM_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TSM))

typedef struct _GstTsm GstTsm;
typedef struct _GstTsmClass GstTsmClass;

struct _GstTsm {
  GstVideoFilter parent;

  GStaticMutex lock;
  
  GstVideoFormat in_format, out_format;
  gint width, height, numframes;
  
  bool      display;  
  int       debug;  

  IplImage* pImageRGBA ; // 4channel input

  IplImage* pImgRGB;     // 3channel version of the 4channel input
  IplImage* pImgScratch; // 3channel scratchpad
  IplImage* pImgRGB_1;   // storage of previous pImgRGB

  IplImage* pImgGRAY;       // Gray of the RGB image
  IplImage* pImgGRAY_copy;  // Gray of the RGB image
  IplImage* pImgGRAY_diff;  // Smoothed difference between GRAY and GRAY_1
  IplImage* pImgGRAY_1;     // storage of previous pImgGRAY
  IplImage* pImgGRAY_1copy; // Gray of the RGB image
  CvMat*    pImgGRAY_flow;  //  Dense OF output flow image (32b float)

  IplImage* pImg_DOFMask;  // Mask obtained from the Dense OF
  IplImage* pImg_DOFMap;   // Map (solid image) obtained from the Dense OF
  IplImage* pImg_Codebook; // Mask obtained from the Codebook

  IplImage* pImg_DistMask;  // Distance, from Distance Transform
  IplImage* pImg_LablMask;  // Labels, from Distance Transform

  IplImage* pImgEDGE;     // Edge of the GRAY image
  IplImage* pImgEDGE_1;   // storage of previous pImgEDGE

  IplImage* pImgChA;     // Alpha channel of the input
  IplImage* pImgCh1;
  IplImage* pImgCh2;
  IplImage* pImgCh3;

  // sparse optical flow stuff
  int           num_corners;
  CvPoint2D32f* cornersA;
  CvPoint2D32f* cornersB;
  IplImage*     cornersB_asMat;  // OF features as a Matrix
  char*         corners_found;//[ MAX_CORNERS ];
  float*        corners_error;//[ MAX_CORNERS ];

  // mask merging stuff
  CvMat*        dictionary;     // label index (row) to (x,y)(columns) coordinate
  CvMemStorage* dof_memstorage;
  CvSeq*        dof_contours;

  IplImage*     pImg_MergedMask;  // Merged DOF and Sparse OF result
  CvSeq*        mergedof_contours;// for the contours on the merged mask
  IplImage*     pImg_FinalMask;  // Merged DOF and Sparse OF result

  // TSM stuff
  // block-based avg and deviation
  int        blocksize;
  CvMat*     blockstat_avg; // average
  CvMat*     blockstat_dev; // and deviation of a block of pixels
  CvMat*     blockstat_avg_1; // same avg and dev but
  CvMat*     blockstat_dev_1; // for the previous frame
  IplImage*  pImg_TSM;        // IplImage to compose the TSM
  CvMat*     pImg_PFG;        // "Previous FG" labels
  CvMat*     pImg_TSM_small;  // TSM in blocks


  CvMat*     grabcut_mask; // mask created by graphcut
  CvRect     bbox_prev;
  CvRect     bbox_now;
  CvRect     facepos;
  CvSubdiv2D* subdiv;
  CvMemStorage* subdivstorage;
};

struct _GstTsmClass {
  GstVideoFilterClass parent_class;
};

GType gst_tsm_get_type(void);

G_END_DECLS

gboolean gst_tsm_plugin_init(GstPlugin * plugin);

#endif /* __GST_TSM_H__ */
