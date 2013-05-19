#include "face.h"
#include <string.h>
#include "drawing.h"

inline unsigned char uSub(unsigned char a, unsigned char b) {
	if (a > b)
		return (a - b);
	else
		return 0;
}

inline unsigned char aSub(unsigned char a, unsigned char b) {
	if (a + b < 255)
		return (a + b);
	else
		return 255;
}

int face_create_bins(t_face* face, t_image *image, unsigned char ymean, unsigned char yvar, unsigned char umean, unsigned char uvar, unsigned char vmean,
		unsigned char vvar) {

	face->skincolor = colorbins_create(uSub(ymean, yvar), aSub(ymean, yvar), 10, uSub(umean, uvar), aSub(umean, uvar), 10,
			uSub(vmean, vvar), aSub(vmean, vvar), 10);
	face->bgcolor = colorbins_create(uSub(ymean, yvar), aSub(ymean, yvar), 10, uSub(umean, uvar), aSub(umean, uvar), 10, uSub(vmean, vvar),
			aSub(vmean, vvar), 10);
	face->fgcolor = colorbins_create(uSub(ymean, yvar), aSub(ymean, yvar), 10, uSub(umean, uvar), aSub(umean, uvar), 10, uSub(vmean, vvar),
			aSub(vmean, vvar), 10);

	face_reset(face, image);

	return 0;
}

t_face* face_create(t_image *image) {

	t_face* face = (t_face*)malloc(sizeof(t_face));

	face->geometry = ks_create_shared(5);
	face->skincolor = colorbins_create(60, 240, 10, 100, 150, 10, 120, 170, 10);
	face->bgcolor = colorbins_create(60, 240, 10, 100, 150, 10, 120, 170, 10);
	face->fgcolor = colorbins_create(60, 240, 10, 100, 150, 10, 120, 170, 10);

	face_reset(face, image);

	return face;
}

void face_reset(t_face *face, t_image *image) {

	unsigned int w = image->width;
	unsigned int h = image->height;

	mat_set(face->geometry->cov, 0, 25);
	face->geometry->mean[0] = w / 2;
	face->geometry->cov[0] = w * w; //face->geometry->cov[3] = -w*w;
	face->geometry->mean[1] = h / 3;
	face->geometry->cov[6] = h * h; //face->geometry->cov[9] = -h*h;
	face->geometry->mean[2] = h / 6;
	face->geometry->cov[12] = w * h / 16;
	face->geometry->mean[3] = 0.0;
	face->geometry->cov[18] = w * h / 1024;
	face->geometry->cov[15] = face->geometry->cov[3];
	face->geometry->mean[4] = 0.0;
	face->geometry->cov[24] = w * h / 1024;
	face->geometry->cov[21] = face->geometry->cov[9];

	face_bgcolor_update(face, image);
	colorbins_add_gaussian_particles(face->fgcolor, 140, 120, 145, 50, 5, 5, 1000);
	colorbins_normalize(face->skincolor, face->fgcolor, face->bgcolor);
	colorbins_reset_outbins(face->skincolor);

}

void face_reset_geometry(t_face *face, t_image *image) {

	unsigned int w = image->width;
	unsigned int h = image->height;

	mat_set(face->geometry->cov,0,25);
	face->geometry->mean[0] = w/2;	face->geometry->cov[0] = w*w;		//f`ace->geometry->cov[3] = -w*w;
	face->geometry->mean[1] = h/3;	face->geometry->cov[6] = h*h;		//face->geometry->cov[9] = -h*h;
	face->geometry->mean[2] = h/6;	face->geometry->cov[12] = w*h/16;
	face->geometry->mean[3] = 0.0;	face->geometry->cov[18] = w*h/1024;	face->geometry->cov[15] = face->geometry->cov[3];
	face->geometry->mean[4] = 0.0;	face->geometry->cov[24] = w*h/1024;	face->geometry->cov[21] = face->geometry->cov[9];
}

