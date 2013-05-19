/* GStreamer
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* non-GST-specific stuff */

#include "gstimgsrc.h"
#include "imgsrc.h"
//#include "gstimgsrcorc.h"

#include <gst/math-compat.h>

#include <string.h>
#include <stdlib.h>

static void paint_tmpline_ARGB (paintinfo * p, int x, int w);
static void paint_tmpline_AYUV (paintinfo * p, int x, int w);

static unsigned char
random_char (void)
{
  static unsigned int state;

  state *= 1103515245;
  state += 12345;
  return (state >> 16) & 0xff;
}

enum
{
  COLOR_WHITE = 0,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_GREEN,
  COLOR_MAGENTA,
  COLOR_RED,
  COLOR_BLUE,
  COLOR_BLACK,
  COLOR_NEG_I,
  COLOR_POS_Q,
  COLOR_SUPER_BLACK,
  COLOR_DARK_GREY
};

static const struct vts_color_struct vts_colors_bt709_ycbcr_100[] = {
  {235, 128, 128, 255, 255, 255, 255, (235 << 8)},
  {219, 16, 138, 255, 255, 255, 0, (219 << 8)},
  {188, 154, 16, 255, 0, 255, 255, (188 < 8)},
  {173, 42, 26, 255, 0, 255, 0, (173 << 8)},
  {78, 214, 230, 255, 255, 0, 255, (78 << 8)},
  {63, 102, 240, 255, 255, 0, 0, (64 << 8)},
  {32, 240, 118, 255, 0, 0, 255, (32 << 8)},
  {16, 128, 128, 255, 0, 0, 0, (16 << 8)},
  {16, 198, 21, 255, 0, 0, 128, (16 << 8)},     /* -I ? */
  {16, 235, 198, 255, 0, 128, 255, (16 << 8)},  /* +Q ? */
  {0, 128, 128, 255, 0, 0, 0, 0},
  {32, 128, 128, 255, 19, 19, 19, (32 << 8)},
};

static const struct vts_color_struct vts_colors_bt709_ycbcr_75[] = {
  {180, 128, 128, 255, 191, 191, 191, (180 << 8)},
  {168, 44, 136, 255, 191, 191, 0, (168 << 8)},
  {145, 147, 44, 255, 0, 191, 191, (145 << 8)},
  {133, 63, 52, 255, 0, 191, 0, (133 << 8)},
  {63, 193, 204, 255, 191, 0, 191, (63 << 8)},
  {51, 109, 212, 255, 191, 0, 0, (51 << 8)},
  {28, 212, 120, 255, 0, 0, 191, (28 << 8)},
  {16, 128, 128, 255, 0, 0, 0, (16 << 8)},
  {16, 198, 21, 255, 0, 0, 128, (16 << 8)},     /* -I ? */
  {16, 235, 198, 255, 0, 128, 255, (16 << 8)},  /* +Q ? */
  {0, 128, 128, 255, 0, 0, 0, 0},
  {32, 128, 128, 255, 19, 19, 19, (32 << 8)},
};

static const struct vts_color_struct vts_colors_bt601_ycbcr_100[] = {
  {235, 128, 128, 255, 255, 255, 255, (235 << 8)},
  {210, 16, 146, 255, 255, 255, 0, (219 << 8)},
  {170, 166, 16, 255, 0, 255, 255, (188 < 8)},
  {145, 54, 34, 255, 0, 255, 0, (173 << 8)},
  {106, 202, 222, 255, 255, 0, 255, (78 << 8)},
  {81, 90, 240, 255, 255, 0, 0, (64 << 8)},
  {41, 240, 110, 255, 0, 0, 255, (32 << 8)},
  {16, 128, 128, 255, 0, 0, 0, (16 << 8)},
  {16, 198, 21, 255, 0, 0, 128, (16 << 8)},     /* -I ? */
  {16, 235, 198, 255, 0, 128, 255, (16 << 8)},  /* +Q ? */
  {-0, 128, 128, 255, 0, 0, 0, 0},
  {32, 128, 128, 255, 19, 19, 19, (32 << 8)},
};

static const struct vts_color_struct vts_colors_bt601_ycbcr_75[] = {
  {180, 128, 128, 255, 191, 191, 191, (180 << 8)},
  {162, 44, 142, 255, 191, 191, 0, (168 << 8)},
  {131, 156, 44, 255, 0, 191, 191, (145 << 8)},
  {112, 72, 58, 255, 0, 191, 0, (133 << 8)},
  {84, 184, 198, 255, 191, 0, 191, (63 << 8)},
  {65, 100, 212, 255, 191, 0, 0, (51 << 8)},
  {35, 212, 114, 255, 0, 0, 191, (28 << 8)},
  {16, 128, 128, 255, 0, 0, 0, (16 << 8)},
  {16, 198, 21, 255, 0, 0, 128, (16 << 8)},     /* -I ? */
  {16, 235, 198, 255, 0, 128, 255, (16 << 8)},  /* +Q ? */
  {-0, 128, 128, 255, 0, 0, 0, 0},
  {32, 128, 128, 255, 19, 19, 19, (32 << 8)},
};


static void paint_setup_I420 (paintinfo * p, unsigned char *dest);
static void paint_setup_YV12 (paintinfo * p, unsigned char *dest);
static void paint_setup_YUY2 (paintinfo * p, unsigned char *dest);
static void paint_setup_UYVY (paintinfo * p, unsigned char *dest);
static void paint_setup_YVYU (paintinfo * p, unsigned char *dest);
#ifdef disabled
static void paint_setup_IYU2 (paintinfo * p, unsigned char *dest);
#endif
static void paint_setup_Y41B (paintinfo * p, unsigned char *dest);
static void paint_setup_Y42B (paintinfo * p, unsigned char *dest);
static void paint_setup_Y444 (paintinfo * p, unsigned char *dest);
static void paint_setup_Y800 (paintinfo * p, unsigned char *dest);
static void paint_setup_AYUV (paintinfo * p, unsigned char *dest);
static void paint_setup_v308 (paintinfo * p, unsigned char *dest);
static void paint_setup_NV12 (paintinfo * p, unsigned char *dest);
static void paint_setup_NV21 (paintinfo * p, unsigned char *dest);
#ifdef disabled
static void paint_setup_v410 (paintinfo * p, unsigned char *dest);
#endif
static void paint_setup_v216 (paintinfo * p, unsigned char *dest);
static void paint_setup_v210 (paintinfo * p, unsigned char *dest);
static void paint_setup_UYVP (paintinfo * p, unsigned char *dest);

static void paint_setup_YUV9 (paintinfo * p, unsigned char *dest);
static void paint_setup_YVU9 (paintinfo * p, unsigned char *dest);
static void paint_setup_ARGB8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_ABGR8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_RGBA8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_BGRA8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_xRGB8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_xBGR8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_RGBx8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_BGRx8888 (paintinfo * p, unsigned char *dest);
static void paint_setup_RGB888 (paintinfo * p, unsigned char *dest);
static void paint_setup_BGR888 (paintinfo * p, unsigned char *dest);
static void paint_setup_RGB565 (paintinfo * p, unsigned char *dest);
static void paint_setup_xRGB1555 (paintinfo * p, unsigned char *dest);

static void paint_setup_bayer_bggr (paintinfo * p, unsigned char *dest);
static void paint_setup_bayer_rggb (paintinfo * p, unsigned char *dest);
static void paint_setup_bayer_gbrg (paintinfo * p, unsigned char *dest);
static void paint_setup_bayer_grbg (paintinfo * p, unsigned char *dest);

