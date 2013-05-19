
#include "defines.h"
#include "image.h"
#include "drawing.h"
#include <math.h>

static unsigned int draw_font_7x4[] = {
	// (space) ! " # $ % & ' ( ) * + , - . /
	0x0000000, 0x4440400, 0xAA00000, 0x6F6F600, 0x27B3E20, 0xCD6B300, 0x685A500, 0x4400000, 
	0x2444200, 0x4222400, 0x04EA000, 0x04E4000, 0x0000260, 0x00E0000, 0x0000400, 0x2444800, 
	// 0-9
	0x6999600, 0x6222200, 0xE168F00, 0xE161E00, 0x26AF200, 0xF8E1E00, 0x68E9600, 0xF124400,
	0x6969600, 0x6971600,
	// : ; < = > ? @
	0x0400400, 0x0200260, 0x3484300, 0x0E0E000, 0xC212C00, 0x6924400, 0x69B8600,
	// A-Z
	0x699F900, 0xE9E9E00, 0x7888700, 0xE999E00, 0xF8F8F00, 0xF8F8800, 0x78B9700, 0x99F9900,
	0xE444E00, 0x3119600, 0x9ACA900, 0x8888F00, 0x9FF9900, 0x9DB9900, 0x6999600, 0xE99E900,
	0x6999610, 0xE99E900, 0x7861E00, 0xE444400, 0x9999600, 0x99AA400, 0x99FF900, 0x9969900,
	0x9971600, 0xF168F00,
	// [ \ ] ^ _ `
	0x6444600, 0x8444200, 0x6222600, 0x4A00000, 0x00000F0, 0x4200000,
	// a-z
	0x0799700, 0x8E99E00, 0x0788700, 0x1799700, 0x06BC600, 0x24E2200, 0x0799716, 0x8E99900,
	0x4044400, 0x2022224, 0x89AE900, 0x4444400, 0x0EBBB00, 0x0E99900, 0x0699600, 0x0E99E88,
	0x0799711, 0x0BC8800, 0x07B3E00, 0x4E44200, 0x0999700, 0x099A400, 0x099F600, 0x0A44A00,
	0x0999716, 0x0F24F00,
	// { | } ~
	0x64C4600, 0x4444440, 0xC464C00, 0x005A000
	
};

void draw_line(t_image *out, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	float dx = x2-x1;
	float dy = y2-y1;
	float x, y, tmp;
	
	if (fabs(dx)>fabs(dy)) {
		if (x2<x1) {
			tmp=x2; x2=x1; x1=tmp;
			tmp=y2; y2=y1; y1=tmp;
		}
		for (x=x1,y=y1; x<=x2; x++, y+=dy/dx) {
			if (x>=0 && y>=0 && x<out->width && y<out->height) {
				unsigned char *oo = out->data[0] + out->rowbytes*((int)y) + 4*((int)x);
				oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255; oo[3] = a;
			}
				
		}
	} else {
		if (y2<y1) {
			tmp=x2; x2=x1; x1=tmp;
			tmp=y2; y2=y1; y1=tmp;
		}
		for (x=x1,y=y1; y<=y2; y++, x+=dx/dy) {
			if (x>=0 && y>=0 && x<out->width && y<out->height) {
				unsigned char *oo = out->data[0] + out->rowbytes*((int)y) + 4*((int)x);
				oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255; oo[3] = a;
			}
				
		}
	}
}


