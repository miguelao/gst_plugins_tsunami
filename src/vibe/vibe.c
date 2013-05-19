#include "vibe.h"

#define VIBE_NRANDOM 7777

#include <string.h>

t_u_int8 expt[] = {0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  		  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
		  0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
		  1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
		  2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
		  3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,
		  4,   4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,   6,   6,
		  6,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,   9,   9,   9,   9,   9,
		  10,  10,  10,   11,   11,   11,   11,   12,   12,   12,   13,   13,   13,   14,   14,   14,
		  15,  15,  16,   16,   16,   17,   17,   18,   18,   19,   19,   20,   20,   21,   21,   22,
		  23,  23,  24,   24,   25,   26,   26,   27,   28,   28,   29,   30,   31,   31,   32,   33,
		  34,  35,  36,   37,   38,   39,   40,   41,   42,   43,   44,   45,   46,   47,   49,   50,
		  51,  53,  54,   55,   57,   58,   60,   61,   63,   64,   66,   68,   70,   71,   73,   75,
		  77,  79,  81,    83,    85,    88,    90,    92,    94,    97,    99,   102,   105,   107,   110,   113,
		  116, 119, 122,   125,   128,   132,   135,   138,   142,   146,   149,   153,   157,   161,   165,   170,
		  174, 178, 183,   188,  193,   197,   203,   208,   213,   219,   224,   230,   236,  242,   248,   255};


t_vibe *vibe_create(t_u_int32 width, t_u_int32 height, t_u_int8 nsamples)
{
  t_vibe *vb = (t_vibe*)malloc(sizeof(t_vibe));
  t_u_int32 i;

  vb->width = width;
  vb->height = height;
  vb->nsamples = nsamples;

  vb->cardinality = nsamples/3;
  vb->sigma_y = 24;
  vb->sigma_u = 12;
  vb->sigma_v = 12;

  vb->lspeed_fg = 80;//4096;
  vb->lspeed_bg = 10; //128;

  vb->cdec_fg = 1;
  vb->cinc_bg = 100;

  vb->lastscore = 0.5;

  vb->random = (t_u_int32*)malloc(VIBE_NRANDOM*sizeof(t_u_int32));
  vb->irandom = 0;
  for (i=0; i<VIBE_NRANDOM; i++) vb->random[i] = rand()%255;

  vb->model = (t_u_int8*)malloc(3*vb->width*vb->height*vb->nsamples*sizeof(t_u_int8));
  memset(vb->model,0,3*vb->width*vb->height*vb->nsamples*sizeof(t_u_int8));
  vb->conf = (t_u_int8*)malloc(vb->width*vb->height*sizeof(t_u_int8));
  memset(vb->conf,0,vb->width*vb->height*sizeof(t_u_int8));

  return vb;
}
void vibe_destroy(t_vibe *vb) 
{
  free(vb->model);
  free(vb->conf);
  free(vb->random);
  free(vb);
}

t_u_int32 vibe_getrandom(t_vibe *vb) {
  t_u_int32 r = vb->random[vb->irandom];
  vb->irandom++; 
  if (vb->irandom==VIBE_NRANDOM) 
    vb->irandom=0;
  return r;
}

// jvw mcs 14/aug/12
// per pixel we need to score the image agains the model, meaning compare the
// distribution parameters yuv between both and if the relative values are
// larger than a certain threshold we admit it has changed (fg) otherwise is
// not changed, so is background. The vibe->model is thus having values 
// {0, alpha, 255}, where alpha is sigma* alpha where alpha \in [-0.5,0.5]
// and sigma is a constant from the beginning
void vibe_initmodel(t_vibe *vb, IplImage *image)
{
  t_u_int32 i, j, k;

  t_u_int8 *mm = vb->model;
  for(i=0; i<image->height; i++)
  {
    t_u_int8 *ii = (t_u_int8*)(image->imageData + image->widthStep*i);		
    for (j=0; j<image->width; j++, ii+=4)
    {
      for (k=0; k<vb->nsamples; k++, mm+=3)
      {
        int m;
        m = ii[0] + ((int)vb->sigma_y * ((int)(vibe_getrandom(vb) % 256) - 127))/256;
        mm[0]=m; 
        if (m<0) 
          mm[0]=0; 
        else if (m>255) 
          mm[0]=255;
        m = ii[1] + ((int)vb->sigma_u * ((int)(vibe_getrandom(vb) % 256) - 127))/256;
        mm[1]=m; 
        if (m<0) 
          mm[1]=0; 
        else if (m>255) 
          mm[1]=255;
        m = ii[2] + ((int)vb->sigma_v * ((int)(vibe_getrandom(vb) % 256) - 127))/256;
        mm[2]=m; 
        if (m<0) 
          mm[2]=0; 
        else if (m>255) 
          mm[2]=255;
      }
    }
  }

  memset(vb->conf,255,vb->width*vb->height*sizeof(t_u_int8));
}