static void convert_hline_I420 (paintinfo * p, int y);
static void convert_hline_NV12 (paintinfo * p, int y);
static void convert_hline_NV21 (paintinfo * p, int y);
static void convert_hline_YUY2 (paintinfo * p, int y);
#ifdef disabled
static void convert_hline_IYU2 (paintinfo * p, int y);
#endif
static void convert_hline_Y41B (paintinfo * p, int y);
static void convert_hline_Y42B (paintinfo * p, int y);
static void convert_hline_Y444 (paintinfo * p, int y);
static void convert_hline_Y800 (paintinfo * p, int y);
static void convert_hline_v308 (paintinfo * p, int y);
static void convert_hline_AYUV (paintinfo * p, int y);
#ifdef disabled
static void convert_hline_v410 (paintinfo * p, int y);
#endif
static void convert_hline_v216 (paintinfo * p, int y);
static void convert_hline_v210 (paintinfo * p, int y);
static void convert_hline_UYVP (paintinfo * p, int y);

static void convert_hline_YUV9 (paintinfo * p, int y);
static void convert_hline_astr4 (paintinfo * p, int y);
static void convert_hline_str4 (paintinfo * p, int y);
static void convert_hline_str3 (paintinfo * p, int y);
static void convert_hline_RGB565 (paintinfo * p, int y);
static void convert_hline_xRGB1555 (paintinfo * p, int y);

static void convert_hline_bayer (paintinfo * p, int y);

static void paint_setup_GRAY8 (paintinfo * p, unsigned char *dest);
static void paint_setup_GRAY16 (paintinfo * p, unsigned char *dest);
static void convert_hline_GRAY8 (paintinfo * p, int y);
static void convert_hline_GRAY16 (paintinfo * p, int y);

struct fourcc_list_struct fourcc_list[] = {
/* packed */
  {VTS_YUV, "YUY2", "YUY2", 16, paint_setup_YUY2, convert_hline_YUY2},
  {VTS_YUV, "UYVY", "UYVY", 16, paint_setup_UYVY, convert_hline_YUY2},
#ifdef disabled
  {VTS_YUV, "Y422", "Y422", 16, paint_setup_UYVY, convert_hline_YUY2},
  {VTS_YUV, "UYNV", "UYNV", 16, paint_setup_UYVY, convert_hline_YUY2},  /* FIXME: UYNV? */
#endif
  {VTS_YUV, "YVYU", "YVYU", 16, paint_setup_YVYU, convert_hline_YUY2},
  {VTS_YUV, "v308", "v308", 24, paint_setup_v308, convert_hline_v308},
  {VTS_YUV, "AYUV", "AYUV", 32, paint_setup_AYUV, convert_hline_AYUV},
#ifdef disabled
  {VTS_YUV, "v410", "v410", 32, paint_setup_v410, convert_hline_v410},
#endif
  {VTS_YUV, "v210", "v210", 21, paint_setup_v210, convert_hline_v210},
  {VTS_YUV, "v216", "v216", 32, paint_setup_v216, convert_hline_v216},
  {VTS_YUV, "UYVP", "UYVP", 20, paint_setup_UYVP, convert_hline_UYVP},

#ifdef disabled
  {VTS_YUV, "IYU2", "IYU2", 24, paint_setup_IYU2, convert_hline_IYU2},
#endif

/* planar */
  /* YVU9 */
  {VTS_YUV, "YVU9", "YVU9", 9, paint_setup_YVU9, convert_hline_YUV9},
  /* YUV9 */
  {VTS_YUV, "YUV9", "YUV9", 9, paint_setup_YUV9, convert_hline_YUV9},
  /* IF09 */
  /* YV12 */
  {VTS_YUV, "YV12", "YV12", 12, paint_setup_YV12, convert_hline_I420},
  /* I420 */
  {VTS_YUV, "I420", "I420", 12, paint_setup_I420, convert_hline_I420},
  /* NV12 */
  {VTS_YUV, "NV12", "NV12", 12, paint_setup_NV12, convert_hline_NV12},
  /* NV21 */
  {VTS_YUV, "NV21", "NV21", 12, paint_setup_NV21, convert_hline_NV21},
  /* CLPL */
  /* Y41B */
  {VTS_YUV, "Y41B", "Y41B", 12, paint_setup_Y41B, convert_hline_Y41B},
  /* Y42B */
  {VTS_YUV, "Y42B", "Y42B", 16, paint_setup_Y42B, convert_hline_Y42B},
  /* Y444 */
  {VTS_YUV, "Y444", "Y444", 24, paint_setup_Y444, convert_hline_Y444},
  /* Y800 grayscale */
  {VTS_YUV, "Y800", "Y800", 8, paint_setup_Y800, convert_hline_Y800},

  /* Not exactly YUV but it's the same as above */
  {VTS_GRAY, "GRAY8", "GRAY8", 8, paint_setup_GRAY8, convert_hline_GRAY8},
  {VTS_GRAY, "GRAY16", "GRAY16", 16, paint_setup_GRAY16, convert_hline_GRAY16},

  {VTS_RGB, "RGB ", "xRGB8888", 32, paint_setup_xRGB8888, convert_hline_str4,
        24,
      0x00ff0000, 0x0000ff00, 0x000000ff},
  {VTS_RGB, "RGB ", "xBGR8888", 32, paint_setup_xBGR8888, convert_hline_str4,
        24,
      0x000000ff, 0x0000ff00, 0x00ff0000},
  {VTS_RGB, "RGB ", "RGBx8888", 32, paint_setup_RGBx8888, convert_hline_str4,
        24,
      0xff000000, 0x00ff0000, 0x0000ff00},
  {VTS_RGB, "RGB ", "BGRx8888", 32, paint_setup_BGRx8888, convert_hline_str4,
        24,
      0x0000ff00, 0x00ff0000, 0xff000000},
  {VTS_RGB, "RGB ", "ARGB8888", 32, paint_setup_ARGB8888, convert_hline_astr4,
        32,
      0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000},
  {VTS_RGB, "RGB ", "ABGR8888", 32, paint_setup_ABGR8888, convert_hline_astr4,
        32,
      0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000},
  {VTS_RGB, "RGB ", "RGBA8888", 32, paint_setup_RGBA8888, convert_hline_astr4,
        32,
      0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff},
  {VTS_RGB, "RGB ", "BGRA8888", 32, paint_setup_BGRA8888, convert_hline_astr4,
        32,
      0x0000ff00, 0x00ff0000, 0xff000000, 0x000000ff},
  {VTS_RGB, "RGB ", "RGB888", 24, paint_setup_RGB888, convert_hline_str3, 24,
      0x00ff0000, 0x0000ff00, 0x000000ff},
  {VTS_RGB, "RGB ", "BGR888", 24, paint_setup_BGR888, convert_hline_str3, 24,
      0x000000ff, 0x0000ff00, 0x00ff0000},
  {VTS_RGB, "RGB ", "RGB565", 16, paint_setup_RGB565, convert_hline_RGB565, 16,
      0x0000f800, 0x000007e0, 0x0000001f},
  {VTS_RGB, "RGB ", "xRGB1555", 16, paint_setup_xRGB1555,
        convert_hline_xRGB1555,
        15,
      0x00007c00, 0x000003e0, 0x0000001f},

  {VTS_BAYER, "bggr", "Bayer", 8, paint_setup_bayer_bggr, convert_hline_bayer},
  {VTS_BAYER, "rggb", "Bayer", 8, paint_setup_bayer_rggb, convert_hline_bayer},
  {VTS_BAYER, "grbg", "Bayer", 8, paint_setup_bayer_grbg, convert_hline_bayer},
  {VTS_BAYER, "gbrg", "Bayer", 8, paint_setup_bayer_gbrg, convert_hline_bayer}
};

