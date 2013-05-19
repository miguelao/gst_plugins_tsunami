
#include <stdlib.h> // malloc

struct bbox_double{
  double x,y,w,h;
};

struct bbox_int{
  int x,y,w,h;
};


inline struct bbox_double* bbox_double_create(void) { 
  return( (struct bbox_double*) malloc(sizeof(struct bbox_double))); 
}

inline struct bbox_int* bbox_int_create(void) { 
  return( (struct bbox_int*) malloc(sizeof(struct bbox_int))); 
}

inline void bbox_double_to_int(struct bbox_int* bboxint, struct bbox_double* bboxdou){
  bboxint->x = (int) bboxdou->x;
  bboxint->y = (int) bboxdou->y;
  bboxint->w = (int) bboxdou->w;
  bboxint->h = (int) bboxdou->h;
}

inline void bbox_int_to_double(struct bbox_double* bboxdou, struct bbox_int* bboxint){
  bboxdou->x = (double) bboxint->x;
  bboxdou->y = (double) bboxint->y;
  bboxdou->w = (double) bboxint->w;
  bboxdou->h = (double) bboxint->h;
}

// EOF
