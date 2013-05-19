#include "mtxcore.h"
#include <math.h>
#include <opencv/cv.h>

// a[n](i) = s
/*inline*/ float* mat_set(float *a, float s, unsigned int n)
{
	unsigned int i;
	float *aa=a;

	for (i=0;i<n;i++,aa++) (*aa) = s;

	return a;
}

// a[n,n](i,i) = s; a[n,n](i,j) = 0
/*inline*/ float* mat_eye(float *a, float s, unsigned int n)
{
	unsigned int i;
	float *aa=a;
	memset(a,0,n*n*sizeof(float));

	for (i=0;i<n;i++,aa+=(n+1)) (*aa) = s;

	return a;
}

// a[n] = b[n]-c[n], may be in place (a=b or a=c)
/*inline*/ float* mat_sub(float *a, float *b, float *c, unsigned int n)
{
	unsigned int i;
	float *aa=a;

	for (i=0;i<n;i++,aa++,b++,c++) (*aa) = (*b)-(*c);

	return a;
}

// a[n] = b[n]+c[n], may be in place (a=b or a=c)
/*inline*/ float* mat_add(float *a, float *b, float *c, unsigned int n)
{
	unsigned int i;
	float *aa=a;

	for (i=0;i<n;i++,aa++,b++,c++) (*aa) = (*b)+(*c);

	return a;
}

// a[n] = b[n]+s*c[n], may be in place (a=b or a=c)
/*inline*/ float* mat_add_scale(float *a, float *b, float *c, float s, unsigned int n)
{
	unsigned int i;
	float *aa=a;

	for (i=0;i<n;i++,aa++,b++,c++) (*aa) = (*b)+s*(*c);

	return a;
}

// a[n] = (sb*b[n]+sc*c[n])/(sb+sc), may be in place (a=b or a=c)
/*inline*/ float* mat_lincomb(float *a, float *b, float *c, float sb, float sc, unsigned int n)
{
	unsigned int i;
	float *aa=a;
	float s = sb+sc;
	sb/=s;
	sc/=s;

	for (i=0;i<n;i++,aa++,b++,c++) (*aa) = sb*(*b)+sc*(*c);

	return a;
}

// a[m,n] = b[m,n]-repmat(c[n],m,1), may be in place (a=b)
/*inline*/ float* mat_diffvec(float *a, float *b, float *c, unsigned int m, unsigned int n)
{
	float *aa, *bb, *cc;
	unsigned int i,j;

	for (i=0,aa=a,bb=b;i<m;i++)
		for (j=0,cc=c;j<n;j++,aa++,bb++,cc++)
			*aa = *bb - *cc;

	return a;
}

// a[n] = mean(b[m,n]) over m row-samples
/*inline*/ float* mat_meanvec(float *a, float *b, unsigned int m, unsigned int n)
{
	float *aa, *bb;
	unsigned int i,j;

	memset(a,0,n*sizeof(float));
	for (i=0,bb=b;i<m;i++)
		for (j=0,aa=a;j<n;j++,aa++,bb++)
			*aa += *bb;
	for (j=0,aa=a;j<n;j++,aa++)
		*aa /= m;

	return a;
}



// a[m,p] = b[m,n] * c[n,p]
/*inline*/ float* mat_multiply(float *a, float *b, float *c, unsigned int m, unsigned int n, unsigned int p)
{
	unsigned int i,j,k;
	float *aa, *bb, *cc, *ccc;

	for (i=0,aa=a;i<m;i++,b+=n)
		for (j=0, cc=c;j<p;j++,cc++,aa++) {
			(*aa) = 0;
			for (k=0,bb=b,ccc=cc;k<n;k++,ccc+=p,bb++) (*aa) += (*bb) * (*ccc);
		}

	return a;
}