int n_fourccs = G_N_ELEMENTS (fourcc_list);

struct fourcc_list_struct *
paintinfo_find_by_structure (const GstStructure * structure)
{
  int i;
  const char *media_type = gst_structure_get_name (structure);
  int ret;

  g_return_val_if_fail (structure, NULL);

  if (strcmp (media_type, "video/x-raw-gray") == 0) {
    gint bpp, depth, endianness = 0;

    ret = gst_structure_get_int (structure, "bpp", &bpp) &&
        gst_structure_get_int (structure, "depth", &depth);
    if (!ret || bpp != depth || (depth != 8 && depth != 16))
      return NULL;

    ret = gst_structure_get_int (structure, "endianness", &endianness);
    if ((!ret || endianness != G_BYTE_ORDER) && bpp == 16)
      return NULL;

    for (i = 0; i < n_fourccs; i++) {
      if (fourcc_list[i].type == VTS_GRAY && fourcc_list[i].bitspp == bpp) {
        return fourcc_list + i;
      }
    }
  } else if (strcmp (media_type, "video/x-raw-yuv") == 0) {
    const char *s;
    int fourcc;
    guint32 format;

    ret = gst_structure_get_fourcc (structure, "format", &format);
    if (!ret)
      return NULL;
    for (i = 0; i < n_fourccs; i++) {
      s = fourcc_list[i].fourcc;
      /* g_print("testing %" GST_FOURCC_FORMAT " and %s\n", GST_FOURCC_ARGS(format), s); */
      fourcc = GST_MAKE_FOURCC (s[0], s[1], s[2], s[3]);
      if (fourcc_list[i].type == VTS_YUV && fourcc == (int)format) {
        return fourcc_list + i;
      }
    }
  } else if (strcmp (media_type, "video/x-raw-rgb") == 0) {
    int red_mask;
    int green_mask;
    int blue_mask;
    int alpha_mask;
    int depth;
    int bpp;

    ret = gst_structure_get_int (structure, "red_mask", &red_mask);
    ret &= gst_structure_get_int (structure, "green_mask", &green_mask);
    ret &= gst_structure_get_int (structure, "blue_mask", &blue_mask);
    ret &= gst_structure_get_int (structure, "depth", &depth);
    ret &= gst_structure_get_int (structure, "bpp", &bpp);

    if (depth == 32) {
      ret &= gst_structure_get_int (structure, "alpha_mask", &alpha_mask);
      ret &= (alpha_mask != 0);
    } else {
      alpha_mask = 0;
    }

    if (!ret) {
      GST_WARNING ("incomplete caps structure: %" GST_PTR_FORMAT, structure);
      return NULL;
    }

    for (i = 0; i < n_fourccs; i++) {
      if (fourcc_list[i].type == VTS_RGB &&
          fourcc_list[i].red_mask == (guint32)red_mask &&
          fourcc_list[i].green_mask == (guint32)green_mask &&
          fourcc_list[i].blue_mask == (guint32)blue_mask &&
          (alpha_mask == 0 || fourcc_list[i].alpha_mask == (guint32)alpha_mask) &&
          fourcc_list[i].depth == depth && fourcc_list[i].bitspp == bpp) {
        return fourcc_list + i;
      }
    }
    return NULL;
  } else if (strcmp (media_type, "video/x-raw-bayer") == 0) {
    const gchar *format;

    format = gst_structure_get_string (structure, "format");
    if (!format) {
      GST_WARNING ("incomplete caps structure: %" GST_PTR_FORMAT, structure);
      return NULL;
    }

    for (i = 0; i < n_fourccs; i++) {
      if (fourcc_list[i].type == VTS_BAYER &&
          g_str_equal (format, fourcc_list[i].fourcc)) {
        return fourcc_list + i;
      }
    }
    return NULL;
  }

  g_critical ("format not found for media type %s", media_type);

  return NULL;
}

struct fourcc_list_struct *
paintrect_find_fourcc (int find_fourcc)
{
  int i;

  for (i = 0; i < n_fourccs; i++) {
    const char *s;
    int fourcc;

    s = fourcc_list[i].fourcc;
    fourcc = GST_MAKE_FOURCC (s[0], s[1], s[2], s[3]);
    if (find_fourcc == fourcc) {
      /* If YUV format, it's good */
      if (!fourcc_list[i].type == VTS_YUV) {
        return fourcc_list + i;
      }

      return fourcc_list + i;
    }
  }
  return NULL;
}

struct fourcc_list_struct *
paintrect_find_name (const char *name)
{
  int i;

  for (i = 0; i < n_fourccs; i++) {
    if (strcmp (name, fourcc_list[i].name) == 0) {
      return fourcc_list + i;
    }
  }
  return NULL;
}


