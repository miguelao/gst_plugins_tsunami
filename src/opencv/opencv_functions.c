


#include "opencv_functions.h"
#include <stdio.h>  //printf


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/// \function calculate_pyrlk
/// \param[in]  inputimage: single channel image (IplImage), is the frame t-1
/// \param[in]  inputimage2: single channel image (IplImage), is the frame in t
/// \param[in]  num_vertexes: amount of features to track
/// \param[out] vertexesA: emtpy array of CvPoint2D32f that will be filled in with the vertexes' coordinates
/// \param[out] vertexes_found: char array of num_vertexes length, output: 1=found, 0= not found
/// \param[out] vertexes_error: float array of num_vertexes length, with a number describing the tracking confidence
/// \param[out] vertexesB: emtpy array of CvPoint2D32f that will be filled in with the propagated vertexes' coordinates
///
int calculate_pyrlk(IplImage *inputimage, IplImage *inputimage2, int num_vertexes, 
                    CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error, CvPoint2D32f* vertexesB)
{

  CvSize img_sz = cvGetSize( inputimage );
  int win_size = 15;
  // Get the features for tracking
  IplImage* eig_image = cvCreateImage( img_sz, IPL_DEPTH_32F, 1 );
  IplImage* tmp_image = cvCreateImage( img_sz, IPL_DEPTH_32F, 1 );
  int corner_count = num_vertexes;

  // good features to track are taken from the Grey image in t (not in t+1 )
  cvGoodFeaturesToTrack( inputimage, eig_image, tmp_image, vertexesA, &corner_count,
                         0.05,   // Multiplier for the maxmin eigenvalue; minimal accepted q of image corners.
                         1.0,    // Limit, specifying minimum possible distance between returned corners;
                         0,      // Region of interest. 
                         3,      // Size of the averaging block
                         0,      // If nonzero, Harris operator 
                         0.04 ); // Free parameter of Harris detector; used only if use_harris≠0
#if 0
  cvFindCornerSubPix( inputimage, vertexesA, corner_count, cvSize( win_size, win_size ),
                      cvSize( -1, -1 ), cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.03 ) );