t_face* face_deep_copy(t_face *face, bool deepcopy_geometry, bool deepcopy_colormodels)
{
	t_face* cp = (t_face*)malloc(sizeof(t_face));
	cp->geometry = face->geometry;
	cp->skincolor = face->skincolor;
	cp->bgcolor = face->bgcolor;
	cp->fgcolor = face->fgcolor;

	if (deepcopy_geometry)
		cp->geometry = ks_deep_copy(face->geometry);
	if (deepcopy_colormodels) {
		cp->skincolor = colorbins_deep_copy(face->skincolor);
		cp->bgcolor = colorbins_deep_copy(face->bgcolor);
		cp->fgcolor = colorbins_deep_copy(face->fgcolor);
	}

	return cp;
}
void face_destroy(t_face *face, bool destroy_geometry, bool destroy_colormodels) {
	if (destroy_geometry)
		ks_destroy_shared(face->geometry);
	if (destroy_colormodels) {
		colorbins_destroy(face->skincolor);
		colorbins_destroy(face->bgcolor);
		colorbins_destroy(face->fgcolor);
	}
	if (destroy_geometry && destroy_colormodels)
		free(face);
}

void face_geometry_predict(t_face *face) {

	if (face->geometry->mean[2] < 20.0)
		face->geometry->mean[2] = 20.0;

	float s2 = face->geometry->mean[2] * face->geometry->mean[2];
	face->geometry->cov[0] += 2.0 * s2;
	face->geometry->cov[6] += 2.0 * s2;
	face->geometry->cov[12] += 0.03 * s2;
	face->geometry->cov[18] += 0.1 * s2;
	face->geometry->cov[24] += 0.1 * s2;

	face->geometry->mean[3] *= 0.9;
	face->geometry->mean[4] *= 0.9;

	//if (face->geometry->cov[3]>-0.5*face->geometry->cov[0]) face->geometry->cov[3]=-0.5*face->geometry->cov[0];
	//if (face->geometry->cov[9]>-0.5*face->geometry->cov[6]) face->geometry->cov[9]=-0.5*face->geometry->cov[6];
	face->geometry->cov[15] = face->geometry->cov[3];
	face->geometry->cov[21] = face->geometry->cov[9];

	if (face->geometry->mean[3] < -face->geometry->mean[2])
		face->geometry->mean[3] = -face->geometry->mean[2];
	if (face->geometry->mean[3] > face->geometry->mean[2])
		face->geometry->mean[3] = face->geometry->mean[2];
	if (face->geometry->mean[4] < -face->geometry->mean[2])
		face->geometry->mean[4] = -face->geometry->mean[2];
	if (face->geometry->mean[4] > face->geometry->mean[2])
		face->geometry->mean[4] = face->geometry->mean[2];

	//mat_print("cov",face->geometry->cov,5,5);
}

static inline float face_get_sigma(int sxx, int sx, int sn) {
	float dxx = ((float)sx) * ((float)sx) / sn;
	return ((float)sxx - dxx) / sn;
}

void face_bgcolor_update(t_face *face, t_image *in) {
	// background color model
	colorbins_scale(face->bgcolor, 0.9);
	//colorbins_reset(face->bgcolor);
	colorbins_addsubimage(face->bgcolor, in, 0, in->width, 10, 0, in->height, 10);
}

void face_skincolor_update(t_face *face, t_image *in) {
	// skincolor model
	//colorbins_reset(face->fgcolor);

	if (fabs(face->geometry->mean[3] / face->geometry->mean[2]) > 0.3)
		return;
	if (fabs(face->geometry->mean[4] / face->geometry->mean[2]) > 0.3)
		return;

	colorbins_scale(face->fgcolor, 9.0);
	colorbins_addsubimage(face->fgcolor, in, face->geometry->mean[0] - 0.4 * face->geometry->mean[2], face->geometry->mean[0] + 0.4
			* face->geometry->mean[2], 0.4 * face->geometry->mean[2] / 20, face->geometry->mean[1] + face->geometry->mean[4] - 0.6
			* face->geometry->mean[2], face->geometry->mean[1] + face->geometry->mean[4] + 0.6 * face->geometry->mean[2], 0.6
			* face->geometry->mean[2] / 20);
	colorbins_scale(face->fgcolor, 0.1);
	colorbins_saturate_inbins(face->fgcolor, 15);
	colorbins_normalize(face->skincolor, face->fgcolor, face->bgcolor);
	colorbins_reset_outbins(face->skincolor);
}