GstStructure *
paint_get_structure (struct fourcc_list_struct * format)
{
  GstStructure *structure = NULL;
  unsigned int fourcc;
  int endianness;

  g_return_val_if_fail (format, NULL);

  fourcc =
      GST_MAKE_FOURCC (format->fourcc[0], format->fourcc[1], format->fourcc[2],
      format->fourcc[3]);

  switch (format->type) {
    case VTS_RGB:
      if (format->bitspp == 16) {
        endianness = G_BYTE_ORDER;
      } else {
        endianness = G_BIG_ENDIAN;
      }
      structure = gst_structure_new ("video/x-raw-rgb",
          "bpp", G_TYPE_INT, format->bitspp,
          "endianness", G_TYPE_INT, endianness,
          "depth", G_TYPE_INT, format->depth,
          "red_mask", G_TYPE_INT, format->red_mask,
          "green_mask", G_TYPE_INT, format->green_mask,
          "blue_mask", G_TYPE_INT, format->blue_mask, NULL);
      if (format->depth == 32 && format->alpha_mask > 0) {
        gst_structure_set (structure, "alpha_mask", G_TYPE_INT,
            format->alpha_mask, NULL);
      }
      break;
    case VTS_GRAY:
      structure = gst_structure_new ("video/x-raw-gray",
          "bpp", G_TYPE_INT, format->bitspp, "depth", G_TYPE_INT,
          format->bitspp, NULL);
      if (format->bitspp == 16)
        gst_structure_set (structure, "endianness", G_TYPE_INT, G_BYTE_ORDER,
            NULL);
      break;
    case VTS_YUV:
    {
      GValue value_list = { 0 };
      GValue value = { 0 };

      structure = gst_structure_new ("video/x-raw-yuv",
          "format", GST_TYPE_FOURCC, fourcc, NULL);

      if (fourcc != GST_STR_FOURCC ("Y800")) {
        g_value_init (&value_list, GST_TYPE_LIST);

        g_value_init (&value, G_TYPE_STRING);
        g_value_set_static_string (&value, "sdtv");
        gst_value_list_append_value (&value_list, &value);

        g_value_set_static_string (&value, "hdtv");
        gst_value_list_append_value (&value_list, &value);

        gst_structure_set_value (structure, "color-matrix", &value_list);
        g_value_reset (&value_list);

        if (fourcc != GST_STR_FOURCC ("AYUV") &&
            fourcc != GST_STR_FOURCC ("v308") &&
            fourcc != GST_STR_FOURCC ("v410") &&
            fourcc != GST_STR_FOURCC ("Y444")) {
          g_value_set_static_string (&value, "mpeg2");
          gst_value_list_append_value (&value_list, &value);

          g_value_set_static_string (&value, "jpeg");
          gst_value_list_append_value (&value_list, &value);

          gst_structure_set_value (structure, "chroma-site", &value_list);
        }
        g_value_unset (&value_list);
      }
    }
      break;
    case VTS_BAYER:
      structure = gst_structure_new ("video/x-raw-bayer",
          "format", G_TYPE_STRING, format->fourcc, NULL);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  return structure;
}

/* returns the size in bytes for one video frame of the given dimensions
 * given the fourcc in GstImgSrc */
int
gst_img_src_get_size (GstImgSrc * v, int w, int h)
{
  paintinfo pi = { NULL, };
  paintinfo *p = &pi;
  struct fourcc_list_struct *fourcc;

  p->width = w;
  p->height = h;
  fourcc = v->fourcc;
  if (fourcc == NULL)
    return 0;

  fourcc->paint_setup (p, NULL);

  return (unsigned long) p->endptr;
}

#define SCALEBITS 10
#define ONE_HALF  (1 << (SCALEBITS - 1))
#define FIX(x)    ((int) ((x) * (1<<SCALEBITS) + 0.5))

#define RGB_TO_Y(r, g, b) \
((FIX(0.29900) * (r) + FIX(0.58700) * (g) + \
  FIX(0.11400) * (b) + ONE_HALF) >> SCALEBITS)

#define RGB_TO_U(r1, g1, b1, shift)\
(((- FIX(0.16874) * r1 - FIX(0.33126) * g1 +         \
     FIX(0.50000) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V(r1, g1, b1, shift)\
(((FIX(0.50000) * r1 - FIX(0.41869) * g1 -           \
   FIX(0.08131) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_Y_CCIR(r, g, b) \
((FIX(0.29900*219.0/255.0) * (r) + FIX(0.58700*219.0/255.0) * (g) + \
  FIX(0.11400*219.0/255.0) * (b) + (ONE_HALF + (16 << SCALEBITS))) >> SCALEBITS)

#define RGB_TO_U_CCIR(r1, g1, b1, shift)\
(((- FIX(0.16874*224.0/255.0) * r1 - FIX(0.33126*224.0/255.0) * g1 +         \
     FIX(0.50000*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V_CCIR(r1, g1, b1, shift)\
(((FIX(0.50000*224.0/255.0) * r1 - FIX(0.41869*224.0/255.0) * g1 -           \
   FIX(0.08131*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_Y_CCIR_709(r, g, b) \
((FIX(0.212600*219.0/255.0) * (r) + FIX(0.715200*219.0/255.0) * (g) + \
  FIX(0.072200*219.0/255.0) * (b) + (ONE_HALF + (16 << SCALEBITS))) >> SCALEBITS)

#define RGB_TO_U_CCIR_709(r1, g1, b1, shift)\
(((- FIX(0.114572*224.0/255.0) * r1 - FIX(0.385427*224.0/255.0) * g1 +         \
     FIX(0.50000*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

#define RGB_TO_V_CCIR_709(r1, g1, b1, shift)\
(((FIX(0.50000*224.0/255.0) * r1 - FIX(0.454153*224.0/255.0) * g1 -           \
   FIX(0.045847*224.0/255.0) * b1 + (ONE_HALF << shift) - 1) >> (SCALEBITS + shift)) + 128)

static void
imgsrc_setup_paintinfo (GstImgSrc * v, paintinfo * p, int w, int h)
{
  int a, r, g, b;

  if (v->color_spec == GST_IMG_SRC_BT601) {
    p->colors = vts_colors_bt601_ycbcr_100;
  } else {
    p->colors = vts_colors_bt709_ycbcr_100;
  }
  p->width = w;
  p->height = h;

  p->convert_tmpline = v->fourcc->convert_hline;
  if (v->fourcc->type == VTS_RGB || v->fourcc->type == VTS_BAYER) {
    p->paint_tmpline = paint_tmpline_ARGB;
  } else {
    p->paint_tmpline = paint_tmpline_AYUV;
  }
  p->tmpline = v->tmpline;
  p->tmpline2 = v->tmpline2;
  p->tmpline_u8 = v->tmpline_u8;
  p->x_offset = (v->horizontal_speed * v->n_frames) % p->width;
  if (p->x_offset < 0)
    p->x_offset += p->width;

  a = (v->foreground_color >> 24) & 0xff;
  r = (v->foreground_color >> 16) & 0xff;
  g = (v->foreground_color >> 8) & 0xff;
  b = (v->foreground_color >> 0) & 0xff;
  p->foreground_color.A = a;
  p->foreground_color.R = r;
  p->foreground_color.G = g;
  p->foreground_color.B = b;
  if (v->color_spec == GST_IMG_SRC_BT601) {
    p->foreground_color.Y = RGB_TO_Y_CCIR (r, g, b);
    p->foreground_color.U = RGB_TO_U_CCIR (r, g, b, 0);
    p->foreground_color.V = RGB_TO_V_CCIR (r, g, b, 0);
  } else {
    p->foreground_color.Y = RGB_TO_Y_CCIR_709 (r, g, b);
    p->foreground_color.U = RGB_TO_U_CCIR_709 (r, g, b, 0);
    p->foreground_color.V = RGB_TO_V_CCIR_709 (r, g, b, 0);
  }
  p->foreground_color.gray = RGB_TO_Y (r, g, b);

  a = (v->background_color >> 24) & 0xff;
  r = (v->background_color >> 16) & 0xff;
  g = (v->background_color >> 8) & 0xff;
  b = (v->background_color >> 0) & 0xff;
  p->background_color.A = a;
  p->background_color.R = r;
  p->background_color.G = g;
  p->background_color.B = b;
  if (v->color_spec == GST_IMG_SRC_BT601) {
    p->background_color.Y = RGB_TO_Y_CCIR (r, g, b);
    p->background_color.U = RGB_TO_U_CCIR (r, g, b, 0);
    p->background_color.V = RGB_TO_V_CCIR (r, g, b, 0);
  } else {
    p->background_color.Y = RGB_TO_Y_CCIR_709 (r, g, b);
    p->background_color.U = RGB_TO_U_CCIR_709 (r, g, b, 0);
    p->background_color.V = RGB_TO_V_CCIR_709 (r, g, b, 0);
  }
  p->background_color.gray = RGB_TO_Y (r, g, b);

}

static void
imgsrc_convert_tmpline (paintinfo * p, int j)
{
  int x = p->x_offset;
  int i;

  if (x != 0) {
    memcpy (p->tmpline2, p->tmpline, p->width * 4);
    memcpy (p->tmpline, p->tmpline2 + x * 4, (p->width - x) * 4);
    memcpy (p->tmpline + (p->width - x) * 4, p->tmpline2, x * 4);
  }

  for (i = p->width; i < p->width + 5; i++) {
    p->tmpline[4 * i + 0] = p->tmpline[4 * (p->width - 1) + 0];
    p->tmpline[4 * i + 1] = p->tmpline[4 * (p->width - 1) + 1];
    p->tmpline[4 * i + 2] = p->tmpline[4 * (p->width - 1) + 2];
    p->tmpline[4 * i + 3] = p->tmpline[4 * (p->width - 1) + 3];
  }

  p->convert_tmpline (p, j);
}

#define BLEND1(a,b,x) ((a)*(x) + (b)*(255-(x)))
#define DIV255(x) (((x) + (((x)+128)>>8) + 128)>>8)
#define BLEND(a,b,x) DIV255(BLEND1(a,b,x))

#ifdef unused
static void
imgsrc_blend_color (struct vts_color_struct *dest,
    struct vts_color_struct *a, struct vts_color_struct *b, int x)
{
  dest->Y = BLEND (a->Y, b->Y, x);
  dest->U = BLEND (a->U, b->U, x);
  dest->V = BLEND (a->V, b->V, x);
  dest->R = BLEND (a->R, b->R, x);
  dest->G = BLEND (a->G, b->G, x);
  dest->B = BLEND (a->B, b->B, x);
  dest->gray = BLEND (a->gray, b->gray, x);

}
#endif

static void
imgsrc_blend_line (GstImgSrc * v, guint8 * dest, guint8 * src,
    struct vts_color_struct *a, struct vts_color_struct *b, int n)
{
  int i;
  if (v->fourcc->type == VTS_RGB || v->fourcc->type == VTS_BAYER) {
    for (i = 0; i < n; i++) {
      dest[i * 4 + 0] = BLEND (a->A, b->A, src[i]);
      dest[i * 4 + 1] = BLEND (a->R, b->R, src[i]);
      dest[i * 4 + 2] = BLEND (a->G, b->G, src[i]);
      dest[i * 4 + 3] = BLEND (a->B, b->B, src[i]);
    }
  } else {
    for (i = 0; i < n; i++) {
      dest[i * 4 + 0] = BLEND (a->A, b->A, src[i]);
      dest[i * 4 + 1] = BLEND (a->Y, b->Y, src[i]);
      dest[i * 4 + 2] = BLEND (a->U, b->U, src[i]);
      dest[i * 4 + 3] = BLEND (a->V, b->V, src[i]);
    }
  }
#undef BLEND
}

void
gst_img_src_smpte (GstImgSrc * v, unsigned char *dest, int w,
    int h)
{
  int i;
  int y1, y2;
  int j;
  paintinfo pi = { NULL, };
  paintinfo *p = &pi;
  struct fourcc_list_struct *fourcc;

  imgsrc_setup_paintinfo (v, p, w, h);
  fourcc = v->fourcc;
  if (fourcc == NULL)
    return;

  fourcc->paint_setup (p, dest);

  y1 = 2 * h / 3;
  y2 = h * 0.75;

  /* color bars */
  for (j = 0; j < y1; j++) {
    for (i = 0; i < 7; i++) {
      int x1 = i * w / 7;
      int x2 = (i + 1) * w / 7;

      p->color = p->colors + i;
      p->paint_tmpline (p, x1, (x2 - x1));
    }
    imgsrc_convert_tmpline (p, j);
  }

  /* inverse blue bars */
  for (j = y1; j < y2; j++) {
    for (i = 0; i < 7; i++) {
      int x1 = i * w / 7;
      int x2 = (i + 1) * w / 7;
      int k;

      if (i & 1) {
        k = 7;
      } else {
        k = 6 - i;
      }
      p->color = p->colors + k;
      p->paint_tmpline (p, x1, (x2 - x1));
    }
    imgsrc_convert_tmpline (p, j);
  }

  for (j = y2; j < h; j++) {
    /* -I, white, Q regions */
    for (i = 0; i < 3; i++) {
      int x1 = i * w / 6;
      int x2 = (i + 1) * w / 6;
      int k;

      if (i == 0) {
        k = 8;
      } else if (i == 1) {
        k = 0;
      } else
        k = 9;

      p->color = p->colors + k;
      p->paint_tmpline (p, x1, (x2 - x1));
    }

    /* superblack, black, dark grey */
    for (i = 0; i < 3; i++) {
      int x1 = w / 2 + i * w / 12;
      int x2 = w / 2 + (i + 1) * w / 12;
      int k;

      if (i == 0) {
        k = COLOR_SUPER_BLACK;
      } else if (i == 1) {
        k = COLOR_BLACK;
      } else
        k = COLOR_DARK_GREY;

      p->color = p->colors + k;
      p->paint_tmpline (p, x1, (x2 - x1));
    }

    {
      int x1 = w * 3 / 4;
      struct vts_color_struct color;

      color = p->colors[COLOR_BLACK];
      p->color = &color;

      for (i = x1; i < w; i++) {
        int y = random_char ();
        p->tmpline_u8[i] = y;
      }
      imgsrc_blend_line (v, p->tmpline + x1 * 4, p->tmpline_u8 + x1,
          &p->foreground_color, &p->background_color, w - x1);

    }
    imgsrc_convert_tmpline (p, j);

  }
}


static void
convert_hline_bayer (paintinfo * p, int y)
{
  int i;
  guint8 *R = p->yp + y * p->ystride;
  guint8 *argb = p->tmpline;
  int x_inv = p->bayer_x_invert;
  int y_inv = p->bayer_y_invert;

  if ((y ^ y_inv) & 1) {
    for (i = 0; i < p->width; i++) {
      if ((i ^ x_inv) & 1) {
        R[i] = argb[4 * i + 1];
      } else {
        R[i] = argb[4 * i + 2];
      }
    }
  } else {
    for (i = 0; i < p->width; i++) {
      if ((i ^ x_inv) & 1) {
        R[i] = argb[4 * i + 2];
      } else {
        R[i] = argb[4 * i + 3];
      }
    }
  }
}




static void
paint_tmpline_ARGB (paintinfo * p, int x, int w)
{
  int offset;
  guint32 value;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  value = (p->color->A << 0) | (p->color->R << 8) |
      (p->color->G << 16) | (p->color->B << 24);
#else
  value = (p->color->A << 24) | (p->color->R << 16) |
      (p->color->G << 8) | (p->color->B << 0);
#endif

  offset = (x * 4);
  //gst_orc_splat_u32 (p->tmpline + offset, value, w);
}

static void
paint_tmpline_AYUV (paintinfo * p, int x, int w)
{
  int offset;
  guint32 value;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  value = (p->color->A << 0) | (p->color->Y << 8) |
      (p->color->U << 16) | (p->color->V << 24);
#else
  value = (p->color->A << 24) | (p->color->Y << 16) |
      (p->color->U << 8) | (p->color->V << 0);
#endif

  offset = (x * 4);
  //gst_orc_splat_u32 (p->tmpline + offset, value, w);
}


static void
paint_setup_I420 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * GST_ROUND_UP_2 (p->height);
  p->ustride = GST_ROUND_UP_8 (p->width) / 2;
  p->vp = p->up + p->ustride * GST_ROUND_UP_2 (p->height) / 2;
  p->vstride = GST_ROUND_UP_8 (p->ystride) / 2;
  p->endptr = p->vp + p->vstride * GST_ROUND_UP_2 (p->height) / 2;
}

static void
paint_setup_NV12 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * GST_ROUND_UP_2 (p->height);
  p->vp = p->up + 1;
  p->ustride = p->ystride;
  p->vstride = p->ystride;
  p->endptr = p->up + (p->ystride * GST_ROUND_UP_2 (p->height)) / 2;
}

static void
paint_setup_NV21 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->vp = p->yp + p->ystride * GST_ROUND_UP_2 (p->height);
  p->up = p->vp + 1;
  p->ustride = p->ystride;
  p->vstride = p->ystride;
  p->endptr = p->vp + (p->ystride * GST_ROUND_UP_2 (p->height)) / 2;
}

static void
convert_hline_I420 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + (y / 2) * p->ustride;
  guint8 *V = p->vp + (y / 2) * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 1) / 2; i++) {
    U[i] = (ayuv[4 * (i * 2) + 2] + ayuv[4 * (i * 2 + 1) + 2] + 1) >> 1;
    V[i] = (ayuv[4 * (i * 2) + 3] + ayuv[4 * (i * 2 + 1) + 3] + 1) >> 1;
  }
}

static void
convert_hline_NV12 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + (y / 2) * p->ustride;
  guint8 *V = p->vp + (y / 2) * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 1) / 2; i++) {
    U[i * 2] = (ayuv[4 * (i * 2) + 2] + ayuv[4 * (i * 2 + 1) + 2] + 1) >> 1;
    V[i * 2] = (ayuv[4 * (i * 2) + 3] + ayuv[4 * (i * 2 + 1) + 3] + 1) >> 1;
  }
}

static void
convert_hline_NV21 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + (y / 2) * p->ustride;
  guint8 *V = p->vp + (y / 2) * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 1) / 2; i++) {
    U[i * 2] = (ayuv[4 * (i * 2) + 2] + ayuv[4 * (i * 2 + 1) + 2] + 1) >> 1;
    V[i * 2] = (ayuv[4 * (i * 2) + 3] + ayuv[4 * (i * 2 + 1) + 3] + 1) >> 1;
  }
}


static void
paint_setup_YV12 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->vp = p->yp + p->ystride * GST_ROUND_UP_2 (p->height);
  p->vstride = GST_ROUND_UP_8 (p->ystride) / 2;
  p->up = p->vp + p->vstride * GST_ROUND_UP_2 (p->height) / 2;
  p->ustride = GST_ROUND_UP_8 (p->ystride) / 2;
  p->endptr = p->up + p->ustride * GST_ROUND_UP_2 (p->height) / 2;
}

static void
paint_setup_v308 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + 1;
  p->vp = dest + 2;
  p->ystride = GST_ROUND_UP_4 (p->width * 3);
  p->ustride = GST_ROUND_UP_4 (p->width * 3);
  p->vstride = GST_ROUND_UP_4 (p->width * 3);
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_AYUV (paintinfo * p, unsigned char *dest)
{
  p->ap = dest;
  p->yp = dest + 1;
  p->up = dest + 2;
  p->vp = dest + 3;
  p->ystride = p->width * 4;
  p->ustride = p->width * 4;
  p->vstride = p->width * 4;
  p->endptr = dest + p->ystride * p->height;
}

#ifdef disabled
static void
paint_setup_v410 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 0;
  p->up = dest + 0;
  p->vp = dest + 0;
  p->ystride = p->width * 4;
  p->endptr = dest + p->ystride * p->height;
}
#endif

static void
paint_setup_v216 (paintinfo * p, unsigned char *dest)
{
  p->ap = dest;
  p->yp = dest + 2;
  p->up = dest + 0;
  p->vp = dest + 4;
  p->ystride = p->width * 4;
  p->ustride = p->width * 4;
  p->vstride = p->width * 4;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_v210 (paintinfo * p, unsigned char *dest)
{
  p->ap = dest;
  p->yp = dest + 0;
  p->up = dest + 0;
  p->vp = dest + 0;
  p->ystride = ((p->width + 47) / 48) * 128;    /* no, really. */
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_UYVP (paintinfo * p, unsigned char *dest)
{
  p->ap = dest;
  p->yp = dest + 0;
  p->up = dest + 0;
  p->vp = dest + 0;
  p->ystride = GST_ROUND_UP_4 ((p->width * 2 * 5 + 3) / 4);
  GST_ERROR ("stride %d", p->ystride);
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_YUY2 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + 1;
  p->vp = dest + 3;
  p->ystride = GST_ROUND_UP_2 (p->width) * 2;
  p->ustride = GST_ROUND_UP_2 (p->width) * 2;
  p->vstride = GST_ROUND_UP_2 (p->width) * 2;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_UYVY (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 1;
  p->up = dest;
  p->vp = dest + 2;
  p->ystride = GST_ROUND_UP_2 (p->width) * 2;
  p->ustride = GST_ROUND_UP_2 (p->width) * 2;
  p->vstride = GST_ROUND_UP_2 (p->width) * 2;
  p->endptr = dest + p->ystride * p->height;
}

static void
paint_setup_YVYU (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->up = dest + 3;
  p->vp = dest + 1;
  p->ystride = GST_ROUND_UP_2 (p->width) * 2;
  p->ustride = GST_ROUND_UP_2 (p->width) * 2;
  p->vstride = GST_ROUND_UP_2 (p->width) * 2;
  p->endptr = dest + p->ystride * p->height;
}


static void
convert_hline_YUY2 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i * 2] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 1) / 2; i++) {
    U[4 * i] = (ayuv[4 * (i * 2) + 2] + ayuv[4 * (i * 2 + 1) + 2] + 1) >> 1;
    V[4 * i] = (ayuv[4 * (i * 2) + 3] + ayuv[4 * (i * 2 + 1) + 3] + 1) >> 1;
  }
}



static void
convert_hline_v308 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i * 3] = ayuv[4 * i + 1];
    U[i * 3] = ayuv[4 * i + 2];
    V[i * 3] = ayuv[4 * i + 3];
  }
}

static void
convert_hline_AYUV (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *A = p->ap + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    A[i * 4] = ayuv[4 * i + 0];
    Y[i * 4] = ayuv[4 * i + 1];
    U[i * 4] = ayuv[4 * i + 2];
    V[i * 4] = ayuv[4 * i + 3];
  }
}

