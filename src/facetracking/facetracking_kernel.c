
/*#######################################################################
 #                                                                      #
 #  INCLUDES                                                            #
 #                                                                      #
 ######################################################################*/
#include "facetracking_kernel.h"


/*#######################################################################
 #                                                                      #
 # DEFINES, MACROS                                                      #
 #                                                                      #
 ######################################################################*/

/*#######################################################################
 #                                                                      #
 # EXTERN CONSTANT AND EXTERN VARIABLE DECLARATIONS (discouraged)       #
 #                                                                      #
 ######################################################################*/

/*#######################################################################
 #                                                                      #
 #  TYPE DEFINITIONS                                                    #
 #                                                                      #
 ######################################################################*/

/*#######################################################################
 #                                                                      #
 #  INTERFACE CONSTANT AND INTERFACE VARIABLE DEFINITIONS               #
 #                                                                      #
 ######################################################################*/

/*#######################################################################
 #                                                                      #
 #  STATIC CONSTANT AND STATIC VARIABLE DEFINITIONS                     #
 #                                                                      #
 ######################################################################*/
////////////////////////////////////////////////////////////////////////////////


/*#######################################################################
 #                                                                      #
 # INTERNAL FUNCTION PROTOTYPES                                         #
 #                                                                      #
 ######################################################################*/

struct bbox_int iterate_kalman( struct kernel_internal_state* kernel, 
                                struct bbox_double* meas, 
                                int nobjects);

/*#######################################################################
 #                                                                      #
 #  INTERFACE FUNCTION DEFINITIONS                                      #
 #                                                                      #
 ######################################################################*/

