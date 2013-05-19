#include <glib.h> // for gboolean
#include <opencv/cv.h>


struct bbox{
  double x,y,w,h;
};


int calculate_pyrlk(IplImage *inputimage, IplImage *inputimage2, int num_vertexes, 
                    CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error, 
                    CvPoint2D32f* vertexesB);
int calc_eisemann_durand_luminance( IplImage* imgin, IplImage* imgout );
void draw_subdiv_edge( IplImage* img, CvSubdiv2DEdge edge, CvScalar color );

void draw_pyrlk_vectors(IplImage *image, int num_vertexes, 
                        CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error,  
                        CvPoint2D32f* vertexesB  );
void draw2_pyrlk_vectors(IplImage *image, int num_vertexes, 
                        CvPoint2D32f* vertexesA, char *vertexes_found, float *vertexes_error,  
                        CvPoint2D32f* vertexesB, CvPoint *center  );
void draw_points(IplImage *image, int num_vertexes, CvPoint2D32f* vertexes, CvScalar colour );





gboolean is_point_in_bbox (CvPoint p, struct bbox box );