#define TO_16(x) (((x)<<8) | (x))
#define TO_10(x) (((x)<<2) | ((x)>>6))

static void
convert_hline_v216 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    GST_WRITE_UINT16_LE (Y + i * 4, TO_16 (ayuv[4 * i + 1]));
  }
  for (i = 0; i < (p->width + 1) / 2; i++) {
    GST_WRITE_UINT16_LE (U + i * 8, TO_16 (ayuv[4 * (i * 2) + 2]));
    GST_WRITE_UINT16_LE (V + i * 8, TO_16 (ayuv[4 * (i * 2) + 3]));
  }
}

#ifdef disabled
static void
convert_hline_v410 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    guint32 a;

    a = (TO_10 (ayuv[4 * i + 2]) << 22) |
        (TO_10 (ayuv[4 * i + 1]) << 12) | (TO_10 (ayuv[4 * i + 3]) << 2);
    GST_WRITE_UINT32_LE (Y + i * 4, a);
  }
}
#endif

static void
convert_hline_v210 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width + 5; i += 6) {
    guint32 a0, a1, a2, a3;
    guint16 y0, y1, y2, y3, y4, y5;
    guint16 u0, u1, u2;
    guint16 v0, v1, v2;

    y0 = ayuv[4 * (i + 0) + 1];
    y1 = ayuv[4 * (i + 1) + 1];
    y2 = ayuv[4 * (i + 2) + 1];
    y3 = ayuv[4 * (i + 3) + 1];
    y4 = ayuv[4 * (i + 4) + 1];
    y5 = ayuv[4 * (i + 5) + 1];

    u0 = (ayuv[4 * (i + 0) + 2] + ayuv[4 * (i + 1) + 2] + 1) >> 1;
    u1 = (ayuv[4 * (i + 2) + 2] + ayuv[4 * (i + 3) + 2] + 1) >> 1;
    u2 = (ayuv[4 * (i + 4) + 2] + ayuv[4 * (i + 5) + 2] + 1) >> 1;

    v0 = (ayuv[4 * (i + 0) + 3] + ayuv[4 * (i + 1) + 3] + 1) >> 1;
    v1 = (ayuv[4 * (i + 2) + 3] + ayuv[4 * (i + 3) + 3] + 1) >> 1;
    v2 = (ayuv[4 * (i + 4) + 3] + ayuv[4 * (i + 5) + 3] + 1) >> 1;

#if 0
    a0 = TO_10 (ayuv[4 * (i + 0) + 2]) | (TO_10 (ayuv[4 * (i + 0) + 1]) << 10)
        | (TO_10 (ayuv[4 * (i + 0) + 3]) << 20);
    a1 = TO_10 (ayuv[4 * (i + 1) + 1]) | (TO_10 (ayuv[4 * (i + 2) + 2]) << 10)
        | (TO_10 (ayuv[4 * (i + 2) + 1]) << 20);
    a2 = TO_10 (ayuv[4 * (i + 2) + 3]) | (TO_10 (ayuv[4 * (i + 3) + 1]) << 10)
        | (TO_10 (ayuv[4 * (i + 4) + 2]) << 20);
    a3 = TO_10 (ayuv[4 * (i + 4) + 1]) | (TO_10 (ayuv[4 * (i + 4) + 3]) << 10)
        | (TO_10 (ayuv[4 * (i + 5) + 1]) << 20);
#endif

    a0 = TO_10 (u0) | (TO_10 (y0) << 10) | (TO_10 (v0) << 20);
    a1 = TO_10 (y1) | (TO_10 (u1) << 10) | (TO_10 (y2) << 20);
    a2 = TO_10 (v1) | (TO_10 (y3) << 10) | (TO_10 (u2) << 20);
    a3 = TO_10 (y4) | (TO_10 (v2) << 10) | (TO_10 (y5) << 20);

    GST_WRITE_UINT32_LE (Y + (i / 6) * 16 + 0, a0);
    GST_WRITE_UINT32_LE (Y + (i / 6) * 16 + 4, a1);
    GST_WRITE_UINT32_LE (Y + (i / 6) * 16 + 8, a2);
    GST_WRITE_UINT32_LE (Y + (i / 6) * 16 + 12, a3);
  }
}