// flags==MAT_TRANSPOSE_A: (a[p,m])' = b[m,n] * c[n,p]
// flags==MAT_TRANSPOSE_B: a[p,m] = (b[n,m])' * c[n,p]
// flags==MAT_TRANSPOSE_C: a[p,m] = b[m,n] * (c[p,n])'
/*inline*/ float* mat_multiply_transpose(float *a, float *b, float *c, unsigned int m, unsigned int n, unsigned int p, unsigned char flags)
{
	if (flags==0) return mat_multiply(a,b,c,m,n,p);

	unsigned int i,j,k;
	float *aa, *bb, *cc, *aaa, *bbb, *ccc;
	unsigned int sa=1;
	unsigned int sb=1;
	unsigned int sc=1;
	unsigned int la=p;
	unsigned int lb=n;
	unsigned int lc=p;

	if (flags & MAT_TRANSPOSE_A) { sa=m; la=1; }
	if (flags & MAT_TRANSPOSE_B) { sb=m; lb=1; }
	if (flags & MAT_TRANSPOSE_C) { sc=n; lc=1; }
	for (i=0,aa=a,bb=b;i<m;i++,aa+=la,bb+=lb)
		for (j=0,aaa=aa,cc=c;j<p;j++,aaa+=sa,cc+=sc) {
			(*aaa) = 0;
			for (k=0,bbb=bb,ccc=cc;k<n;k++,bbb+=sb,ccc+=lc) (*aaa) += (*bbb) * (*ccc);
		}

	return a;
}

// a[n] = b[n]*s, may be in place (a=b)
/*inline*/ float* mat_scale(float *a, float *b, float s, unsigned int n)
{
	unsigned int i;
	float *aa=a;

	for (i=0;i<n;i++,aa++,b++) (*aa) = (*b)*s;

	return a;
}

// a[n,n] = b[n,n]', may be in place (a=b)
/*inline*/ float* mat_transpose(float *a, float *b, unsigned int n)
{
	unsigned int i, j;
	float tmp;
	float *aa,*bb;

	if (a!=b) memcpy(a,b,n*n*sizeof(float));
	aa = a;
	b = a;
	for (i=0;i<n;i++,b++) {
		for (j=0,bb=b;j<i;j++,bb+=n,aa++) {}
		for (;j<n;j++,bb+=n,aa++) { tmp = (*aa); (*aa)=(*bb); (*bb)=tmp; }
	}

	return a;
}

// a[n,n] = inv(b[n,n])
// TODO: eliminate opencv
/*inline*/ float* mat_invert(float *a, float *b, unsigned int n)
{
	CvMat ma; cvInitMatHeader( &ma, n, n, CV_32FC1, a, CV_AUTOSTEP );
	CvMat mb; cvInitMatHeader( &mb, n, n, CV_32FC1, b, CV_AUTOSTEP );

	cvInvert(&mb,&ma,CV_LU);

	return a;
}

// a[m,n]*x[n] = 0
// TODO: eliminate opencv
/*inline*/ float* mat_solve_homogeneous( float *x, float *a, unsigned int m, unsigned int n)
{
	CvMat mA; cvInitMatHeader( &mA, m, n, CV_32FC1, a, CV_AUTOSTEP );
	CvMat *mW = cvCreateMat(1,n,CV_32FC1);
	CvMat *mV = cvCreateMat(n,n,CV_32FC1);


	//alternative:
		CvMat *mAtA = cvCreateMat(n,n,CV_32FC1);
		cvMulTransposed(&mA,mAtA,1,NULL,1.0);
		cvEigenVV(mAtA,mV,mW,0,0,0);
		cvReleaseMat(&mAtA);

	//cvSVD(&mA,mW,NULL,mV,CV_SVD_MODIFY_A+CV_SVD_V_T);

	memcpy(x,mV->data.fl+n*(n-1),n*sizeof(float));

	cvReleaseMat(&mW); cvReleaseMat(&mV);

	return x;

}

// a[m,n]*x[n] = 0
// TODO: eliminate opencv
/*inline*/ float mat_solve_homogeneous_fast( float *x, float *a, unsigned int m, unsigned int n)
{
	// Extract fundamental matrix from the column of V corresponding to
	// smallest singular value.

	float *u = (float*)malloc(m*m*sizeof(float));
	float *s = (float*)malloc(m*n*sizeof(float));
	float *vt = (float*)malloc(n*n*sizeof(float));
	float r;
	unsigned int i;

	CvMat mA; cvInitMatHeader( &mA, m, n, CV_32FC1, a, CV_AUTOSTEP );
	CvMat mU; cvInitMatHeader( &mU, m, m, CV_32FC1, u, CV_AUTOSTEP );
	CvMat mS; cvInitMatHeader( &mS, m, n, CV_32FC1, s, CV_AUTOSTEP );
	CvMat mVt; cvInitMatHeader( &mVt, n, n, CV_32FC1, vt, CV_AUTOSTEP );

	cvSVD(&mA,&mS,&mU,&mVt,CV_SVD_V_T+CV_SVD_U_T);
	//mat_svd(a, u, s, vt, m, n);

	memcpy(x, vt + n*(n-1), n*sizeof(float));

	if (m<n)
		r = 1.0;
	else {
		for (i=0,r=1;i<n-1;i++)
			r *= s[i*(n+1)];
		r /= s[n*n-1];
	}

	free(u);
	free(s);
	free(vt);

	return r;
}

