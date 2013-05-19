#include <opencv/cv.h>

#define FTRACK_HISTORY_FRAMES 3 // history frames for face x,y
#define FTRACK_HISTORY_FRAMES_RAD FTRACK_HISTORY_FRAMES*3 // history frames for face radius

class ftrack
{
 public:
  ftrack();
  ~ftrack();

  void init();

  // set latest IPP/OCV detected coordinates, and get smoothened coords back
  bool updateFaceCoords(CvRect in, CvRect& out, bool isFace=true);

  void reset();

 private:
  int face_x[FTRACK_HISTORY_FRAMES];
  int face_y[FTRACK_HISTORY_FRAMES];
  int face_w[FTRACK_HISTORY_FRAMES_RAD];
  int face_h[FTRACK_HISTORY_FRAMES_RAD];

  long nrFrames;
  bool isReset,faceSeen;
};