static void
convert_hline_UYVP (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i += 2) {
    guint16 y0, y1;
    guint16 u0;
    guint16 v0;

    y0 = ayuv[4 * (i + 0) + 1];
    y1 = ayuv[4 * (i + 1) + 1];
    u0 = (ayuv[4 * (i + 0) + 2] + ayuv[4 * (i + 1) + 2] + 1) >> 1;
    v0 = (ayuv[4 * (i + 0) + 3] + ayuv[4 * (i + 1) + 3] + 1) >> 1;

    Y[(i / 2) * 5 + 0] = u0;
    Y[(i / 2) * 5 + 1] = y0 >> 2;
    Y[(i / 2) * 5 + 2] = (y0 << 6) | (v0 >> 4);
    Y[(i / 2) * 5 + 3] = (v0 << 4) | (y1 >> 2);
    Y[(i / 2) * 5 + 4] = (y1 << 2);
  }
}


#ifdef disabled
static void
paint_setup_IYU2 (paintinfo * p, unsigned char *dest)
{
  /* untested */
  p->yp = dest + 1;
  p->up = dest + 0;
  p->vp = dest + 2;
  p->ystride = GST_ROUND_UP_4 (p->width * 3);
  p->endptr = dest + p->ystride * p->height;
}

static void
convert_hline_IYU2 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i * 3] = ayuv[4 * i + 1];
    U[i * 3] = ayuv[4 * i + 2];
    V[i * 3] = ayuv[4 * i + 3];
  }
}
#endif