// a[m,n]*x[n] = 0
// TODO: eliminate opencv
/*inline*/ float mat_solve_homogeneous_fasfloat( float *x, float *a, unsigned int m, unsigned int n)
{
	// Extract fundamental matrix from the column of V corresponding to
	// smallest singular value.

	float *ad = (float*)malloc(m*n*sizeof(float));
	float *u = (float*)malloc(m*m*sizeof(float));
	float *s = (float*)malloc(m*n*sizeof(float));
	float *vt = (float*)malloc(n*n*sizeof(float));
	float r;
	unsigned int i;

	for (i=0; i<m*n; i++) ad[i] = a[i];

	CvMat mA; cvInitMatHeader( &mA, m, n, CV_64FC1, ad, CV_AUTOSTEP );
	CvMat mU; cvInitMatHeader( &mU, m, m, CV_64FC1, u, CV_AUTOSTEP );
	CvMat mS; cvInitMatHeader( &mS, m, n, CV_64FC1, s, CV_AUTOSTEP );
	CvMat mVt; cvInitMatHeader( &mVt, n, n, CV_64FC1, vt, CV_AUTOSTEP );

	cvSVD(&mA,&mS,&mU,&mVt,CV_SVD_V_T+CV_SVD_U_T);
	//mat_svd(a, u, s, vt, m, n);

	for (i=0; i<n; i++) x[i] = vt[n*(n-1)+i];

	if (m<n)
		r = 1.0;
	else {
		for (i=0,r=1;i<n-1;i++)
			r *= s[i*(n+1)];
		r /= s[n*n-1];
	}

	free(u);
	free(s);
	free(vt);

	return r;
}

// a[m,n]*x[n] = 0
// TODO: eliminate opencv
/*inline*/ float* mat_solve_squared_double( float *x, float *a, float *b, unsigned int n)
{
	unsigned int i;

	CvMat *A = cvCreateMat(n, n, CV_64FC1); for (i=0; i<n*n; i++) A->data.db[i] = a[i];
	CvMat *X = cvCreateMat(n, 1, CV_64FC1);
	CvMat *B = cvCreateMat(n, 1, CV_64FC1); for (i=0; i<n; i++) B->data.db[i] = b[i];
	cvSolve(A, B, X, CV_LU);    // solve (AX=B) for X

	for (i=0; i<n; i++) x[i] = X->data.db[i];

	cvReleaseMat(&A);
	cvReleaseMat(&X);
	cvReleaseMat(&B);

	return x;

}

// a[n] = b[n]/norm(b[n]) may be in-place (a=b)
/*inline*/ float* mat_normalize(float *a, float *b, unsigned int n)
{
	float s;
	unsigned int i;
	float *aa=a;

	s = mat_norm(b,n);

	for (i=0;i<n;i++,aa++,b++) (*aa) = (*b)/s;

	return a;
}

// a[n] = b[n]/b[N] may be in-place (a=b)
/*inline*/ float* mat_dehomogenize(float *a, float *b, unsigned int n)
{
	unsigned int i;
	float s = b[n-1];
	float *aa=a;

	for (i=0;i<n-1;i++,aa++,b++) (*aa) = (*b)/s;
	(*aa) = 1.0;

	return a;
}

// same as dehomogenize, but reduces dimension of a to n-1 (a(n-1) is not set to 1)
/*inline*/ float* mat_dehomogenize_reduce(float *a, float *b, unsigned int n)
{
	unsigned int i;
	float s = b[n-1];
	float *aa=a;

	for (i=0;i<n-1;i++,aa++,b++) (*aa) = (*b)/s;

	return a;
}

// a[n](i) = b[n-1](i) and a[n](n-1)=1
/*inline*/ float* mat_homogenize_augment(float *a, float *b, unsigned int n)
{
	memcpy(a,b,(n-1)*sizeof(float));
	a[n-1] = 1.0;
	return a;
}