void draw_box_outline(t_image *out, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int i;

	if (x1>x2) {int tmp=x1; x1=x2; x2=tmp; }
	if (y1>y2) {int tmp=y1; y1=y2; y2=tmp; }

	if (y1>=0 && y1<(int)out->height)
	  for (i=(x1<0)?0:x1; i<=(int)(((x2<(int)out->width)?x2:out->width-1)); i++) {
			unsigned char *oo = out->data[0] + out->rowbytes*y1 + out->channels*i;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255;
			if (out->channels == 4) oo[3] = a;
		}
	if (y2>=0 && y2<(int)out->height)
	  for (i=(x1<0)?0:x1; i<=(int)(((x2<(int)out->width)?x2:out->width-1)); i++) {
			unsigned char *oo = out->data[0] + out->rowbytes*y2 + out->channels*i;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255;
			if (out->channels == 4) oo[3] = a;
		}
	if (x1>=0 && x1<(int)out->width)
	  for (i=(y1<0)?0:y1; i<=(int)(((y2<(int)out->height)?y2:out->height-1)); i++) {
			unsigned char *oo = out->data[0] + out->rowbytes*i + out->channels*x1;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255;
			if (out->channels == 4) oo[3] = a;
		}
	if (x2>=0 && x2<(int)out->width)
	  for (i=(y1<0)?0:y1; i<=(int)(((y2<(int)out->height)?y2:out->height-1)); i++) {
			unsigned char *oo = out->data[0] + out->rowbytes*i + out->channels*x2;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255;
			if (out->channels == 4) oo[3] = a;
		}			
}


void draw_box_filled(t_image *out, int x1, int y1, int x2, int y2, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int i, j;

	if (x1>x2) {int tmp=x1; x1=x2; x2=tmp; }
	if (y1>y2) {int tmp=y1; y1=y2; y2=tmp; }

	if (y1<0) y1 = 0;
	if (x1<0) x1 = 0;
	if (y2>=(int)out->height) y2 = out->height-1;
	if (x2>=(int)out->width) x2 = out->width-1;
	
	for (i=y1; i<=y2; i++)
		for (j=x1; j<=x2; j++) {
			unsigned char *oo = out->data[0] + out->rowbytes*i + 4*j;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255; oo[3] = a;
		}
}
void draw_circle_outline(t_image *out, int x, int y, int radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	float t, dt;

	dt = 6.28/(radius*10);

	for (t=0;t<6.3;t+=dt) {
		int i = y + radius*sin(t);
		int j = x + radius*cos(t);
		if (i>=0 && i<(int)out->height && j>=0 && j<(int)out->width) {
			unsigned char *oo = out->data[0] + out->rowbytes*i + 4*j;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255; oo[3] = a;
		}
	}
}

void draw_circle_filled(t_image *out, int x, int y, int radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int i, j;
	int x1,x2,y1,y2;
	
	y1=y-radius; if (y1<0) y1 = 0;
	y2=y+radius; if (y2>(int)(out->height-1)) y2 = out->height-1;
	for (i=y1; i<=y2; i++) {
		int dx = sqrt(radius*radius-(i-y)*(i-y));
		x1 = x-dx; if (x1<0) x1 = 0;
		x2 = x+dx; if (x2>(int)(out->width-1)) x2 = out->width-1;
		for (j=x1; j<=x2; j++) {
			unsigned char *oo = out->data[0] + out->rowbytes*i + 4*j;
			oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255; oo[3] = a;
		}
	}
}

void draw_character(t_image *out, unsigned char chr, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	int i, j;

	if (chr<32 || chr>126) return;
	if (x<0 || x>(int)(out->width-5) || y<0 || y>(int)(out->height-8)) return;

	unsigned int font = draw_font_7x4[chr-32];
	for (i=6; i>=0; i--) {		
		for (j=3; j>=0; j--) {
			if (font & 0x1) {
				unsigned char *oo = out->data[0] + out->rowbytes*(i+y) + 4*(j+x);
				oo[0] = (oo[0]*(255-a)+r*a)/255; oo[1] = (oo[1]*(255-a)+g*a)/255; oo[2] = (oo[2]*(255-a)+b*a)/255; oo[3] = a;
			}
			font /=2;
		}
	}


}

void draw_string(t_image *out, unsigned char *str, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	while (*str>0) {
		draw_character(out, *str, x, y, r, g, b, a);
		str++;
		x+=5;
	}
}