static void
paint_setup_Y41B (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * p->height;
  p->ustride = GST_ROUND_UP_16 (p->width) / 4;
  p->vp = p->up + p->ustride * p->height;
  p->vstride = GST_ROUND_UP_16 (p->width) / 4;
  p->endptr = p->vp + p->vstride * p->height;
}

static void
convert_hline_Y41B (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 3) / 4; i++) {
    U[i] = (ayuv[4 * (i * 4) + 2] + ayuv[4 * (i * 4 + 1) + 2] +
        ayuv[4 * (i * 4 + 2) + 2] + ayuv[4 * (i * 4 + 3) + 2] + 2) >> 2;
    V[i] = (ayuv[4 * (i * 4) + 3] + ayuv[4 * (i * 4 + 1) + 3] +
        ayuv[4 * (i * 4 + 2) + 3] + ayuv[4 * (i * 4 + 3) + 3] + 2) >> 2;
  }
}

static void
paint_setup_Y42B (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * p->height;
  p->ustride = GST_ROUND_UP_8 (p->width) / 2;
  p->vp = p->up + p->ustride * p->height;
  p->vstride = GST_ROUND_UP_8 (p->width) / 2;
  p->endptr = p->vp + p->vstride * p->height;
}

static void
convert_hline_Y42B (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 1) / 2; i++) {
    U[i] = (ayuv[4 * (i * 2) + 2] + ayuv[4 * (i * 2 + 1) + 2] + 1) >> 1;
    V[i] = (ayuv[4 * (i * 2) + 3] + ayuv[4 * (i * 2 + 1) + 3] + 1) >> 1;
  }
}

static void
paint_setup_Y444 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->ustride = GST_ROUND_UP_4 (p->width);
  p->vstride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * p->height;
  p->vp = p->up + p->ystride * p->height;
  p->endptr = p->vp + p->ystride * p->height;
}

static void
convert_hline_Y444 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + y * p->ustride;
  guint8 *V = p->vp + y * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
    U[i] = ayuv[4 * i + 2];
    V[i] = ayuv[4 * i + 3];
  }
}

static void
paint_setup_Y800 (paintinfo * p, unsigned char *dest)
{
  /* untested */
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->endptr = dest + p->ystride * p->height;
}

static void
convert_hline_Y800 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
}

static void
paint_setup_YVU9 (paintinfo * p, unsigned char *dest)
{
  int h = GST_ROUND_UP_4 (p->height);

  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->vp = p->yp + p->ystride * h;
  p->vstride = GST_ROUND_UP_4 (p->ystride / 4);
  p->up = p->vp + p->vstride * h / 4;
  p->ustride = GST_ROUND_UP_4 (p->ystride / 4);
  p->endptr = p->up + p->ustride * h / 4;
}

static void
paint_setup_YUV9 (paintinfo * p, unsigned char *dest)
{
  /* untested */
  int h = GST_ROUND_UP_4 (p->height);

  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->up = p->yp + p->ystride * h;
  p->ustride = GST_ROUND_UP_4 (p->ystride / 4);
  p->vp = p->up + p->ustride * h / 4;
  p->vstride = GST_ROUND_UP_4 (p->ystride / 4);
  p->endptr = p->vp + p->vstride * h / 4;
}

static void
convert_hline_YUV9 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *U = p->up + (y / 4) * p->ustride;
  guint8 *V = p->vp + (y / 4) * p->vstride;
  guint8 *ayuv = p->tmpline;

  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
  for (i = 0; i < (p->width + 3) / 4; i++) {
    U[i] = (ayuv[4 * (i * 4) + 2] + ayuv[4 * (i * 4 + 1) + 2] +
        ayuv[4 * (i * 4 + 2) + 2] + ayuv[4 * (i * 4 + 3) + 2] + 2) >> 2;
    V[i] = (ayuv[4 * (i * 4) + 3] + ayuv[4 * (i * 4 + 1) + 3] +
        ayuv[4 * (i * 4 + 2) + 3] + ayuv[4 * (i * 4 + 3) + 3] + 2) >> 2;
  }
}

static void
paint_setup_ARGB8888 (paintinfo * p, unsigned char *dest)
{
  paint_setup_xRGB8888 (p, dest);
}

static void
paint_setup_ABGR8888 (paintinfo * p, unsigned char *dest)
{
  paint_setup_xBGR8888 (p, dest);
}

static void
paint_setup_RGBA8888 (paintinfo * p, unsigned char *dest)
{
  paint_setup_RGBx8888 (p, dest);
}

static void
paint_setup_BGRA8888 (paintinfo * p, unsigned char *dest)
{
  paint_setup_BGRx8888 (p, dest);
}

static void
paint_setup_xRGB8888 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 1;
  p->up = dest + 2;
  p->vp = dest + 3;
  p->ap = dest;
  p->ystride = p->width * 4;
  p->ustride = p->width * 4;
  p->vstride = p->width * 4;
  p->endptr = p->dest + p->ystride * p->height;
}