// a[3] = cross_product(b[3],c[3])
/*inline*/ float* mat_cross(float a[3], float b[3], float c[3])
{
	a[0] = b[1]*c[2] - b[2]*c[1];
	a[1] = b[2]*c[0] - b[0]*c[2];
	a[2] = b[0]*c[1] - b[1]*c[0];
	return a;
}

// return dot_product(a[n],b[n])
/*inline*/ float mat_dot(float *a, float *b, unsigned int n)
{
	float s;
	unsigned int i;

	for (i=0,s=0;i<n;i++,a++,b++) s+= (*a)*(*b);

	return s;
}

// return sqrt(sum((a[n]-b[n])^2))
/*inline*/ float mat_distance(float *a, float *b, unsigned int n)
{
	float s;
	unsigned int i;

	for (i=0,s=0;i<n;i++,a++,b++) s+= ((*a)-(*b))*((*a)-(*b));

	return sqrt(s);
}

// return sqrt(sum(a[n]^2))
/*inline*/ float mat_norm(float *a, unsigned int n)
{
	float s;
	unsigned int i;

	for (i=0,s=0;i<n;i++,a++) s+= (*a)*(*a);

	return sqrt(s);
}

// return det(a[3,3])
/*inline*/ float mat_det3x3(float a[9])
{
	float det;
	det = a[0]*a[4]*a[8];
	det += a[1]*a[5]*a[6];
	det += a[2]*a[3]*a[7];
	det -= a[6]*a[4]*a[2];
	det -= a[7]*a[5]*a[0];
	det -= a[8]*a[3]*a[1];
	return det;
}

// r[3,3] = chol(a[3,3])
/*inline*/ float* mat_chol3x3(float r[9], float a[9])
{
	// A = R'R and R is upper-triangle
	// A should be symm, pos definite

	r[0] = sqrt(a[0]);
	r[1] = a[1]/r[0];
	r[2] = a[2]/r[0];
	r[3] = 0;
	r[4] = sqrt(a[4]-r[1]*r[1]);
	r[5] = (a[5]-r[1]*r[2])/r[4];
	r[6] = 0;
	r[7] = 0;
	r[8] = sqrt(a[8]-r[2]*r[2]-r[5]*r[5]);

	return r;
}

// svd of a[m,n]
// TODO: eliminate opencv
/*inline*/ float* mat_svd(float *a, float *u, float *s, float *vt, unsigned int m, unsigned int n)
{
	CvMat mA; cvInitMatHeader( &mA, m, n, CV_32FC1, a, CV_AUTOSTEP );
	CvMat mU; cvInitMatHeader( &mU, m, m, CV_32FC1, u, CV_AUTOSTEP );
	CvMat mS; cvInitMatHeader( &mS, m, n, CV_32FC1, s, CV_AUTOSTEP );
	CvMat mVt; cvInitMatHeader( &mVt, n, n, CV_32FC1, vt, CV_AUTOSTEP );

	cvSVD(&mA,&mS,&mU,&mVt,CV_SVD_V_T);

	return s;

}

// rodrigues rotation matrix to rotation vector
// TODO: eliminate opencv
/*inline*/ float* mat_rodrigues_R2v(float *v, float *R)
{
	CvMat mR, mv;

	cvRodrigues2(cvInitMatHeader(&mR,3,3,CV_32FC1,R,CV_AUTOSTEP), cvInitMatHeader(&mv,1,3,CV_32FC1,v,CV_AUTOSTEP),NULL);

	return v;
}

// rodrigues rotation matrix to rotation vector
// TODO: eliminate opencv
/*inline*/ float* mat_rodrigues_v2R(float *R, float *v)
{
	CvMat mR, mv;

	cvRodrigues2(cvInitMatHeader(&mv,1,3,CV_32FC1,v,CV_AUTOSTEP), cvInitMatHeader(&mR,3,3,CV_32FC1,R,CV_AUTOSTEP),NULL);

	return R;
}

// swap a[n] and b[n]
/*inline*/ float* mat_swap(float *a, float *b, unsigned int n)
{
	float tmp;
	unsigned int i;
	float *aa=a;

	for (i=0;i<n;i++,aa++,b++) { tmp=(*aa); (*aa)=(*b); (*b)=tmp; }

	return a;
}


// print elements of a[m,n]
/*inline*/ float* mat_print(char *name, float *a, unsigned int m, unsigned int n)
{
	unsigned int i,j;
	float *aa = a;
	printf("%s = [",name);
	for (i=0;i<m;i++) {
		for (j=0;j<n;j++,aa++)
			printf("\t%e,",*aa);
		if (i<m-1) printf(";\n");
	}
	printf("];\n");

	return a;

}

