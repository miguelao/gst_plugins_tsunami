#ifndef IMAGE_H_
#define IMAGE_H_

// \todo Replace by OpenCV stuff

typedef enum
{
  COLOR_SPACE_RGB = 0,
  COLOR_SPACE_YUV,
  COLOR_SPACE_DISPARITY,
  COLOR_SPACE_HSV,
  COLOR_SPACE_GRAYSCALE,
  COLOR_SPACE_ALPHA,

  //including ALPHA channel
  COLOR_SPACE_RGBA,
  COLOR_SPACE_YUVA,

  //12bpp: 8 bit Y plane followed by 8 bit 2x2 subsampled U and V planes
  COLOR_SPACE_I420,

  //old deprecated names
  RGB = COLOR_SPACE_RGB,
  YUV = COLOR_SPACE_YUV,
  DISPARITY = COLOR_SPACE_DISPARITY,
  HSV = COLOR_SPACE_HSV,
  GRAYSCALE = COLOR_SPACE_GRAYSCALE,
  RGBA = COLOR_SPACE_RGBA,

  COLOR_LAST
} t_image_color_space;

/*! \name Image channel.
 Defines the data channel of the image structure to be accessed. */
/*! \{ */
typedef enum
{
  IMAGE_CHANNEL_R = 0,
  IMAGE_CHANNEL_Y = 0,

  IMAGE_CHANNEL_G = 1,
  IMAGE_CHANNEL_U = 1,

  IMAGE_CHANNEL_V = 2,
  IMAGE_CHANNEL_B = 2,

  IMAGE_CHANNEL_ALPHA = 3,

  //deprecated
  RY = IMAGE_CHANNEL_R,
  GU = IMAGE_CHANNEL_G,
  BV = IMAGE_CHANNEL_B,
  ALPHA = IMAGE_CHANNEL_ALPHA
} t_image_channel;
/*! \} */

typedef enum
{
  IMAGE_DATA_FORMAT_PLANAR = 0,
  IMAGE_DATA_FORMAT_PACKED = 1,

  //old deprecated
  PLANAR = IMAGE_DATA_FORMAT_PLANAR,
  PACKED = IMAGE_DATA_FORMAT_PACKED
} t_image_data_format;

// image type definition
typedef struct {
  t_image_color_space color_space;
  t_image_data_format data_format;
  unsigned int      height;
  unsigned int      width;
  unsigned int      channels;
  unsigned int      bpp;
  unsigned int      rowbytes;
  unsigned int      rowsize[4];
  unsigned char*      data[4];
} t_image;

void init_image(t_image* img, t_image_color_space cs, t_image_data_format df = IMAGE_DATA_FORMAT_PACKED);
t_image* create_image(t_image_color_space cs = COLOR_SPACE_RGBA, t_image_data_format df = IMAGE_DATA_FORMAT_PACKED);
void destroy_image(t_image* img);
void free_image(t_image* img);
void resize_image(t_image* image, unsigned int width, unsigned int height);
void setdata_image(t_image* image, unsigned char* data);
void alloc_image(t_image* image, unsigned int width, unsigned int height);
bool copy_image(t_image* dst, const t_image* src);

#endif /* IMAGE_H_ */
