/* GStreamer
 */

#ifndef __IMG_SRC_H__
#define __IMG_SRC_H__

#include <glib.h>

enum {
  VTS_YUV,
  VTS_RGB,
  VTS_GRAY,
  VTS_BAYER
};

enum {
  VTS_BAYER_BGGR,
  VTS_BAYER_RGGB,
  VTS_BAYER_GRBG,
  VTS_BAYER_GBRG
};

struct vts_color_struct {
  guint8 Y, U, V, A;
  guint8 R, G, B;
  guint16 gray;
};


typedef struct paintinfo_struct paintinfo;
struct paintinfo_struct
{
  unsigned char *dest;          /* pointer to first byte of video data */
  unsigned char *yp, *up, *vp;  /* pointers to first byte of each component
                                 * for both packed/planar YUV and RGB */
  unsigned char *ap;            /* pointer to first byte of alpha component */
  unsigned char *endptr;        /* pointer to byte beyond last video data */
  int ystride;
  int ustride;
  int vstride;
  int width;
  int height;
  const struct vts_color_struct *colors;
  const struct vts_color_struct *color;
  /*  const struct vts_color_struct *color; */
  void (*paint_hline) (paintinfo * p, int x, int y, int w);
  void (*paint_tmpline) (paintinfo * p, int x, int w);
  void (*convert_tmpline) (paintinfo * p, int y);
  int x_offset;

  int bayer_x_invert;
  int bayer_y_invert;

  guint8 *tmpline;
  guint8 *tmpline2;
  guint8 *tmpline_u8;

  struct vts_color_struct foreground_color;
  struct vts_color_struct background_color;
};

struct fourcc_list_struct
{
  int type;
  const char *fourcc;
  const char *name;
  int bitspp;
  void (*paint_setup) (paintinfo * p, unsigned char *dest);
  void (*convert_hline) (paintinfo * p, int y);
  int depth;
  unsigned int red_mask;
  unsigned int green_mask;
  unsigned int blue_mask;
  unsigned int alpha_mask;
};

struct fourcc_list_struct *
        paintrect_find_fourcc           (int find_fourcc);
struct fourcc_list_struct *
        paintrect_find_name             (const char *name);
struct fourcc_list_struct *
        paintinfo_find_by_structure     (const GstStructure *structure);
GstStructure *
        paint_get_structure             (struct fourcc_list_struct *format);
int     gst_img_src_get_size     (GstImgSrc * v, int w, int h);

extern struct fourcc_list_struct fourcc_list[];
extern int n_fourccs;

#endif