// X <- B/A or solve A*X = B and put X->B
// TODO: control divide by zero problem
/*inline*/ float*  mat_gauss_elim(float *A, float *B, unsigned int n)
{
	unsigned int i,j,k;
	unsigned int piv[n], ipiv;

	memset(piv,0,n*sizeof(unsigned int));
	for (i=0; i<n; i++) {
		// get non-zero pivot
		for (ipiv=0;ipiv<n;ipiv++) if (piv[ipiv]==0 && A[i*n+ipiv]!=0) break;
		piv[ipiv] = 1;

		// normalize pivot row
		for (j=i+1; j<n; j++) A[ipiv*n+j] /= A[ipiv*n+i];
		B[ipiv] /= A[ipiv*n+i];
		A[ipiv*n+i] = 1.0;

		// subtract pivot row
		for (j=0; j<n; j++)
			if (j!=ipiv)
			{
				for (k=i+1;k<n;k++) A[j*n+k]-=A[j*n+i]*A[ipiv*n+k];
				B[j]-=A[j*n+i]*B[ipiv];
				A[j*n+i]=0.0;
			}
	}

	return B;
}

// Cholesky functions:
//   R[p,p] is cholesky decomp of A=R'R;
//   xi[p,n](columns) or xi[n,p](rows) is matrix with n vectors of p elements

// R'R <- R'R + xixi' for all xi (columns)
/*inline*/ float*  mat_chol_update_plus(float *R, float *x, unsigned int p, unsigned int n)
{

	float c[p];
	float s[p];
	float t;
	float xj;
	float norm;
	unsigned int jj, ij, k, j, i;

	for (k=0; k<n; k++){
		for (j=0; j<p; j++){
			xj = x[k+j*n];
			if (j>0){
				for(i=0; i<j; i++){
					ij = j+i*p;
					t = c[i]*R[ij] + s[i]*xj;
					xj = c[i]*xj - s[i]*R[ij];
					R[ij] = t;
				}
			}
			jj = j+j*p;
			norm = sqrt(R[jj]*R[jj]+xj*xj);
			if (norm>0){
				c[j] = R[jj]/norm;
				s[j] = xj/norm;
			} else {
				c[j] = 0;
				s[j] = 1;
			}
			R[jj] = norm;
		}
	}

	for (i=0; i<p; i++) {
		float *rr = R + i*(1+p);
		if ((*rr)<0)
			for (j=i;j<p;j++,rr++) (*rr) = -(*rr);
	}

	return R;

}

// R'R <- R'R + xi'xi for all xi (rows)
/*inline*/ float*  mat_chol_update_plus_trans(float *R, float *x, unsigned int p, unsigned int n)
{

	float c[p];
	float s[p];
	float t;
	float xj;
	float norm;
	unsigned int jj, ij, k, j, i;

	for (k=0; k<n; k++){
		for (j=0; j<p; j++){
			xj = x[k*p+j];
			if (j>0){
				for(i=0; i<j; i++){
					ij = j+i*p;
					t = c[i]*R[ij] + s[i]*xj;
					xj = c[i]*xj - s[i]*R[ij];
					R[ij] = t;
				}
			}
			jj = j+j*p;
			norm = sqrt(R[jj]*R[jj]+xj*xj);
			if (norm>0){
				c[j] = R[jj]/norm;
				s[j] = xj/norm;
			} else {
				c[j] = 0;
				s[j] = 1;
			}
			R[jj] = norm;
		}
	}

	for (i=0; i<p; i++) {
		float *rr = R + i*(1+p);
		if ((*rr)<0)
			for (j=i;j<p;j++,rr++) (*rr) = -(*rr);
	}

	return R;

}