#endif

  CvSize pyr_sz = cvSize( inputimage->width+8, inputimage->height/3 );

  IplImage* pyrA = cvCreateImage( pyr_sz, IPL_DEPTH_32F, 1 );
  IplImage* pyrB = cvCreateImage( pyr_sz, IPL_DEPTH_32F, 1 );

  // Call Lucas Kanade algorithm
  cvCalcOpticalFlowPyrLK( inputimage, inputimage2, pyrA, pyrB, vertexesA, vertexesB, corner_count, 
                          cvSize( win_size, win_size ), 5, vertexes_found, vertexes_error,
                          cvTermCriteria( CV_TERMCRIT_ITER | CV_TERMCRIT_EPS, 20, 0.3 ), 0 );

  return(corner_count);
}



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
/// \function calculate_pyrlk
/// \param[in]  inputimage: single channel image (IplImage), is the frame t-1
/// \param[in]  inputimage2: single channel image (IplImage), is the frame in t
/// \param[in]  num_vertexes: amount of features to track
/// \param[out] vertexesA: emtpy array of CvPoint2D32f that will be filled in with the vertexes' coordinates
/// \param[out] vertexes_found: char array of num_vertexes length, output: 1=found, 0= not found
/// \param[out] vertexes_error: float array of num_vertexes length, with a number describing the tracking confidence
/// \param[out] vertexesB: emtpy array of CvPoint2D32f that will be filled in with the propagated vertexes' coordinates
///
//int calculate_delaunay(IplImage *inputimage, IplImage *inputimage2, int num_vertexes, 
//                    CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error, CvPoint2D32f* vertexesB)
//{
//
//#if 0
//  // try to draw the delaunay of the found points  
//  CvRect rect ={0, 0, img_sz.width, img_sz.height};
//  CvMemStorage *storage;
//  storage=cvCreateMemStorage(0);
//  CvSubdiv2D *subdiv;
//  
//  subdiv = cvCreateSubdiv2D( CV_SEQ_KIND_SUBDIV2D, sizeof(*subdiv), sizeof(CvSubdiv2DPoint),
//                             sizeof(CvQuadEdge2D), storage);
//  cvInitSubdivDelaunay2D( subdiv, rect);
//
//  for( int i=0; i<corner_count; i++ ){
//    if(!( features_found[i]==0 || feature_errors[i]>550) ){
//      printf(" %f- %f (%d)\n", cornersA[i].x, cornersA[i].y, i);
//      if( (cornersA[i].x <= img_sz.width) && (cornersA[i].y <= img_sz.height))
//        cvSubdivDelaunay2DInsert(subdiv, cornersA[i]);
//      CvPoint p0 = cvPoint( cvRound( cornersA[i].x ), cvRound( cornersA[i].y ) );
//      cvLine( pyrlk->cvRGBout, p0, p0, CV_RGB(0,0,255), 1 );
//    }
//  }
//  
//  CvSeqReader  reader;
//  int i, total = subdiv->edges->total;
//  int elem_size = subdiv->edges->elem_size;
//  cvStartReadSeq( (CvSeq*)(subdiv->edges), &reader, 0 );
//
//  for( i = 0; i < total; i++ )
//  {
//    CvQuadEdge2D* edge = (CvQuadEdge2D*)(reader.ptr);
//    
//    if( CV_IS_SET_ELEM( edge ))
//    {
//      draw_subdiv_edge( pyrlk->cvRGBout, (CvSubdiv2DEdge)edge + 1, CV_RGB(0, 180, 0) );
//      draw_subdiv_edge( pyrlk->cvRGBout, (CvSubdiv2DEdge)edge, CV_RGB( 0,180,0) );
//    }
//    
//    CV_NEXT_SEQ_ELEM( elem_size, reader );
//  }
//
//
//  cvReleaseMemStorage( &storage );
//#endif
//
//}



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
int calc_eisemann_durand_luminance( IplImage* imgin, IplImage* imgout )
{

#define EISEMANN_DURAND_I(R,G,B) ( ((R*R)/(R+G+B)) + ((G*G)/(R+G+B)) + ((B*B)/(R+G+B)) )

  if( (imgin-> width != imgout->width) ||
      (imgin-> height != imgout->height)){
    printf(" Images must be of the same size! (they are not)\n");
    return(-1);
  }

  for (int i=0; i < imgin->width; i++){
    for (int j=0; j < imgin->height; j++){
      imgout->imageData[ (i*imgin->width)+j ] =
        EISEMANN_DURAND_I( imgin->imageData[ 3*((i*imgin->width)+j)     ],
                           imgin->imageData[ 3*((i*imgin->width)+j) + 1 ],
                           imgin->imageData[ 3*((i*imgin->width)+j) + 2 ]);
    }
  }
  return(0);
}





