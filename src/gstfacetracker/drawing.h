#ifndef LIB_IMAGE_COMPOSITION_DRAWING_H
#define LIB_IMAGE_COMPOSITION_DRAWING_H

#define DRAWING_YUV_BLACK       0, 128, 128
#define DRAWING_YUV_RED        81,  90, 239
#define DRAWING_YUV_GREEN     144,  54,  34
#define DRAWING_YUV_BLUE       40, 239, 110
#define DRAWING_YUV_YELLOW    210,  16, 145
#define DRAWING_YUV_MAGENTHA  106, 201, 221
#define DRAWING_YUV_CYAN      169, 165,  16
#define DRAWING_YUV_WHITE     255, 128, 128
#define DRAWING_YUV_GRAY      128, 128, 128

#define DRAWING_RGB_BLACK       0,   0,   0
#define DRAWING_RGB_RED       255,   0,   0
#define DRAWING_RGB_GREEN       0, 255,   0
#define DRAWING_RGB_BLUE        0,   0, 255
#define DRAWING_RGB_YELLOW    255, 255,   0
#define DRAWING_RGB_MAGENTHA  255,   0, 255
#define DRAWING_RGB_CYAN        0, 255, 255
#define DRAWING_RGB_WHITE     255, 255, 255
#define DRAWING_RGB_GRAY      128, 128, 128

// works only on PACKED images
void draw_line(t_image *out, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_box_outline(t_image *out, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_box_filled(t_image *out, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_circle_outline(t_image *out, int x, int y, int radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_circle_filled(t_image *out, int x, int y, int radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_character(t_image *out, unsigned char chr, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void draw_string(t_image *out, unsigned char *str, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#endif