// R'R <- R'R - xixi' for all xi (columns)
/*inline*/ float*  mat_chol_update_min(float *R, float *x, unsigned int p, unsigned int n)
{

	float c[p];
	float s[p];
	float t;
	float xx;
	float norm;
	float alpha;
	float ztemp;
	float scale;
	float a, b;
	unsigned int ij, k, j;
	int i;

	for (k=0; k<n; k++){

		if (R[0] != 0) s[0] = x[k]/R[0];
			else s[0]=0;
		norm = s[0]*s[0];

		for (j=1; j<p; j++){
			ztemp = 0;
			for (i=0;i<(int)j;i++){
				ztemp += R[i*p+j]*s[i];
			}
			if (R[j+p*j] != 0) s[j] = (x[k+n*j] - ztemp)/R[j+p*j];
				else s[j] = 0;
			norm += s[j]*s[j];
		}
		alpha = sqrt(1.0-norm);
		norm = sqrt(norm);

		for (i=p-1;i>=0;i--){
			scale = alpha + fabs(s[i]);
			a = alpha/scale;
			b = s[i]/scale;
			norm = sqrt(a*a+b*b);
			c[i] = a/norm;
			s[i] = b/norm;
			alpha = scale*norm;
		}

		for (j=0; j<p; j++){
			xx = 0;
			for(i=j; i>=0; i--){
				ij = j+i*p;
				t = c[i]*xx + s[i]*R[ij];
				R[ij] = c[i]*R[ij] - s[i]*xx;
				xx = t;
			}
		}
	}

	return R;
}

// B[p,n] <- inv(R'R)*B
/*inline*/ float*  mat_chol_solve_rhs(float *R, float *B, unsigned int p, unsigned int n)
{

	float t;
	float ztemp;
	unsigned int i,k;
	int j;

	for (k=0; k<n; k++){

		if (R[0] != 0) B[k] = B[k]/R[0];
			else B[k]=0;

		for (j=1; j<(int)p; j++){
			ztemp = 0;
			for (i=0;i<(unsigned int)j;i++) 
                          ztemp += R[i*p+j]*B[k+i*n];
			if (R[j+p*j] != 0) B[k+n*j] = (B[k+n*j] - ztemp)/R[j+p*j];
				else B[k+n*j] = 0;
		}

		for (j=p-1; j>=0; j--){
			B[k+j*n] /= R[j+p*j];
			t = B[k+j*n];
			for (i = 0; i<(unsigned int)j; i++) B[k+i*n] -= t*R[j+i*p];
		}

	}

	return B;
}

// B[n,p] <- B*inv(R'R)
/*inline*/ float*  mat_chol_solve_lhs(float *R, float *B, unsigned int p, unsigned int n)
{
	float t;
	float ztemp;
	unsigned int i,k;
	int j;

	for (k=0; k<n; k++){

		if (R[0] != 0) B[k*p] = B[k*p]/R[0];
			else B[k*p]=0;

		for (j=1; j<(int)p; j++){
			ztemp = 0;
			for (i=0;i<(unsigned int)j;i++) ztemp += R[i*p+j]*B[k*p+i];
			if (R[j+p*j] != 0) B[k*p+j] = (B[k*p+j] - ztemp)/R[j+p*j];
				else B[k*p+j] = 0;
		}

		for (j=p-1; j>=0; j--){
			B[k*p+j] /= R[j+p*j];
			t = B[k*p+j];
			for (i = 0; i<(unsigned int)j; i++) B[k*p+i] -= t*R[j+i*p];
		}
	}

	return B;
}

// out[p,n] <- R'*A[p,n]
/*inline*/ float*  mat_chol_mult_trans(float *R, float *A, float *out, unsigned int p, unsigned int n)
{
	float accum, *oo;
	unsigned int r, c, kR, kA, k;


	for(r=0,oo=out; r<p; r++){
		for(c=0; c<n; c++,oo++){
			accum = 0;
			for(kR=r, kA=c, k=0; k<=r; k++, kR+=p, kA+=n)
					accum += R[kR]*A[kA];
			*oo = accum;
		}
	}

	return out;

}

// B <- B/R (R upper triangular)
/*inline*/ float*  mat_chol_gauss_elim(float *R, float *B, unsigned int p, unsigned int n)
{

	// solve R*X = B and put X->B
	// or B <- B/R
	// with R uppertriangular p*p
	// with B rectangular n*p

	float ztemp;
	unsigned int i,j,k;

	for (k=0; k<n; k++){

		if (R[0] != 0) B[k*p] = B[k*p]/R[0];
			else B[k*p]=0;

		for (j=1; j<p; j++){
			ztemp = 0;
			for (i=0;i<j;i++) ztemp += R[i*p+j]*B[k*p+i];
			if (R[j+p*j] != 0) B[k*p+j] = (B[k*p+j] - ztemp)/R[j+p*j];
				else B[k*p+j] = 0;
		}

	}
	return B;
}