void vibe_segment(t_vibe *vb, IplImage *image)
{
  t_u_int32 i, j, k;
  t_u_int8 card;
  t_double totscore;

  t_u_int32 suv = vb->sigma_u * vb->sigma_v; suv *= suv;
  t_u_int32 svy = vb->sigma_v * vb->sigma_y; svy *= svy;
  t_u_int32 syu = vb->sigma_y * vb->sigma_u; syu *= syu;
  t_u_int32 syuv = vb->sigma_y * vb->sigma_u * vb->sigma_v; syuv *= syuv;

  t_u_int8 *mm = vb->model;
  t_u_int8 *cc = vb->conf;
  for(i=0, totscore = 0.0; i<image->height; i++)
  {
    t_u_int8 *ii = (t_u_int8*)image->imageData + image->widthStep*i;		
    for (j=0; j<image->width; j++, ii+=4, mm+=3*vb->nsamples, cc++)
    {
      //ii[3] = 255;
      for (k=0, card=0; k<vb->nsamples; k++)
      {
        t_s_int32 dy = mm[3*k]  -ii[0];
        t_s_int32 du = mm[3*k+1]-ii[1];
        t_s_int32 dv = mm[3*k+2]-ii[2];
        if (dy*dy*suv + du*du*svy + dv*dv*syu < syuv) card++;
        //if (card >= vb->cardinality) {
        //	ii[3] = 0;
        //	break;
        //}
      }
			
      //t_s_int32 score = (vb->nsamples-card)*40;
      t_s_int32 score = (cc[0] * (2*vb->cardinality - card))/(2*vb->cardinality) + 127 - cc[0]/2;
      ii[3] = (score>0)? score : 0;

      totscore += ii[3];
    }
  }

  totscore /= (image->width*image->height*255.0);
	
  if (totscore-vb->lastscore > 0.1) {
    printf("[Vibe] camera movement (%f/%f), resetting confidence \n", totscore, vb->lastscore);
    memset(vb->conf,0,vb->width*vb->height*sizeof(t_u_int8));
    for(i=0; i<image->height; i++)
    {
      t_u_int8 *ii = (t_u_int8*)image->imageData + image->widthStep*i;		
      for (j=0; j<image->width; j++, ii+=4)
      {
        ii[3] = 127;
        totscore += ii[3];
      }
    }
    totscore = 0.5;
  }
	

  vb->lastscore = totscore;
}

// jvw mcs 14/aug/12
// Update one model which is randomly chosen, using t, a decay factor
// calculated using lspeed_fg and l_speed_bg
void vibe_update(t_vibe *vb, IplImage *image)
{
  t_u_int32 i, j;

  t_u_int8 *mm = vb->model;
  t_u_int8 *cc = vb->conf;
  for(i=0; i<image->height; i++)
  {
    t_u_int8 *ii = (t_u_int8*)image->imageData + image->widthStep*i;		
    for (j=0; j<image->width; j++, ii+=4, mm+=3*vb->nsamples, cc++) {
      //t_s_int32 t = (ii[3]*vb->lspeed_fg)/255 + ((255-ii[3])*vb->lspeed_bg)/255;
      t_s_int32 t = (expt[ (ii[3]*cc[0])/255 ] * vb->lspeed_fg)/255.0 + vb->lspeed_bg;
      //t = (t*cc[0])/255;
      if ((t<2)||(vibe_getrandom(vb) % t ==0)) { 
        t_u_int32 r = vibe_getrandom(vb) % vb->nsamples;
        mm[3*r] = ii[0];
        mm[3*r+1] = ii[1];
        mm[3*r+2] = ii[2];
        cc[0] = (cc[0]<256-vb->cinc_bg)? cc[0]+vb->cinc_bg : 255;
      } else {
        cc[0] = (cc[0]>vb->cdec_fg-1)? cc[0]-vb->cdec_fg : 0;
      }
    }
  }
}

void vibe_display(t_vibe *vb, IplImage *image)
{
  t_u_int32 i, j;

  t_u_int8 *cc = vb->conf;
  for(i=0; i<image->height; i++)
  {
    t_u_int8 *ii = (t_u_int8*)image->imageData + image->widthStep*i;		
    for (j=0; j<image->width; j++, ii+=4, cc++) {
      ii[0] = ii[3];
      ii[1] = 127; //ii[3];
      ii[2] = 127; //ii[3];
    }
  }
}

void vibe_display_model(t_vibe *vb, IplImage *image)
{
  t_u_int32 i, j, k;

  t_u_int16 sum;

  t_u_int8 *mm = vb->model;
  t_u_int8 *cc = vb->conf;
  for(i=0; i<image->height; i++)
  {
    t_u_int8 *ii = (t_u_int8*)image->imageData + image->widthStep*i;		
    for (j=0; j<image->width; j++, ii+=4, cc++, mm+=3*vb->nsamples) {
      for (k=0,sum=0; k<vb->nsamples;k++) 
        sum+=mm[3*k]; 
      ii[0] = sum/vb->nsamples;
      for (k=0,sum=0; k<vb->nsamples;k++) 
        sum+=mm[3*k+1]; 
      ii[1] = sum/vb->nsamples;
      for (k=0,sum=0; k<vb->nsamples;k++) 
        sum+=mm[3*k+2]; 
      ii[2] = sum/vb->nsamples;
    }
  }
}