static void
paint_setup_xBGR8888 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 3;
  p->up = dest + 2;
  p->vp = dest + 1;
  p->ap = dest;
  p->ystride = p->width * 4;
  p->ustride = p->width * 4;
  p->vstride = p->width * 4;
  p->endptr = p->dest + p->ystride * p->height;
}

static void
paint_setup_RGBx8888 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 0;
  p->up = dest + 1;
  p->vp = dest + 2;
  p->ap = dest + 3;
  p->ystride = p->width * 4;
  p->ustride = p->width * 4;
  p->vstride = p->width * 4;
  p->endptr = p->dest + p->ystride * p->height;
}

static void
paint_setup_BGRx8888 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 2;
  p->up = dest + 1;
  p->vp = dest + 0;
  p->ap = dest + 3;
  p->ystride = p->width * 4;
  p->ustride = p->width * 4;
  p->vstride = p->width * 4;
  p->endptr = p->dest + p->ystride * p->height;
}

static void
paint_setup_RGB888 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 0;
  p->up = dest + 1;
  p->vp = dest + 2;
  p->ystride = GST_ROUND_UP_4 (p->width * 3);
  p->ustride = GST_ROUND_UP_4 (p->width * 3);
  p->vstride = GST_ROUND_UP_4 (p->width * 3);
  p->endptr = p->dest + p->ystride * p->height;
}

static void
paint_setup_BGR888 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest + 2;
  p->up = dest + 1;
  p->vp = dest + 0;
  p->ystride = GST_ROUND_UP_4 (p->width * 3);
  p->ustride = GST_ROUND_UP_4 (p->width * 3);
  p->vstride = GST_ROUND_UP_4 (p->width * 3);
  p->endptr = p->dest + p->ystride * p->height;
}

static void
convert_hline_str4 (paintinfo * p, int y)
{
  int i;
  guint8 *A = p->ap + y * p->ystride;
  guint8 *R = p->yp + y * p->ystride;
  guint8 *G = p->up + y * p->ustride;
  guint8 *B = p->vp + y * p->vstride;
  guint8 *argb = p->tmpline;

  for (i = 0; i < p->width; i++) {
    A[4 * i] = 0xff;
    R[4 * i] = argb[4 * i + 1];
    G[4 * i] = argb[4 * i + 2];
    B[4 * i] = argb[4 * i + 3];
  }
}

static void
convert_hline_astr4 (paintinfo * p, int y)
{
  int i;
  guint8 *A = p->ap + y * p->ystride;
  guint8 *R = p->yp + y * p->ystride;
  guint8 *G = p->up + y * p->ustride;
  guint8 *B = p->vp + y * p->vstride;
  guint8 *argb = p->tmpline;

  for (i = 0; i < p->width; i++) {
    A[4 * i] = argb[4 * i + 0];
    R[4 * i] = argb[4 * i + 1];
    G[4 * i] = argb[4 * i + 2];
    B[4 * i] = argb[4 * i + 3];
  }
}

static void
convert_hline_str3 (paintinfo * p, int y)
{
  int i;
  guint8 *R = p->yp + y * p->ystride;
  guint8 *G = p->up + y * p->ustride;
  guint8 *B = p->vp + y * p->vstride;
  guint8 *argb = p->tmpline;

  for (i = 0; i < p->width; i++) {
    R[3 * i] = argb[4 * i + 1];
    G[3 * i] = argb[4 * i + 2];
    B[3 * i] = argb[4 * i + 3];
  }
}

static void
paint_setup_RGB565 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width * 2);
  p->ustride = GST_ROUND_UP_4 (p->width * 2);
  p->vstride = GST_ROUND_UP_4 (p->width * 2);
  p->endptr = p->dest + p->ystride * p->height;
}

static void
convert_hline_RGB565 (paintinfo * p, int y)
{
  int i;
  guint8 *R = p->yp + y * p->ystride;
  guint8 *argb = p->tmpline;

  for (i = 0; i < p->width; i++) {
    guint16 value = ((argb[4 * i + 1] & 0xf8) << 8) |
        ((argb[4 * i + 2] & 0xfc) << 3) | ((argb[4 * i + 3] & 0xf8) >> 3);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    GST_WRITE_UINT16_LE (R + 2 * i, value);
#else
    GST_WRITE_UINT16_BE (R + 2 * i, value);
#endif
  }
}

static void
convert_hline_xRGB1555 (paintinfo * p, int y)
{
  int i;
  guint8 *R = p->yp + y * p->ystride;
  guint8 *argb = p->tmpline;

  for (i = 0; i < p->width; i++) {
    guint16 value = ((argb[4 * i + 1] & 0xf8) << 7) |
        ((argb[4 * i + 2] & 0xf8) << 2) | ((argb[4 * i + 3] & 0xf8) >> 3);
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    GST_WRITE_UINT16_LE (R + 2 * i, value);
#else
    GST_WRITE_UINT16_BE (R + 2 * i, value);
#endif
  }
}

static void
paint_setup_xRGB1555 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width * 2);
  p->ustride = GST_ROUND_UP_4 (p->width * 2);
  p->vstride = GST_ROUND_UP_4 (p->width * 2);
  p->endptr = p->dest + p->ystride * p->height;
}


static void
paint_setup_bayer_bggr (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->ustride = GST_ROUND_UP_4 (p->width);
  p->vstride = GST_ROUND_UP_4 (p->width);
  p->endptr = p->dest + p->ystride * p->height;
  p->bayer_x_invert = 0;
  p->bayer_y_invert = 0;
}

static void
paint_setup_bayer_rggb (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->ustride = GST_ROUND_UP_4 (p->width);
  p->vstride = GST_ROUND_UP_4 (p->width);
  p->endptr = p->dest + p->ystride * p->height;
  p->bayer_x_invert = 1;
  p->bayer_y_invert = 1;
}

static void
paint_setup_bayer_grbg (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->ustride = GST_ROUND_UP_4 (p->width);
  p->vstride = GST_ROUND_UP_4 (p->width);
  p->endptr = p->dest + p->ystride * p->height;
  p->bayer_x_invert = 0;
  p->bayer_y_invert = 1;
}

static void
paint_setup_bayer_gbrg (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->ustride = GST_ROUND_UP_4 (p->width);
  p->vstride = GST_ROUND_UP_4 (p->width);
  p->endptr = p->dest + p->ystride * p->height;
  p->bayer_x_invert = 1;
  p->bayer_y_invert = 0;
}

static void
paint_setup_GRAY8 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width);
  p->endptr = dest + p->ystride * p->height;
}

static void
convert_hline_GRAY8 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  /* FIXME this should use gray, not YUV */
  for (i = 0; i < p->width; i++) {
    Y[i] = ayuv[4 * i + 1];
  }
}

static void
paint_setup_GRAY16 (paintinfo * p, unsigned char *dest)
{
  p->yp = dest;
  p->ystride = GST_ROUND_UP_4 (p->width * 2);
  p->endptr = dest + p->ystride * p->height;
}

static void
convert_hline_GRAY16 (paintinfo * p, int y)
{
  int i;
  guint8 *Y = p->yp + y * p->ystride;
  guint8 *ayuv = p->tmpline;

  /* FIXME this should use gray, not YUV */
  for (i = 0; i < p->width; i++) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    GST_WRITE_UINT16_LE (Y + i * 2, ayuv[4 * i + 1] << 8);
#else
    GST_WRITE_UINT16_BE (Y + i * 2, ayuv[4 * i + 1] << 8);
#endif
  }
}