bool face_geometry_update_haar(t_face *face, t_image *in, t_haarclass *hc, t_image *out) {
	unsigned int i;

	///////////////////////
	// SELECT BEST MATCH

	t_featurelist *fl = featurelist_haar(in, NULL, hc);

	float minscore;
	t_blobfeature *bb = fl->feat.blob;

	t_kalman_state *o = ks_create_shared(3);
	t_kalman_state *besto = ks_create_shared(3);

	float Ht[15];
	float hx[3];

	// set Ht, hx
	mat_set(Ht, 0, 15);
	Ht[0] = Ht[9] = Ht[4] = Ht[13] = 1.0;
	Ht[8] = 2.0;
	mat_multiply_transpose(hx, Ht, face->geometry->mean, 3, 5, 1, MAT_TRANSPOSE_B);

	// init kalman score
	minscore = 2.0;
	memcpy(besto->mean, hx, 3 * sizeof(float));
	mat_eye(besto->cov, 1e32, 3);

	//printf("[FaceTracker] score = [");
	for (i = 0; i < fl->nfeat; i++, bb++) {
          
          // set o
          mat_set(o->cov, 0, 9);
          o->mean[0] = bb->p[0];
          o->cov[0] = 0.5 * bb->w * bb->w;
          o->mean[1] = bb->p[1];
          o->cov[4] = 0.5 * bb->w * bb->w;
          o->mean[2] = bb->w;
          o->cov[8] = 1.0 * bb->w * bb->w;
          
          float colorscore = 0.3 / (0.01 + colorbins_scoresubimage(face->skincolor, in, bb->p[0] - bb->w / 4, bb->p[0] + bb->w / 4, bb->w
                                                                   / 20 + 1, bb->p[1] - bb->w / 4, bb->p[1] + bb->w / 4, bb->w / 20 + 1));
          float geoscore = kalman_score(face->geometry, o, hx, Ht);
          
          if (colorscore * geoscore < minscore) {
            minscore = colorscore * geoscore;
            memcpy(besto->mean, o->mean, 3 * sizeof(float));
            memcpy(besto->cov, o->cov, 9 * sizeof(float));
          }
          
          if (out) {
            draw_circle_outline(out, bb->p[0], bb->p[1], bb->w / 2.0, DRAWING_YUV_RED, 255);
            draw_line(out,bb->p[0]-bb->w*0.7,bb->p[1],bb->p[0]+bb->w*0.7,bb->p[1], DRAWING_YUV_RED, 255);
            draw_line(out,bb->p[0],bb->p[1]-bb->w*0.7,bb->p[0],bb->p[1]+bb->w*0.7, DRAWING_YUV_RED, 255);
          }
          
          //printf("%f*%f=%f,  ",colorscore,geoscore,colorscore*geoscore);
        }
        //printf("];\n");
        featurelist_destroy_custom(fl);
	//////////////
	// UPDATE
        
        //mat_scale(besto->cov,besto->cov,minscore,9);
	//mat_scale(besto->cov,besto->cov,minscore,9);
        
        if (besto->cov[0]<1e16) kalman_update_cov(face->geometry, besto, hx, Ht);
	if (besto->cov[0]<1e16) kalman_update_cov(face->geometry, besto, hx, Ht);
        
	const bool ret = (besto->cov[0]<1e16);
        
	if (out && ret) {
          draw_circle_outline(out, besto->mean[0], besto->mean[1], besto->mean[2]/2.0, DRAWING_YUV_GREEN, 255);
          draw_line(out,besto->mean[0]-besto->mean[2]*0.7,besto->mean[1],besto->mean[0]+besto->mean[2]*0.7,besto->mean[1], DRAWING_YUV_GREEN, 255);
          draw_line(out,besto->mean[0],besto->mean[1]-besto->mean[2]*0.7,besto->mean[0],besto->mean[1]+besto->mean[2]*0.7, DRAWING_YUV_GREEN, 255);
	}

	ks_destroy_shared(o);
	ks_destroy_shared(besto);

	return ret;
}