////////////////////////////////////////////////////////////////////////////////
int facetracking_kernel(GstFacetracker3 *facetracker3)
{
  struct bbox_double p, p2;
  guint32  ntorsos, nfaces, nsidefaces, trackingonwhat;

  CvScalar colour=RED; trackingonwhat=0;
  ntorsos = nfaces = nsidefaces = 0;

  // algorithmic idea: we detect the face. No face detected -> torso. 
  // If a face is detected, we run the profile classifier. If this detects sth
  // different than the normal face by over N pixels, we prefer this.
  // The chosen bbox goes into a kalman to smooth out the nonsenses.

  haarwrapper_detect( facetracker3->hc, facetracker3->image_rgb, &p, &nfaces ); 
  //  p --> "raw" haar detection output, if nfaces > 0
  if( nfaces>0 ) {colour=GREEN; trackingonwhat=1;}

  //// now try and find a sidewards looking face.
  //haarwrapper_detect( facetracker3->hc3, facetracker3->image_rgb, &p2, &nsidefaces ); 
  //
  //if( nsidefaces > 0 ){
  //  // if the distance between them is larger than X, prefer the sidewards
  //  if( (abs(p2.x - p.x) + abs(p2.y - p.y)) > 25.0 ){
  //    colour=BLUE; trackingonwhat=2;
  //    memcpy( &p, &p2, sizeof(struct bbox_double));
  //  }    
  //}
  //else{ // flip vertically the image and try again.
  //  haarwrapper_flip( facetracker3->hc3, facetracker3->image_rgb, facetracker3->image_rgb2 );
  //  haarwrapper_detect( facetracker3->hc3, facetracker3->image_rgb2, &p2, &nsidefaces ); 
  //  if( nsidefaces > 0 )
  //    // if the distance between them is larger than X, prefer the sidewards
  //    if( (abs(p2.x - p.x) + abs(p2.y - p.y)) > 25.0 ){
  //      colour=PURPLE; trackingonwhat=3;
  //      memcpy( &p, &p2, sizeof(struct bbox_double));
  //    }    
  //}

  if( ( nfaces == 0 ) ){//& ( nsidefaces == 0 )){
    
    // no face -> track torso
    if( facetracker3->hc2 ){
      haarwrapper_detect( facetracker3->hc2, facetracker3->image_rgb, &p, &ntorsos ); 
      if( ntorsos > 0 ){
        p.x  = (p.x + (p.w/2)) - (0.35 *(p.w/2)) ;
        p.y  = (p.y + (p.h/2)) - (0.525*(p.w/2)) ;
        p.w *= 0.35;
        p.h *= 0.40;
        colour = YELLOW; trackingonwhat=4;
      }
    }
  }

  if( (nfaces>0) || (nsidefaces>0) || (ntorsos>0)){
    struct bbox_int k_face = iterate_kalman( facetracker3->facek, &p, 1);
    memcpy( facetracker3->face, &k_face, sizeof( struct bbox_int ));
  }
  else{
    //bbox_double_to_int( facetracker3->face, &p );
  }

  haarwrapper_drawbox( facetracker3->cvBGR, facetracker3->face , colour);
  if( 0 == trackingonwhat )
    haarwrapper_drawtext(facetracker3->cvBGR, facetracker3->face , colour, (char*)"tracking"); 
  else if( 1 == trackingonwhat )
    haarwrapper_drawtext(facetracker3->cvBGR, facetracker3->face , colour, (char*)"face"); 
  else if( 2 == trackingonwhat )
    haarwrapper_drawtext(facetracker3->cvBGR, facetracker3->face , colour, (char*)"profile"); 
  else if( 3 == trackingonwhat )
    haarwrapper_drawtext(facetracker3->cvBGR, facetracker3->face , colour, (char*)"profile2"); 
  else if( 4 == trackingonwhat )
    haarwrapper_drawtext(facetracker3->cvBGR, facetracker3->face , colour, (char*)"torso"); 

//  //////////////////////////////////////////////////////////////////////////////
//  //////////////////////////////////////////////////////////////////////////////
//  //haarwrapper_detect( facetracker3->hc2, facetracker3->image_rgb, &p, &ntorsos ); 
//  //struct bbox_int k_torso = iterate_kalman( facetracker3->torsok, &p, ntorsos);
//  //bbox_double_to_int( facetracker3->torso, &p );
//  ////printf("t%3.0f, %3.0f, %3.0f, %3.0f\n", p.x, p.y, p.w, p.h);
//
//
//  //////////////////////////////////////////////////////////////////////////////
//  //////////////////////////////////////////////////////////////////////////////
//  // in p, right here, we'd have the "raw" haar detection output, now we need
//  // the kalman tracking
//  haarwrapper_detect( facetracker3->hc, facetracker3->image_rgb, &p, &nfaces ); 
//  //if(nfaces>0)printf("F%3.0f %3.0f %3.0f %3.0f\n", p.x, p.y, p.w, p.h);
//  struct bbox_int k_face = iterate_kalman( facetracker3->facek, &p, nfaces);
//  bbox_double_to_int( facetracker3->face, &p ); // save result in facetracker internals
////  if(nfaces>0)printf("F%3d %3d %3d %3d\n", 
////                     facetracker3->face->x, facetracker3->face->y, 
////                     facetracker3->face->w, facetracker3->face->h);
//
//  //////////////////////////////////////////////////////////////////////////////
//  //////////////////////////////////////////////////////////////////////////////
//  haarwrapper_detect( facetracker3->hc3, facetracker3->image_rgb, &p, &nsidefaces ); 
//  //if(nsidefaces>0)printf("S%3.0f %3.0f %3.0f %3.0f\n", p.x, p.y, p.w, p.h);
//  struct bbox_int k_side = iterate_kalman( facetracker3->sidek, &p, nsidefaces);
//  bbox_double_to_int( facetracker3->side, &p ); // save result in facetracker internals
////  if(nsidefaces>0)printf("S%3d %3d %3d %3d\n", 
////                     facetracker3->side->x, facetracker3->side->y, 
////                     facetracker3->side->w, facetracker3->side->h);
//
//  if( (nsidefaces>0) && (nfaces>0) ){
//    printf(" *** %d %d %d %d\n", 
//           abs(facetracker3->side->x - facetracker3->face->x),
//           abs(facetracker3->side->y - facetracker3->face->y),
//           abs(facetracker3->side->h - facetracker3->face->h),
//           abs(facetracker3->side->w - facetracker3->face->w));
//  }
//
  return(0);
}

