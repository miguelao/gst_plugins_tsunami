
#ifndef __GST_FACETRACKING_KERNEL_H__
#define __GST_FACETRACKING_KERNEL_H__

#include "kalmantracking.h"
#include "gstfacetracker3.h"

int facetracking_kernel(GstFacetracker3 *facetracker3);
int facetracking_display_info(GstFacetracker3 *facetracker3);
struct kernel_internal_state*
    facetracking_init(void);

#endif // #define __GST_FACETRACKING_KERNEL_H__
