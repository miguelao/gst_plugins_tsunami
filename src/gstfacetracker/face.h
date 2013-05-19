#ifndef LIB_IMAGE_FEATURES_FACE_H
#define LIB_IMAGE_FEATURES_FACE_H

#include "defines.h"
#include "kalman.h"
#include "mtxcore.h"
#include "colorbins.h"
#include "imagefeature.h"

#define FACE_MIN_RADIUS 10.0

typedef struct {
	t_kalman_state* geometry;
	t_colorbins* skincolor;
	t_colorbins* bgcolor;
	t_colorbins* fgcolor;
} t_face;

int face_create_bins(t_face* face, t_image *image, unsigned char ymean = 150, unsigned char yvar = 90, unsigned char umean = 125, unsigned char uvar = 25,
		unsigned char vmean = 145, unsigned char vvar = 25);
t_face* face_create(t_image *image);
void face_reset(t_face *face, t_image *image);
void face_reset_geometry(t_face *face, t_image *image);
t_face* face_deep_copy(t_face *face, bool deepcopy_geometry, bool deepcopy_colormodels);
void face_destroy(t_face *face, bool destroy_geometry, bool destroy_colormodels);

void face_geometry_predict(t_face *face);
bool face_geometry_update_haar(t_face *face, t_image *in, t_haarclass *hc, t_image *out);
bool face_geometry_update_color(t_face *face, t_image *in, t_image *out);
void face_skincolor_update(t_face *face, t_image *in);
void face_bgcolor_update(t_face *face, t_image *in);

void face_draw_geometry(t_face *face, t_image *out);
void face_draw_color(t_face *face, t_image *in, t_image *out);

#endif

