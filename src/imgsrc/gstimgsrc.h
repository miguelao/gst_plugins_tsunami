/* GStreamer
 */

#ifndef __GST_IMG_SRC_H__
#define __GST_IMG_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <opencv2/highgui/highgui.hpp>

G_BEGIN_DECLS

#define GST_TYPE_IMG_SRC \
  (gst_img_src_get_type())
#define GST_IMG_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_IMG_SRC,GstImgSrc))
#define GST_IMG_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_IMG_SRC,GstImgSrcClass))
#define GST_IS_IMG_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_IMG_SRC))
#define GST_IS_IMG_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_IMG_SRC))

/**
 * GstImgSrcPattern:
 * @GST_IMG_SRC_SMPTE: A standard SMPTE test pattern
 * @GST_IMG_SRC_SNOW: Random noise
 * @GST_IMG_SRC_BLACK: A black image
 * @GST_IMG_SRC_WHITE: A white image
 * @GST_IMG_SRC_RED: A red image
 * @GST_IMG_SRC_GREEN: A green image
 * @GST_IMG_SRC_BLUE: A blue image
 * @GST_IMG_SRC_CHECKERS1: Checkers pattern (1px)
 * @GST_IMG_SRC_CHECKERS2: Checkers pattern (2px)
 * @GST_IMG_SRC_CHECKERS4: Checkers pattern (4px)
 * @GST_IMG_SRC_CHECKERS8: Checkers pattern (8px)
 * @GST_IMG_SRC_CIRCULAR: Circular pattern
 * @GST_IMG_SRC_BLINK: Alternate between black and white
 * @GST_IMG_SRC_SMPTE75: SMPTE test pattern (75% color bars)
 * @GST_IMG_SRC_ZONE_PLATE: Zone plate
 * @GST_IMG_SRC_GAMUT: Gamut checking pattern
 * @GST_IMG_SRC_CHROMA_ZONE_PLATE: Chroma zone plate
 * @GST_IMG_SRC_BALL: Moving ball
 * @GST_IMG_SRC_SMPTE100: SMPTE test pattern (100% color bars)
 * @GST_IMG_SRC_BAR: Bar with foreground color
 *
 * The test pattern to produce.
 *
 * The Gamut pattern creates a checkerboard pattern of colors at the
 * edge of the YCbCr gamut and nearby colors that are out of gamut.
 * The pattern is divided into 4 regions: black, white, red, and blue.
 * After conversion to RGB, the out-of-gamut colors should be converted
 * to the same value as their in-gamut neighbors.  If the checkerboard
 * pattern is still visible after conversion, this indicates a faulty
 * conversion.  Image manipulation, such as adjusting contrast or
 * brightness, can also cause the pattern to be visible.
 *
 * The Zone Plate pattern is based on BBC R&D Report 1978/23, and can
 * be used to test spatial frequency response of a system.  This
 * pattern generator is controlled by the xoffset and yoffset parameters
 * and also by all the parameters starting with 'k'.  The default
 * parameters produce a grey pattern.  Try 'imgsrc
 * pattern=zone-plate kx2=20 ky2=20 kt=1' to produce something
 * interesting.
 */
typedef enum {
  GST_IMG_SRC_SMPTE,
  GST_IMG_SRC_SNOW,
  GST_IMG_SRC_BLACK,
  GST_IMG_SRC_WHITE,
  GST_IMG_SRC_RED,
  GST_IMG_SRC_GREEN,
  GST_IMG_SRC_BLUE,
  GST_IMG_SRC_CHECKERS1,
  GST_IMG_SRC_CHECKERS2,
  GST_IMG_SRC_CHECKERS4,
  GST_IMG_SRC_CHECKERS8,
  GST_IMG_SRC_CIRCULAR,
  GST_IMG_SRC_BLINK,
  GST_IMG_SRC_SMPTE75,
  GST_IMG_SRC_ZONE_PLATE,
  GST_IMG_SRC_GAMUT,
  GST_IMG_SRC_CHROMA_ZONE_PLATE,
  GST_IMG_SRC_SOLID,
  GST_IMG_SRC_BALL,
  GST_IMG_SRC_SMPTE100,
  GST_IMG_SRC_BAR
} GstImgSrcPattern;

/**
 * GstImgSrcColorSpec:
 * @GST_IMG_SRC_BT601: ITU-R Rec. BT.601/BT.470 (SD)
 * @GST_IMG_SRC_BT709: ITU-R Rec. BT.709 (HD)
 *
 * The color specification to use.
 */
typedef enum {
  GST_IMG_SRC_BT601,
  GST_IMG_SRC_BT709
} GstImgSrcColorSpec;

typedef struct _GstImgSrc GstImgSrc;
typedef struct _GstImgSrcClass GstImgSrcClass;

/**
 * GstImgSrc:
 *
 * Opaque data structure.
 */
struct _GstImgSrc {
  GstPushSrc element;

  /*< private >*/

  /* type of output */
  GstImgSrcPattern pattern_type;

  /* Color spec of output */
  GstImgSrcColorSpec color_spec;

  // image
  gchar    *filename;
  gboolean new_file;
  IplImage *img;

  /* video state */
  char *format_name;
  gint width;
  gint height;
  struct fourcc_list_struct *fourcc;
  gint bpp;
  gint rate_numerator;
  gint rate_denominator;

  /* private */
  gint64 timestamp_offset;              /* base offset */
  GstClockTime running_time;            /* total running time */
  gint64 n_frames;                      /* total frames sent */
  gboolean peer_alloc;

  /* zoneplate */
  gint k0;
  gint kx;
  gint ky;
  gint kt;
  gint kxt;
  gint kyt;
  gint kxy;
  gint kx2;
  gint ky2;
  gint kt2;
  gint xoffset;
  gint yoffset;

  /* solid color */
  guint foreground_color;
  guint background_color;

  /* moving color bars */
  gint horizontal_offset;
  gint horizontal_speed;

  void (*make_image) (GstImgSrc *v, unsigned char *dest, int w, int h);

  /* temporary AYUV/ARGB scanline */
  guint8 *tmpline_u8;
  guint8 *tmpline;
  guint8 *tmpline2;
};

struct _GstImgSrcClass {
  GstPushSrcClass parent_class;
};

GType gst_img_src_get_type (void);
gboolean gst_img_src_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif /* __GST_IMG_SRC_H__ */
