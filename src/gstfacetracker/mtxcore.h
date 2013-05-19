#ifndef LIB_UTILS_MATH_MTXCORE_H
#define LIB_UTILS_MATH_MTXCORE_H

#include "defines.h"

#define MAT_TRANSPOSE_A	1
#define MAT_TRANSPOSE_B	2
#define MAT_TRANSPOSE_C	4


// a[n](i) = s
/*inline*/ float* mat_set(float *a, float s, unsigned int n);
#define vec_set mat_set

// a[n,n](i,i) = s; a[n,n](i,j) = 0
/*inline*/ float* mat_eye(float *a, float s, unsigned int n);

// a[n] = b[n]-c[n], may be in place (a=b or a=c)
/*inline*/ float* mat_sub(float *a, float *b, float *c, unsigned int n);
#define vec_sub mat_sub

// a[n] = b[n]+c[n], may be in place (a=b or a=c)
/*inline*/ float* mat_add(float *a, float *b, float *c, unsigned int n);
#define vec_add mat_add

// a[n] = b[n]+s*c[n], may be in place (a=b or a=c)
/*inline*/ float* mat_add_scale(float *a, float *b, float *c, float s, unsigned int n);
#define vec_add mat_add

// a[n] = (sb*b[n]+sc*c[n])/(sb+sc), may be in place (a=b or a=c)
/*inline*/ float* mat_lincomb(float *a, float *b, float *c, float sb, float sc, unsigned int n);
#define vec_lincomb mat_lincomb

// a[m,n] = b[m,n]-repmat(c[n],m,1), may be in place (a=b)
/*inline*/ float* mat_diffvec(float *a, float *b, float *c, unsigned int m, unsigned int n);

// a[n] = mean(b[m,n]) over m row-samples
/*inline*/ float* mat_meanvec(float *a, float *b, unsigned int m, unsigned int n);

// a[m,p] = b[m,n] * c[n,p]
/*inline*/ float* mat_multiply(float *a, float *b, float *c, unsigned int m, unsigned int n, unsigned int p);
#define vec_multiply(a,b,c,m,n) mat_multiply((a),(b),(c),(m),(n),1)

// flags==MAT_TRANSPOSE_A: (a[p,m])' = b[m,n] * c[n,p]
// flags==MAT_TRANSPOSE_B: a[p,m] = (b[n,m])' * c[n,p]
// flags==MAT_TRANSPOSE_C: a[p,m] = b[m,n] * (c[p,n])'
/*inline*/ float* mat_multiply_transpose(float *a, float *b, float *c, unsigned int m, unsigned int n, unsigned int p, unsigned char flags);
#define vec_multiply_transpose(a,b,c,m,n) mat_multiply_transpose((a),(b),(c),(m),(n),1,MAT_TRANSPOSE_B)

// a[n] = b[n]*s, may be in place (a=b)
/*inline*/ float* mat_scale(float *a, float *b, float s, unsigned int n);
#define vec_scale mat_scale

// a[n,n] = b[n,n]', may be in place (a=b)
/*inline*/ float* mat_transpose(float *a, float *b, unsigned int n);

// a[n,n] = inv(b[n,n])
/*inline*/ float* mat_invert(float *a, float *b, unsigned int n);

// x[n] <= a[m,n]*x[n] = 0 (m should be >=n-1)
/*inline*/ float* mat_solve_homogeneous(float *x, float *a, unsigned int m, unsigned int n);
/*inline*/ float mat_solve_homogeneous_fast(float *x, float *a, unsigned int m, unsigned int n); //returns total/residue
/*inline*/ float mat_solve_homogeneous_fasfloat( float *x, float *a, unsigned int m, unsigned int n);
/*inline*/ float* mat_solve_squared_double(float *x, float *a, float *b, unsigned int n);

// a[n] = b[n]/norm(b[n]) may be in-place (a=b)
/*inline*/ float* mat_normalize(float *a, float *b, unsigned int n);
#define vec_normalize mat_normalize

// if (b[n](n-1)!=0) a[n] = b[n]/b[n](n-1) may be in-place (a=b)
/*inline*/ float* mat_dehomogenize(float *a, float *b, unsigned int n);
#define vec_dehomogenize mat_dehomogenize

// same as dehomogenize, but reduces dimension of a to n-1 (a(n-1) is not set to 1)
/*inline*/ float* mat_dehomogenize_reduce(float *a, float *b, unsigned int n);
#define vec_dehomogenize_reduce mat_dehomogenize_reduce