////////////////////////////////////////////////////////////////////////////////
int facetracking_display_info(GstFacetracker3 *facetracker3)
{

  // overwrite the RGB input-output buffer with the results of our drawbox'es
  cvCopy(facetracker3->cvBGR, facetracker3->cvBGR_input, NULL);
  
  return(0);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
struct kernel_internal_state* facetracking_init(void)
{
  // create the internal kernel state structure
  struct kernel_internal_state* kernel = 
    (struct kernel_internal_state*)malloc(sizeof(struct kernel_internal_state));

  //////////////////////////////////////////////////////////////////////////////
  // What is the size of the Kalman filter?
  //  The whole thing comes from 
  //  http://blog.cordiner.net/2011/05/03/object-tracking-using-a-kalman-filter-matlab/
  // The state tracked is (x,y)top (x,y)bottom (vx,vy)
  // Note that vx,vy would merge size changes and movement ... so maybe to reconsider
  kernel->state_vector_dim = 6;

  // we get (x,y)top (x,y)bottom from the sensors, so meas_dim =4
  kernel->meas_vector_dim = 4;

  // control vector not present in this formulation
  kernel->control_vector_dim = 0;


  //////////////////////////////////////////////////////////////////////////////
  // FIRE!! Go to ritazza and get a coffee. Then run to the bathroom
  kernel->k = cvCreateKalman( kernel->state_vector_dim, 
                              kernel->meas_vector_dim,
                              kernel->control_vector_dim);
  
  //////////////////////////////////////////////////////////////////////////////
  // Measurement vector
  kernel->x_k = cvCreateMat( kernel->state_vector_dim , 1, CV_32FC1 );
  // Measurement vector
  kernel->z_k = cvCreateMat( kernel->meas_vector_dim , 1, CV_32FC1 );
  // Measurement noise (a.k.a. Process noise) 
  kernel->w_k = cvCreateMat( kernel->meas_vector_dim , 1, CV_32FC1 );
  // transition matrix F
  kernel->F = cvCreateMat( kernel->state_vector_dim , kernel->state_vector_dim, CV_32FC1 );
  cvSetIdentity( kernel->F, cvRealScalar(1) );
  cvmSet( kernel->F, 0, 4 , 1.0);
  cvmSet( kernel->F, 1, 5 , 1.0);
  cvmSet( kernel->F, 2, 4 , 1.0);
  cvmSet( kernel->F, 3, 5 , 1.0);
  printf("F \n");
  for( int i=0; i< kernel->state_vector_dim ; i++){
    for( int j=0; j< kernel->state_vector_dim ; j++){
      printf(" %f ", cvmGet(kernel->F, i, j));
    }
    printf("\n");
  }  
  // measurement matrix H
  kernel->H = cvCreateMat( kernel->meas_vector_dim , kernel->state_vector_dim, CV_32FC1 );
  cvSetIdentity( kernel->H, cvRealScalar(1) );
  printf("H \n");
  for( int i=0; i< kernel->meas_vector_dim ; i++){
    for( int j=0; j< kernel->state_vector_dim ; j++){
      printf(" %f ", cvmGet(kernel->H, i, j));
    }
    printf("\n");
  }
  // process noise cov Q
  // According to the model, the number depends on expected acceleration of the pixels
  // so reducing this numbers makes the kalman less responsive to changes
  kernel->Q = cvCreateMat( kernel->state_vector_dim , kernel->state_vector_dim, CV_32FC1 );
  double a[] = { 0.025,     0,     0,     0,  0.05,    0,
                     0, 0.025,     0,     0,     0, 0.05,   
                     0,     0, 0.025,     0,  0.05,    0,
                     0,     0,     0, 0.025,     0, 0.05,   
                  0.05,     0,  0.05,     0,   0.1,    0,
                     0,  0.05,     0,  0.05,     0,  0.1};
  printf("Q \n");
  for( int i=0; i< kernel->state_vector_dim ; i++){
    for( int j=0; j< kernel->state_vector_dim ; j++){
      cvmSet( kernel->Q, i, j ,  a[i*kernel->state_vector_dim + j] / 10 ); // !!!
      printf(" %f ", cvmGet(kernel->Q, i, j));
    }
    printf("\n");
  }
  // meas error cov R
  // Numbers here mean sigma of measured pixel pos (needs to be squared).
  kernel->R = cvCreateMat( kernel->meas_vector_dim , kernel->meas_vector_dim, CV_32FC1 );
  cvSetIdentity( kernel->R, cvRealScalar(70.0) );
  printf("R \n");
  for( int i=0; i< kernel->meas_vector_dim ; i++){
    for( int j=0; j< kernel->meas_vector_dim ; j++){
      printf(" %f ", cvmGet(kernel->R, i, j));
    }
    printf("\n");
  }
  // estimate cov matrix P
  kernel->P = cvCreateMat( kernel->state_vector_dim , kernel->state_vector_dim, CV_32FC1 );
  cvSetIdentity( kernel->P, cvRealScalar(100000) );
  printf("P \n");
  for( int i=0; i< kernel->state_vector_dim ; i++){
    for( int j=0; j< kernel->state_vector_dim ; j++){
      printf(" %f ", cvmGet(kernel->P, i, j));
    }
    printf("\n");
  }
  printf("---------------------------\n");


  kernel->k->transition_matrix     = kernel->F;
  kernel->k->measurement_matrix    = kernel->H;
  kernel->k->process_noise_cov     = kernel->Q;
  kernel->k->measurement_noise_cov = kernel->R;
  kernel->k->error_cov_post        = kernel->P;


//  cvCopy( kernel->F, kernel->k->transition_matrix     );
//  cvCopy( kernel->H, kernel->k->measurement_matrix    );
//  cvCopy( kernel->Q, kernel->k->process_noise_cov     );
//  cvCopy( kernel->R, kernel->k->measurement_noise_cov );
//  cvCopy( kernel->P, kernel->k->error_cov_post        );
//
  //double init[] = { 50, 50, 100, 100, 0, 0 };
  //kernel->k->state_post = cvCreateMat( kernel->state_vector_dim, 1, CV_32FC1 );
  //cvInitMatHeader( kernel->k->state_post, kernel->state_vector_dim, 1, CV_32FC1, init);

  return(kernel);
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct bbox_int iterate_kalman( struct kernel_internal_state* kernel, struct bbox_double* meas, int nobjects)
{

  //////////////////////////////////////////////////////////////////////////////
  // Kalman predict
  const CvMat* y_k = cvKalmanPredict( kernel->k , NULL );
 
  //////////////////////////////////////////////////////////////////////////////
  if( nobjects > 0 ){

    //move kalman meas into kalman formulation
    cvmSet( kernel->z_k, 0 , 0, meas->x);
    cvmSet( kernel->z_k, 1 , 0, meas->y);
    cvmSet( kernel->z_k, 2 , 0, meas->x + meas->w);
    cvmSet( kernel->z_k, 3 , 0, meas->y + meas->h);
   
    // update the meas (output)  [z]k = [[H]] [x]k + [z]k <-- NOT NEEDED
    //cvMatMulAdd( kernel->H, kernel->x_k, kernel->z_k, kernel->z_k );
   
    // Adjust kalman filter state
    cvKalmanCorrect( kernel->k, kernel->z_k );
  }
  else{
    // if there is no meas, best is to input nothing to kalman correction step
  }

  struct bbox_int pred_as_bbox;
  pred_as_bbox.x = cvmGet(y_k, 0, 0);
  pred_as_bbox.y = cvmGet(y_k, 1, 0);
  pred_as_bbox.w = cvmGet(y_k, 2, 0) - pred_as_bbox.x;
  pred_as_bbox.h = cvmGet(y_k, 3, 0) - pred_as_bbox.y;

//  printf(" %3d, %3d, %3d, %3d\n", pred_as_bbox.x, pred_as_bbox.y, pred_as_bbox.w, pred_as_bbox.h);

//  printf("P \n");
//  for( int i=0; i< kernel->state_vector_dim ; i++){
//    for( int j=0; j< kernel->state_vector_dim ; j++){
//      printf(" %f ", cvmGet(kernel->k->error_cov_post, i, j));
//    }
//   printf("\n");
//  }
//  printf(" ** %f\n", cvTrace(kernel->k->error_cov_post).val[0]);

  return( pred_as_bbox );
}



//EOF///////////////////////////////////////////////////////////////////////////
