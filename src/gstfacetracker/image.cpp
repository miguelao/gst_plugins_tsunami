
#include "image.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <stdio.h>

void init_image(t_image* img, t_image_color_space cs, t_image_data_format df) {
  assert(img != NULL);
  memset(img, 0, sizeof(t_image));
  img->color_space = cs;
  img->data_format = df;
  switch (img->color_space) {
  case COLOR_SPACE_RGB:
  case COLOR_SPACE_YUV:
  case COLOR_SPACE_HSV:
    img->channels = 3;
    img->bpp = 24;
    break;
  case COLOR_SPACE_RGBA:
  case COLOR_SPACE_YUVA:
    img->channels = 4;
    img->bpp = 32;
    break;
  case COLOR_SPACE_GRAYSCALE:
  case COLOR_SPACE_ALPHA:
    img->channels = 1;
    img->bpp = 8;
    break;
  case COLOR_SPACE_I420:
    assert(img->data_format == IMAGE_DATA_FORMAT_PLANAR);
    img->channels = 3;
    img->bpp = 16;
    break;
  default:
    fprintf(stderr, "image format not supported\n");
    exit(3);
  }
}

t_image* create_image(t_image_color_space cs, t_image_data_format df) {
  t_image* const img = (t_image *)malloc(sizeof(t_image));
  init_image(img, cs, df);
  return img;
}

void destroy_image(t_image* img) {
  if (img != NULL) {
    free(img);
    img = NULL;
  }
}

void free_image(t_image* img) {
  if (img->data[0] != NULL)
    free(img->data[0]);
  memset(img->rowsize, 0, 4*sizeof(unsigned int));
  memset(img->data, 0, 4*sizeof(unsigned char*));
}

void resize_image(t_image* image, unsigned int width, unsigned int height) {
  image->width = width;
  image->height = height;
  switch (image->color_space) {
  case COLOR_SPACE_RGB:
  case COLOR_SPACE_YUV:
    if (image->data_format == IMAGE_DATA_FORMAT_PLANAR) {
      image->rowbytes = width*3;
      image->rowsize[0] = width;
      image->rowsize[1] = width;
      image->rowsize[2] = width;
    } else {
      image->rowbytes = width*3;
      image->rowsize[0] = image->rowbytes;
    }
    break;
  case COLOR_SPACE_RGBA:
  case COLOR_SPACE_YUVA:
    if (image->data_format == IMAGE_DATA_FORMAT_PLANAR) {
      image->rowbytes = width*4;
      image->rowsize[0] = width;
      image->rowsize[1] = width;
      image->rowsize[2] = width;
      image->rowsize[3] = width;
    } else {
      image->rowbytes = width*4;
      image->rowsize[0] = image->rowbytes;
    }
    break;
  case COLOR_SPACE_GRAYSCALE:
  case COLOR_SPACE_ALPHA:
    image->rowbytes = width;
    image->rowsize[0] = width;
    break;
  case COLOR_SPACE_I420:
    assert(image->data_format == IMAGE_DATA_FORMAT_PLANAR);
    image->rowbytes = width*3/2;
    image->rowsize[0] = width;
    image->rowsize[1] = image->rowsize[2] = width/4;
    break;
  default:
    fprintf(stderr, "image format not supported\n");
    exit(3);
  }
}

void setdata_image(t_image* image, unsigned char* data) {
  image->data[RY] = data;
  if (image->rowsize[1]) { image->data[GU] = (data += image->height*image->rowsize[0]);
  if (image->rowsize[2]) { image->data[BV] = (data += image->height*image->rowsize[1]);
  if (image->rowsize[3]) { image->data[ALPHA] = (data += image->height*image->rowsize[2]); }}}
}

void alloc_image(t_image* image, unsigned int width, unsigned int height) {
  resize_image(image, width, height);
  setdata_image(image, (unsigned char*)malloc(height * image->rowbytes));
}

bool copy_image(t_image* dst, const t_image* src) {
  if (dst->width != src->width || dst->height != src->height ||
    dst->color_space != src->color_space ||
    dst->data_format != src->data_format)
    return false;
  for (unsigned int i=0; i<4; ++i) {
    if (dst->data[i] == NULL || src->data[i] == NULL) {
      assert(dst->data[i] == src->data[i]);
      assert(dst->rowsize[i] == 0);
      assert(src->rowsize[i] == 0);
      break;
    }
    assert(src->rowsize[i] != 0);
    memcpy(dst->data[i], src->data[i], src->height*src->rowsize[i]);
  }
  return true;
}

