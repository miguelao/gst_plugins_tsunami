#ifndef __FACEDETECTLIB_H
#define __FACEDETECTLIB_H

#ifdef __cplusplus
extern "C" {
#endif 

#define _HAS_EXCEPTIONS 0 

void *InitFaceDetector2(char *cascade_name, int minfacesize);
char *DetectFacesV2(IplImage *img, void *buf);
void  ReleaseFaceDetector(void *buf);

#ifdef __cplusplus
}
#endif 

#endif //__FACEDETECTLIB_H