// a[n](i) = b[n-1](i) and a[n](n-1)=1
/*inline*/ float* mat_homogenize_augment(float *a, float *b, unsigned int n);
#define vec_homogenize_augment mat_homogenize_augment

// a[3] = cross_product(b[3],c[3])
/*inline*/ float* mat_cross(float a[3], float b[3], float c[3]);
#define vec_cross mat_cross

// return dot_product(a[n],b[n])
/*inline*/ float mat_dot(float *a, float *b, unsigned int n);
#define vec_dot mat_dot

// return sqrt(sum((a[n]-b[n])^2))
/*inline*/ float mat_distance(float *a, float *b, unsigned int n);
#define vec_distance mat_distance

// return sqrt(sum(a[n]^2))
/*inline*/ float mat_norm(float *a, unsigned int n);
#define vec_norm mat_norm

// return det(a[3,3])
/*inline*/ float mat_det3x3(float a[9]);

// r[3,3] = chol(a[3,3])
/*inline*/ float* mat_chol3x3(float r[9], float a[9]);

// svd of a[m,n]
/*inline*/ float* mat_svd(float *a, float *u, float *s, float *vt, unsigned int m, unsigned int n);

// rodrigues rotation matrix to rotation vector
/*inline*/ float* mat_rodrigues_R2v(float *v, float *R);

// rodrigues rotation matrix to rotation vector
/*inline*/ float* mat_rodrigues_v2R(float *R, float *v);

// swap a[n] and b[n]
/*inline*/ float* mat_swap(float *a, float *b, unsigned int n);
#define vec_swap mat_swap

// print elements of a[m,n]
/*inline*/ float* mat_print(char *name, float *a, unsigned int m, unsigned int n) ;
#define vec_print(name,a,n) mat_print((name),(a),1,(n))

// X <- B/A or solve A*X = B and put X->B
/*inline*/ float*  mat_gauss_elim(float *A, float *B, unsigned int n);


// Cholesky functions:
//   R[p,p] is cholesky decomp of A (A=R'R and R = upper-triangular)
//   xi[p,n](columns) or xi[n,p](rows) is matrix with n vectors of p elements

// R'R <- R'R + xixi' for all xi (columns)
/*inline*/ float*  mat_chol_update_plus(float *R, float *x, unsigned int p, unsigned int n);

// R'R <- R'R + xi'xi for all xi (rows)
/*inline*/ float*  mat_chol_update_plus_trans(float *R, float *x, unsigned int p, unsigned int n);

// R'R <- R'R - xixi' for all xi (columns)
/*inline*/ float*  mat_chol_update_min(float *R, float *x, unsigned int p, unsigned int n);

// B[p,n] <- inv(R'R)*B
/*inline*/ float*  mat_chol_solve_rhs(float *R, float *B, unsigned int p, unsigned int n);

// B[n,p] <- B*inv(R'R)
/*inline*/ float*  mat_chol_solve_lhs(float *R, float *B, unsigned int p, unsigned int n);

// out[p,n] <- R'*A[p,n]
/*inline*/ float*  mat_chol_mult_trans(float *R, float *A, float *out, unsigned int p, unsigned int n);

// B <- B/R (R upper triangular) or solve R*X = B and put X->B
// TODO: control divide by zero problem
/*inline*/ float*  mat_chol_gauss_elim(float *R, float *B, unsigned int p, unsigned int n);


// Macros to be compatible with Donny
#define vectorAdd(vec1,vec2,result,dim) 		vec_add((result),(vec1),(vec2),(dim))
#define vectorSubtract(vec1,vec2,result,dim) 		vec_sub((result),(vec1),(vec2),(dim))
#define vectorMultiplyByCst(vec1, cst, result, dim) 	vec_scale((result),(vec1),(cst),(dim))
#define vectorDivideByCst(vec1, cst, result, dim) 	vec_scale((result),(vec1),1.0/(cst),(dim))
#define vectorNorm(vec,dim) 				vec_norm((vec),(dim))
#define vectorNormalize(vec,result,dim) 		vec_normalize((result),(vec),(dim))
#define vectorCrossProduct(vec1,vec2,result,dim) 	vec_cross((result),(vec1),(vec2))
#define vectorDotProduct(vec1,vec2,dim) 		vec_dot((vec1),(vec2),(dim))
#define vector3DTo4D(vec3D,vec4D) 			vec_homogenize_augment((vec4D),(vec3D),4)
#define vector4DTo3D(vec4D,vec3D) 			vec_dehomogenize_reduce((vec3D),(vec4D),4)
#define matrixTranspose(matrix,transpose,dim) 		mat_transpose((transpose),(matrix),(dim))

#endif