bool face_geometry_update_color(t_face *face, t_image *in, t_image *out) {
	t_kalman_state *o = ks_create_shared(3);
	float Ht[15];
	float hx[3];
	unsigned int i, j;

	// set Ht, hx
	mat_set(Ht, 0, 15);
	Ht[0] = Ht[4] = 1.0;
	Ht[8] = 1.0;
	Ht[9] = 0.1;
	Ht[13] = -0.1;
	mat_multiply_transpose(hx, Ht, face->geometry->mean, 3, 5, 1, MAT_TRANSPOSE_B);

	float sx, sy, sxx, syy, sn;

	int x1 = face->geometry->mean[0] - 1.7 * face->geometry->mean[2];
	if (x1 < 0)
		x1 = 0;
	int x2 = face->geometry->mean[0] + 1.7 * face->geometry->mean[2];
	if (x2 > (int)in->width)
		x2 = in->width;
	int y1 = face->geometry->mean[1] - 1.7 * face->geometry->mean[2];
	if (y1 < 0)
		y1 = 0;
	int y2 = face->geometry->mean[1] + 1.7 * face->geometry->mean[2];
	if (y2 > (int)in->height)
		y2 = in->height;

	// get colors
	for (i = y1, sx = 0, sy = 0, sxx = 0, syy = 0, sn = 0; i < (unsigned int)y2; i += 2)
		for (j = x1; j < (unsigned int)x2; j += 2) {
			unsigned char *ii = in->data[0] + in->rowbytes*i + HAARCOL_BYTESPIXEL*j;
			unsigned int score = colorbins_scorepixel(face->skincolor, ii);
			sx += j * score;
			sy += i * score;
			sn += score;
			sxx += j * j * score;
			syy += i * i * score;
		}
	if (sn < 1000)
		return false;

	// set o
	mat_set(o->cov, 0, 9);
	float sxn = sxx / sn - (sx / sn) * (sx / sn); //face_get_sigma(sxx, sx, sn);
	float syn = syy / sn - (sy / sn) * (sy / sn);
	o->mean[0] = sx / sn;
	o->cov[0] = 0.3 * sxn;
	o->mean[1] = sy / sn;
	o->cov[4] = 2.0 * syn;
	o->mean[2] = 2.0 * sqrt(sqrt(sxn * syn));
	o->cov[8] = 100.0 * sqrt(sxn * syn);

	//////////////
	// UPDATE

	kalman_update_cov(face->geometry, o, hx, Ht);

	if (out)
		draw_box_outline(out, o->mean[0] - o->mean[2], o->mean[1] - o->mean[2], o->mean[0] + o->mean[2], o->mean[1] + o->mean[2], DRAWING_YUV_BLUE, 255);

	return true;
}

void face_draw_geometry(t_face *face, t_image *out) {

	if (!out)
		return;

	float *m = face->geometry->mean;

	// bbox
	draw_box_outline(out, m[0] - m[2], m[1] - m[2], m[0] + m[2], m[1] + m[2], DRAWING_YUV_YELLOW, 255);

	// axes
			draw_line(out,m[0]-m[2]*1.4,m[1],m[0]+m[2]*1.4,m[1], DRAWING_YUV_YELLOW, 50);
			draw_line(out,m[0],m[1]-m[2]*1.4,m[0],m[1]+m[2]*1.4, DRAWING_YUV_YELLOW, 50);

			// nose
			draw_line(out,m[0]-m[2], m[1]-m[2],
					m[0]+m[3], m[1]+m[4], DRAWING_YUV_CYAN, 255);
			draw_line(out,m[0]-m[2], m[1]+m[2],
					m[0]+m[3], m[1]+m[4], DRAWING_YUV_CYAN, 255);
			draw_line(out,m[0]+m[2], m[1]+m[2],
					m[0]+m[3], m[1]+m[4], DRAWING_YUV_CYAN, 255);
			draw_line(out,m[0]+m[2], m[1]-m[2],
					m[0]+m[3], m[1]+m[4], DRAWING_YUV_CYAN, 255);

}

void face_draw_color(t_face *face, t_image *in, t_image *out) {

	if (!out)
		return;

	colorbins_scoreimage(face->skincolor, in, out);
}
