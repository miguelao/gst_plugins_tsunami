/* GStreamer
 */

#ifndef __GST_GHOSTMAPPER_H__
#define __GST_GHOSTMAPPER_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

#include <opencv/cv.h>

G_BEGIN_DECLS

#define GST_TYPE_GHOSTMAPPER \
	(gst_ghostmapper_get_type())
#define GST_GHOSTMAPPER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GHOSTMAPPER,GstGhostmapper))
#define GST_GHOSTMAPPER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GHOSTMAPPER,GstGhostmapperClass))
#define GST_IS_GHOSTMAPPER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GHOSTMAPPER))
#define GST_IS_GHOSTMAPPER_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GHOSTMAPPER))

typedef struct _GstGhostmapper GstGhostmapper;
typedef struct _GstGhostmapperClass GstGhostmapperClass;

//////////////////////////////////////////////
typedef struct curlMemoryStruct {           //
  unsigned char *memory;                    //
  size_t size;                              //
  size_t pos;                               //
} curlMemoryStruct;                         //
                                            //
enum PICSRC_COLOR_TYPE {                    //
  COLOR_TYPE_GRAY = 0,                      //
  COLOR_TYPE_PALETTE,                       //
  COLOR_TYPE_RGB, 		            //
  COLOR_TYPE_RGB_ALPHA,                     //
  COLOR_TYPE_GRAY_ALPHA,                    //
  COLOR_TYPE_UNKNOWN                        //
};                                          //
                                            //
typedef struct picSrcImageInfo {            //
  unsigned int width;                       //
  unsigned int height;                      //
  int          channels;                    //
  int          bpp;                         //
  PICSRC_COLOR_TYPE colorspace;             //
} picSrcImageInfo;                          //
//////////////////////////////////////////////



struct _GstGhostmapper
{
  GstVideoFilter parent; /* subclass the cutout */

  /* caps */
  GStaticMutex lock;

  GstVideoFormat in_format, out_format;
  gint width, height;
  guint id;

  /* properties */
  gboolean passthrough;
  gboolean apply_cutout;
  gboolean fps;
  gboolean green;
  gint32   debug_view;
  
  /* last face info */
  gdouble x, y, w, h;
  gboolean isface;

  // png load stuff
  unsigned char *raw_image;
  picSrcImageInfo info;

  // openCv images and headers, for transformations
  IplImage            *cvImgIn;
  IplImage            *cvGhost;
  IplImage            *cvGhostBw;
  IplImage            *cvGhostBwResized;
  IplImage            *cvGhostBwAffined;

  // point arrays for the affine transform
  CvPoint2D32f srcTri[3], dstTri[3];
  CvMat* warp_mat;

  gchar* ghostfilename;

  gulong findex;
};

struct _GstGhostmapperClass {
  GstVideoFilterClass parent_class;
};

GType gst_ghostmapper_get_type (void);

gboolean gst_ghostmapper_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_GHOSTMAPPER_H__ */
