
#ifndef __GST_KALMANTRACKING_H__
#define __GST_KALMANTRACKING_H__

#include <opencv/cv.h>

struct kernel_internal_state{
  CvKalman* k;
  
  int state_vector_dim;
  int meas_vector_dim;
  int control_vector_dim;
  
  CvMat*     x_k;  // state vector, dimension state_vector_dim
  CvMat*     z_k;  // meas vector, dimension meas_vector_dim
  CvMat*     w_k;  // meas noise (==process) noise, dim meas_vector_dim

  CvMat*     F;    // transition matrix,  dim (state_vector_dim x state_vector_dim)
  CvMat*     H;    // meas matrix, dim (output_vector_dim x state_vector_dim)
  CvMat*     Q;    // process noise (cov) matrix, dim (state_vector_dim x state_vector_dim)
  CvMat*     R;    // meas error cov matrix, dim (meas_vector_dim x meas_vector_dim)
  CvMat*     P;    // estimate cov matrix, dim (state_vector_dim x state_vector_dim)

};


#endif // #define __GST_KALMANTRACKING_H__