//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void draw_subdiv_edge( IplImage* img, CvSubdiv2DEdge edge, CvScalar color )
{
    CvSubdiv2DPoint* org_pt;
    CvSubdiv2DPoint* dst_pt;
    CvPoint2D32f org;
    CvPoint2D32f dst;
    CvPoint iorg, idst;

    org_pt = cvSubdiv2DEdgeOrg(edge);
    dst_pt = cvSubdiv2DEdgeDst(edge);

    if( org_pt && dst_pt )
    {
        org = org_pt->pt;
        dst = dst_pt->pt;

        iorg = cvPoint( cvRound( org.x ), cvRound( org.y ));
        idst = cvPoint( cvRound( dst.x ), cvRound( dst.y ));

        cvLine( img, iorg, idst, color, 1, CV_AA, 0 );
    }
}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void draw_pyrlk_vectors(IplImage *image, int num_vertexes, 
                        CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error,  
                        CvPoint2D32f* vertexesB  )
{
  // Make an image of the results
  for( int i=0; i < num_vertexes; i++ ){
    if( vertexes_found[i]==0 || vertexes_error[i]>550){
      CvPoint p0 = cvPoint( cvRound( vertexesA[i].x ), cvRound( vertexesA[i].y ) );
      cvLine( image, p0, p0, CV_RGB(0,0,255), 1, 8, 0 );
    }else{
      CvPoint p0 = cvPoint( cvRound( vertexesA[i].x ), cvRound( vertexesA[i].y ) );
      CvPoint p1 = cvPoint( cvRound( vertexesB[i].x ), cvRound( vertexesB[i].y ) );
  
      // These lines below increase the size of the line by 3
      double angle;     angle = atan2( (double) p0.y - p1.y, (double) p0.x - p1.x );
      double modulus;	modulus = sqrt( (p0.y - p1.y)*(p0.y - p1.y) + (p0.x - p1.x)*(p0.x - p1.x) );
      p1.x = (int) (p1.x - 3 * modulus * cos(angle));
      p1.y = (int) (p1.y - 3 * modulus * sin(angle));
      // they can be commented out w/o any problem
      
      if( modulus < 10 && modulus > 0.5){
        cvLine( image, p0, p1, CV_RGB(0,255,0), 1, 8, 0 );      
      }
    }
  }

}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void draw2_pyrlk_vectors(IplImage *image, int num_vertexes, 
                         CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error,  
                         CvPoint2D32f* vertexesB, CvPoint *center  )
{
  CvPoint p0 = cvPoint( cvRound( center->x ), cvRound( center->y ) );
  int avgx=0, avgy=0, cnt=0;
  // Make an image of the results
  for( int i=0; i < num_vertexes; i++ ){
    if( vertexes_found[i]==0 || vertexes_error[i]>550){
      CvPoint p0 = cvPoint( cvRound( vertexesA[i].x ), cvRound( vertexesA[i].y ) );
      //cvLine( image, p0, p0, CV_RGB(0,0,255), 3 );
    }else{
      CvPoint p0 = cvPoint( cvRound( vertexesA[i].x ), cvRound( vertexesA[i].y ) );
      CvPoint p1 = cvPoint( cvRound( vertexesB[i].x ), cvRound( vertexesB[i].y ) );
  
      // pass to modulus representation
      double angle;     angle = atan2( (double) p0.y - p1.y, (double) p0.x - p1.x );
      double modulus;	modulus = sqrt( (p0.y - p1.y)*(p0.y - p1.y) + (p0.x - p1.x)*(p0.x - p1.x) );
      // they can be commented out w/o any problem
      
      if( modulus < 10 && modulus > 0.25){        
        cvLine( image, p0, cvPoint(p1.x+10*cos(angle), p1.y+10*sin(angle)), CV_RGB(0,0,255), 1, 8, 0 );      

        cnt++; avgx+=(p1.x-p0.x); avgy+=(p1.y-p0.y);
      }
    }
  }
  if( cnt ){
    printf(" avg vector of %d: (%f,%f) (around %d,%d) \n", cnt, (float)avgx/cnt, (float)avgy/cnt,
           center->x, center->y );
    CvPoint p1 = cvPoint( center->x + 10*cvRound( avgx/cnt ), center->y+10*cvRound( avgy/cnt ) );
    cvLine( image, p0, p1, CV_RGB(255,255,0), 1, 8, 0 ); 
 
    center->x += cvRound( avgx/cnt ); center->y += cvRound( avgy/cnt ) ;
  }
  cvLine( image, p0, p0, CV_RGB(255,255,255), 1, 8, 0 ); 

}


//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void draw_points(IplImage *image, int num_vertexes, CvPoint2D32f* vertexes, CvScalar colour  )
{
  
  for( int i=0; i < num_vertexes; i++ ){
      CvPoint p0 = cvPoint( cvRound( vertexes[i].x ), cvRound( vertexes[i].y ) );
      cvLine( image, p0, p0, colour , 1, 8, 0 );
  }
}


//////////////////////////////////////////////////////////////////////////////
gboolean is_point_in_bbox (CvPoint p, struct bbox box )
{
  if( (p.x >= box.x-0.5*box.w) && (p.x<= (box.x+0.5*box.w)) &&
      (p.y >= box.y-0.5*box.h) && (p.y<= (box.y+0.5*box.h)))
    return(TRUE);
  else
    return(FALSE);
}

