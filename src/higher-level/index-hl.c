/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

This file is part of FORCE - Framework for Operational Radiometric 
Correction for Environmental monitoring.

Copyright (C) 2013-2020 David Frantz

FORCE is free software_: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

FORCE is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with FORCE.  If not, see <http_://www.gnu.org/licenses/>.

+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/

/**+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This file contains functions for computing spectral indices
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


#include "index-hl.h"

/** GNU Scientific Library (GSL) **/
#include "gsl/gsl_blas.h"
#include "gsl/gsl_linalg.h"


enum { TCB, TCG, TCW, TCD};

void index_band(ard_t *ard, small *mask_, tsa_t *ts, int b, int nc, int nt, short nodata);
void index_differenced(ard_t *ard, small *mask_, tsa_t *ts, int b1, int b2, int nc, int nt, short nodata);
void index_kernelized(ard_t *ard, small *mask_, tsa_t *ts, int b1, int b2, int nc, int nt, short nodata);
void index_resistance(ard_t *ard, small *mask_, tsa_t *ts, int n, int r, int b, float f1, float f2, float f3, float f4, bool rbc, int nc, int nt, short nodata);
void index_tasseled(ard_t *ard, small *mask_, tsa_t *ts, int type, int b1, int b2, int b3, int b4, int b5, int b6, int nc, int nt, short nodata);
void index_unmixed(ard_t *ard, small *mask_, tsa_t *ts, int nc, int nt, short nodata, par_sma_t *sma, aux_emb_t *endmember);


/** This function computes a spectral index time series, with band method,
+++ i.e. it essentially copies a band from ARD to the TSA image array
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- b:      band
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_band(ard_t *ard, small *mask_, tsa_t *ts, int b, int nc, int nt, short nodata){
int p, t;


  #pragma omp parallel private(t) shared(ard,mask_,ts,b,nc,nt,nodata) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {
          ts->tss_[t][p] = ard[t].dat[b][p];
        }

      }

    }
  }
  
  return;
}


/** This function computes a spectral index time series, with Normalized
+++ differenced method, e.g. NDVI: (b1-b2)/(b1+b2)
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- b1:     band 1
--- b2:     band 2
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_differenced(ard_t *ard, small *mask_, tsa_t *ts, int b1, int b2, int nc, int nt, short nodata){
int p, t;
float tmp, ind, scale = 10000.0;


  #pragma omp parallel private(t,tmp,ind) shared(ard,mask_,ts,b1,b2,nc,nt,nodata,scale) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {
          tmp = (ard[t].dat[b1][p]+ard[t].dat[b2][p]);
          ind = (ard[t].dat[b1][p]-ard[t].dat[b2][p])/tmp;
          if (tmp == 0 || ind < -1 || ind > 1){
            ts->tss_[t][p] = nodata;
          } else {
            ts->tss_[t][p] = (short)(ind*scale);
          }
        }

      }

    }
  }

  return;
}


/** This function computes a spectral index time series, with Continuum
+++ Removal method
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- b_:     band for continuum removal
--- b1:     left band
--- b2:     right band
--- w_:     wavelength for continuum removal
--- w1:     left wavelength
--- w2:     right wavelength
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_cont_remove(ard_t *ard, small *mask_, tsa_t *ts, int b_, int b1, int b2, float w_, float w1, float w2, int nc, int nt, short nodata){
int p, t;
float tmp;


  #pragma omp parallel private(t,tmp) shared(ard,mask_,ts,b_,b1,b2,w_,w1,w2,nc,nt,nodata) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {


          tmp = (ard[t].dat[b1][p] * (w2 - w_) + 
                 ard[t].dat[b2][p] * (w_ - w1)) / 
                (w2 - w1);

          ts->tss_[t][p] = (short)(ard[t].dat[b_][p] - tmp);

        }

      }

    }
  }

  return;
}


/** This function computes a spectral index time series, with a Ratio - 1,
+++ e.g. CIre: (b1/b2)-1
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- b1:     band 1
--- b2:     band 2
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_ratio_minus1(ard_t *ard, small *mask_, tsa_t *ts, int b1, int b2, int nc, int nt, short nodata){
int p, t;
float ind, scale = 1000.0;


  #pragma omp parallel private(t,ind) shared(ard,mask_,ts,b1,b2,nc,nt,nodata,scale) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }

      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {
          ind = (ard[t].dat[b1][p] / (float)ard[t].dat[b2][p]) - 1.0;
          if (ard[t].dat[b2][p] == 0 || ind*scale > SHRT_MAX || ind*scale < SHRT_MIN){
            ts->tss_[t][p] = nodata;
          } else {
            ts->tss_[t][p] = (short)(ind*scale);
          }
        }

      }

    }
  }

  return;
}


/** This function computes an MSRre-like time series,
+++ MSRre: ((b1/b2)-1)/sqrt((b1/b2)+1)
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- b1:     band 1
--- b2:     band 2
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_msrre(ard_t *ard, small *mask_, tsa_t *ts, int b1, int b2, int nc, int nt, short nodata){
int p, t;
float upper, lower, ind, scale = 10000.0;


  #pragma omp parallel private(t,upper,lower,ind) shared(ard,mask_,ts,b1,b2,nc,nt,nodata,scale) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }

      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {
          if (ard[t].dat[b2][p] != 0){
            upper = (ard[t].dat[b1][p] / (float)ard[t].dat[b2][p]) - 1.0;
            lower = sqrt((ard[t].dat[b1][p] / (float)ard[t].dat[b2][p]) + 1.0);
            ind = upper/lower;
            if (lower == 0 || ind*scale > SHRT_MAX || ind*scale < SHRT_MIN){
              ts->tss_[t][p] = nodata;
            } else {
              ts->tss_[t][p] = (short)(ind*scale);
            }
          } else {
            ts->tss_[t][p] = nodata;
          }
        }

      }

    }
  }

  return;
}

/** This function computes a spectral index time series, with kernelized
+++ Normalized differenced method, e.g. NDVI: (b1-b2/(b1+b2)
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- b1:     band 1
--- b2:     band 2
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_kernelized(ard_t *ard, small *mask_, tsa_t *ts, int b1, int b2, int nc, int nt, short nodata){
int p, t;
float sigma, diff, tmp, ind, scale = 10000.0;


  #pragma omp parallel private(t,sigma,diff,tmp,ind) shared(ard,mask_,ts,b1,b2,nc,nt,nodata,scale) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p] || 
             ard[t].dat[b1][p] <= 0 || 
             ard[t].dat[b2][p] <= 0){
          ts->tss_[t][p] = nodata;
        } else {
          sigma = 0.5 * (ard[t].dat[b1][p] + ard[t].dat[b2][p]);
          diff  = ard[t].dat[b1][p] - ard[t].dat[b2][p];
          tmp   = exp(-(diff*diff) / (2*sigma*sigma));
          ind   = (1-tmp) / (1+tmp);
          ts->tss_[t][p] = (short)(ind*scale);
        }

      }

    }
  }

  return;
}


/** This function computes a spectral index time series, with Normalized
+++ differenced method with resistance terms, e.g. EVI, SARVI, ...: 
+++ f1*(nir-red)/(nir+f2*red-f3*blue+f4*scale)
+++ red might be red-blue corrected: red -= (blue-red)
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- n:      band 1 (nir)
--- r:      band 2 (red)
--- b:      band 3 (blue)
--- f1:     correction factor 1
--- f2:     correction factor 2
--- f3:     correction factor 3
--- f4:     correction factor 4
--- rbc:    do red-blue correction?
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_resistance(ard_t *ard, small *mask_, tsa_t *ts, int n, int r, int b, float f1, float f2, float f3, float f4, bool rbc, int nc, int nt, short nodata){
int p, t;
float tmp, x = 0, nir, red, blue, ind, scale = 10000.0;


  if (rbc) x = 1.0;

  #pragma omp parallel private(t,tmp,nir,red,blue,ind) shared(ard,mask_,ts,n,r,b,nc,nt,nodata,scale,f1,f2,f3,f4,x) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {
          nir  = ard[t].dat[n][p];
          red  = ard[t].dat[r][p];
          blue = ard[t].dat[b][p];
          red -= x*(blue-red);
          tmp = nir+f2*red-f3*blue+f4*scale;
          ind = f1*(nir-red)/tmp;
          if (tmp == 0){// || ind < -1 || ind > 1){
            ts->tss_[t][p] = nodata;
          } else {
            ts->tss_[t][p] = (short)(ind*scale);
          }
        }

      }

    }
  }

  return;
}


/** This function computes a spectral index time series, with Tasseled Cap
+++ method
--- ard:    ARD
--- mask_:  mask image
--- ts:     pointer to instantly useable TSA image arrays
--- type:   Tasseled Cap component
--- b:      band 1 (blue)
--- g:      band 2 (green)
--- r:      band 3 (red)
--- n:      band 4 (nir)
--- s1:     band 5 (sw1)
--- s2:     band 6 (sw1)
--- nc:     number of cells
--- nt:     number of ARD products over time
--- nodata: nodata value
+++ Return: void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_tasseled(ard_t *ard, small *mask_, tsa_t *ts, int type, int b, int g, int r, int n, int s1, int s2, int nc, int nt, short nodata){
int p, t, i, comp0 = type, comp1 = type+1;
float tmp, x[3] = { 1, 1, 1 }, ind;
float tc[3][6] = {
{ 0.2043,  0.4158,  0.5524, 0.5741,  0.3124,  0.2303 },
{-0.1603, -0.2819, -0.4934, 0.7940, -0.0002, -0.1446 },
{ 0.0315,  0.2021,  0.3102, 0.1594, -0.6806, -0.6109 }};


  if (type == TCD){ comp0 = 0; comp1 = 3; x[1] = -1; x[2] = -1;}

  #pragma omp parallel private(t,i,tmp,ind) shared(ard,mask_,ts,b,g,r,n,s1,s2,nc,nt,nodata,x,tc,comp0,comp1) default(none)
  {

    #pragma omp for
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
        } else {
          for (i=comp0, ind=0; i<comp1; i++){
            tmp = tc[i][0]*ard[t].dat[b][p]  + tc[i][1]*ard[t].dat[g][p] + 
                  tc[i][2]*ard[t].dat[r][p]  + tc[i][3]*ard[t].dat[n][p] + 
                  tc[i][4]*ard[t].dat[s1][p] + tc[i][5]*ard[t].dat[s2][p];
            ind += x[i]*tmp;
          }
          ts->tss_[t][p] = (short)ind;

        }

      }

    }
  }

  return;
}


/** Compute spectral mixture analysis index
+++ This function computes SMA fractions for one date. Only one fraction
+++ is retained.
--- ard:       ARD
--- mask_:     mask image
--- ts:        pointer to instantly useable TSA image arrays
--- nc:        number of cells
--- nt:        number of ARD products over time
--- nodata:    nodata value
--- sma:       SMA parameters
--- endmember: endmember (if SMA was selected)
+++ Return:    void
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
void index_unmixed(ard_t *ard, small *mask_, tsa_t *ts, int nc, int nt, short nodata, par_sma_t *sma, aux_emb_t *endmember){
int p, t;
int i, j, ik, jk;
int it, itmax;
int m, nP, sign;
int M = endmember->ne;
int L = endmember->nb;
double tol = FLT_MIN;
double s_min, alpha;
double f = 1.0;
double rsum, dsum, res;
float scale = 10000.0;

gsl_matrix *Z   =  NULL;
gsl_vector *x =    NULL;
gsl_matrix *ZtZ =  NULL;
gsl_vector *Ztx =  NULL;
gsl_vector *P   =  NULL;
gsl_vector *R   =  NULL;
gsl_vector *d   =  NULL;
gsl_vector *s   =  NULL;
gsl_vector *w   =  NULL;
gsl_vector *a   =  NULL;


  if (endmember->nb != get_brick_nbands(ard[0].DAT)){
    printf("number of bands in endmember file and ARD is different.\n"); exit(1);}
    

  itmax = 30*M;

  if (sma->sto) L++;

  // allocate working variables
  Z   = gsl_matrix_alloc(L, M);
  ZtZ = gsl_matrix_alloc(M, M);


  // copy endmember to GSL matrix
  for (i=0; i<endmember->nb; i++){
  for (j=0; j<M; j++){
    gsl_matrix_set(Z, i, j, endmember->tab[i][j]);
  }
  }
  if (sma->sto){ // append a row of 1
    for (j=0; j<M; j++) gsl_matrix_set(Z, i, j, 1);
  }

  #ifdef FORCE_DEBUG
  printf("Endmember matrix_:\n");
  for (i=0; i<L; i++){
  for (j=0; j<M; j++){
    printf(" %.2f", gsl_matrix_get(Z, i, j));
  }
  printf("\n");
  }
  #endif

  // pre-compute crossproduct of Z_: t(Z)Z
  gsl_blas_dgemm(CblasTrans, CblasNoTrans, 1.0, Z, Z, 0.0, ZtZ);


  #pragma omp parallel firstprivate(f) private(t,i,j,ik,jk,it,x,Ztx,P,R,d,s,w,a,nP,m,alpha,s_min,sign,dsum,rsum,res) shared(ard,mask_,ts,sma,nc,nt,nodata,itmax,tol,L,M,Z,ZtZ,stdout,scale,endmember) default(none)
  {

    // allocate working variables
    x   = gsl_vector_alloc(L);
    Ztx = gsl_vector_alloc(M);
    P   = gsl_vector_alloc(M);
    R   = gsl_vector_alloc(M);
    d   = gsl_vector_alloc(M);
    s   = gsl_vector_alloc(M);
    w   = gsl_vector_alloc(M);
    a   = gsl_vector_alloc(M);
    
    // set last element of spectral vector to 1
    if (sma->sto) gsl_vector_set(x, L-1, 1);

    
    #pragma omp for schedule(dynamic,1)
    for (p=0; p<nc; p++){

      if (mask_ != NULL && !mask_[p]){
        for (t=0; t<nt; t++) ts->tss_[t][p] = nodata;
        continue;
      }
      
      for (t=0; t<nt; t++){

        if (!ard[t].msk[p]){
          ts->tss_[t][p] = nodata;
          if (sma->orms) ts->rms_[t][p] = nodata;
        } else {

          it = 0;

          // copy spectrum to GSL vector
          for (i=0; i<endmember->nb; i++) gsl_vector_set(x, i, (double)(ard[t].dat[i][p]/scale));

          // compute crossproduct of Z and x_: t(Z)x
          gsl_blas_dgemv(CblasTrans, 1.0, Z, x, 0.0, Ztx);

          /** unconstrained inversion (negative values are allowed) **/
          if (!sma->pos){

            // allocate working variables
            gsl_matrix *ZtZ_ = gsl_matrix_alloc(M, M);
            gsl_matrix *inv = gsl_matrix_alloc(M, M);
            gsl_permutation *per = gsl_permutation_alloc(M);

            // copy t(Z)Z, as it will be modified by the inversion
            gsl_matrix_memcpy(ZtZ_, ZtZ);
            
            // LU decomposition + matrix inversion
            gsl_linalg_LU_decomp(ZtZ_, per, &sign); 
            gsl_linalg_LU_invert(ZtZ_, per, inv);

            // d = (t(Z)Z)^-1 t(z)x
            gsl_blas_dgemv(CblasNoTrans, 1.0, inv, Ztx, 0.0, d);

            // free working variables
            gsl_matrix_free(ZtZ_);
            gsl_permutation_free(per);
            gsl_matrix_free(inv);

          /** constrained inversion (negative values are not allowed) **/
          } else {

            // A_: initialization (A1-A3)
            for (i=0; i<M; i++){
              gsl_vector_set(P, i, 0);
              gsl_vector_set(R, i, 1);
              gsl_vector_set(d, i, 0);
              gsl_vector_set(s, i, 0);
              gsl_vector_set(a, i, INT_MAX);
            }

            // compute w = t(Z)x - t(Z)Zd (A4)
            gsl_blas_dgemv(CblasNoTrans, 1.0, ZtZ, d, 0.0, w);
            for (i=0; i<M; i++) gsl_vector_set(w, i, 
              gsl_vector_get(Ztx, i) - gsl_vector_get(w, i));

            // B_: main loop (B1)
            while (gsl_vector_max(R) > 0 && 
                   gsl_vector_max(w) > tol){

              // index of maximum w (B2)
              m = gsl_vector_max_index(w);

              // add to passive, remove from active set (B3)
              gsl_vector_set(P, m, 1);
              gsl_vector_set(R, m, 0);

              // number of passive elements
              nP = gsl_blas_dasum(P);

              // allocate subsets for the matrices
              gsl_matrix *ZtZ_ = gsl_matrix_alloc(nP, nP);
              gsl_vector *Ztx_ = gsl_vector_alloc(nP);
              gsl_vector *s_ = gsl_vector_alloc(nP);
              gsl_matrix *inv = gsl_matrix_alloc(nP, nP);
              gsl_permutation *per = gsl_permutation_alloc(nP);          

              // subset the matrices according to the passive set
              for (i=0, ik=0; i<M; i++){
                if (gsl_vector_get(P, i) == 1){
                  gsl_vector_set(Ztx_, ik, gsl_vector_get(Ztx, i));
                  for (j=0, jk=0; j<M; j++){
                    if (gsl_vector_get(P, j) == 1){
                      gsl_matrix_set(ZtZ_, ik, jk, gsl_matrix_get(ZtZ, i, j));
                      jk++;
                    }
                  }
                  ik++;
                }
              }

              // unconstrained inversion of the passive set (B4)

              // LU decomposition + matrix inversion
              gsl_linalg_LU_decomp(ZtZ_, per, &sign); 
              gsl_linalg_LU_invert(ZtZ_, per, inv);

              // compute s = (t(Z)Z)^-1 t(z)x for the passive subset
              gsl_blas_dgemv(CblasNoTrans, 1.0, inv, Ztx_, 0.0, s_);

              // minimum of s in the passive set
              s_min = gsl_vector_min(s_);

              // copy subsetted s to complete s, set active elements to 0
              for (i=0, ik=0; i<M; i++){
                if (gsl_vector_get(P, i) == 1){
                  gsl_vector_set(s, i, gsl_vector_get(s_, ik++));
                } else {
                  gsl_vector_set(s, i, 0);
                }
              }

              // free working variables
              gsl_matrix_free(ZtZ_);
              gsl_vector_free(Ztx_);
              gsl_vector_free(s_);
              gsl_permutation_free(per);
              gsl_matrix_free(inv);

              // C_: inner loop (C1)
              while (s_min <= 0 && it < itmax){

                it++;

                // compute alpha = -min(d/d-s) of passive set (C2)
                for (i=0; i<M; i++){
                  if (gsl_vector_get(s, i) <= tol && gsl_vector_get(P, i) == 1){
                    gsl_vector_set(a, i, gsl_vector_get(d, i) / 
                        (gsl_vector_get(d, i) - gsl_vector_get(s, i)));
                  } else {
                    gsl_vector_set(a, i, INT_MAX);
                  }
                }
                alpha = gsl_vector_min(a);

                // compute new d = d + alpha(s-d) for all elements (C3)
                for (i=0; i<M; i++) gsl_vector_set(d, i, gsl_vector_get(d, i) + 
                  alpha*(gsl_vector_get(s, i)-gsl_vector_get(d, i)));

                // remove from passive and add to active set (C4)
                for (i=0; i<M; i++){
                  if (fabs(gsl_vector_get(d, i)) < tol && gsl_vector_get(P, i) == 1){
                    gsl_vector_set(P, i, 0);
                    gsl_vector_set(R, i, 1);
                  }
                }

                // number of passive elements
                nP = gsl_blas_dasum(P);

                // allocate subsets for the matrices
                gsl_matrix *ZtZ_ = gsl_matrix_alloc(nP, nP);
                gsl_vector *Ztx_ = gsl_vector_alloc(nP);
                gsl_vector *s_ = gsl_vector_alloc(nP);
                gsl_matrix *inv = gsl_matrix_alloc(nP, nP);
                gsl_permutation *per = gsl_permutation_alloc(nP);

                // subset the matrices according to the passive set
                for (i=0, ik=0; i<M; i++){
                  if (gsl_vector_get(P, i) == 1){
                    gsl_vector_set(Ztx_, ik, gsl_vector_get(Ztx, i));
                    for (j=0, jk=0; j<M; j++){
                      if (gsl_vector_get(P, j) == 1){
                        gsl_matrix_set(ZtZ_, ik, jk, gsl_matrix_get(ZtZ, i, j));
                        jk++;
                      }
                    }
                    ik++;
                  }
                }

                // unconstrained inversion of the passive set (C5)

                // LU decomposition + matrix inversion
                gsl_linalg_LU_decomp(ZtZ_, per, &sign); 
                gsl_linalg_LU_invert(ZtZ_, per, inv);

                // compute s = (t(Z)Z)^-1 t(z)x for the passive subset
                gsl_blas_dgemv(CblasNoTrans, 1.0, inv, Ztx_, 0.0, s_);

                // minimum of s in the passive set
                s_min = gsl_vector_min(s_);

                // copy subsetted s to complete s, set active elements to 0 (C5-6)
                for (i=0, ik=0; i<M; i++){
                  if (gsl_vector_get(P, i) == 1){
                    gsl_vector_set(s, i, gsl_vector_get(s_, ik++));
                  } else {
                    gsl_vector_set(s, i, 0);
                  }
                }

                // free working variables
                gsl_matrix_free(ZtZ_);
                gsl_vector_free(Ztx_);
                gsl_vector_free(s_);
                gsl_permutation_free(per);
                gsl_matrix_free(inv);


              }

              // update d with s (B5)
              gsl_vector_memcpy(d, s);

              // compute w = t(Z)x - t(Z)Zd (B6)
              // set passive elements to -1, such that the 
              // looping condition is constrained to the active set
              gsl_blas_dgemv(CblasNoTrans, 1.0, ZtZ, d, 0.0, w);
              for (i=0; i<M; i++){
                if (gsl_vector_get(R, i) == 1){
                  gsl_vector_set(w, i, 
                    gsl_vector_get(Ztx, i) - gsl_vector_get(w, i));
                } else {
                  gsl_vector_set(w, i, -1);
                }
              }

            }

          }


          // RMSE
          rsum = 0;
          for (i=0; i<L; i++){
            dsum = 0;
            for (j=0; j<M; j++){
              dsum += gsl_vector_get(d, j)*gsl_matrix_get(Z, i, j);
            }
            res = gsl_vector_get(x, i)-dsum;
            rsum += res*res;
          }
          if (sma->orms) ts->rms_[t][p] = (short)(sqrt(rsum/L)*scale);


          // apply optional shade normalization (shade must be the last endmember)
          if (sma->shn){
            f = 1.0 / (1.0 - gsl_vector_get(d, M-1));
            for (i=0; i<M-1; i++) gsl_vector_set(d, i, gsl_vector_get(d, i)*f);
            gsl_vector_set(d, i, 0);

          }

          // retain one fraction
          ts->tss_[t][p] = (short)(gsl_vector_get(d, sma->emb-1)*scale);
          
        }

      }
      
    }


    // free working variables
    gsl_vector_free(x);
    gsl_vector_free(Ztx);
    gsl_vector_free(P);
    gsl_vector_free(R);
    gsl_vector_free(d);
    gsl_vector_free(s);
    gsl_vector_free(w);
    gsl_vector_free(a);
    
  }
  
  gsl_matrix_free(Z);
  gsl_matrix_free(ZtZ);

  return;
}


/** public functions
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/


/** This function computes a spectral index time series
--- ard:       ARD
--- ts:        pointer to instantly useable TSA image arrays
--- mask_:     mask image
--- nc:        number of cells
--- nt:        number of ARD products over time
--- idx:       spectral index
--- nodata:    nodata value
--- tsa:       TSA parameters
--- sen:       sensor parameters
--- endmember: endmember (if SMA was selected)
+++ Return:    SUCCESS/FAILURE
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++**/
int tsa_spectral_index(ard_t *ard, tsa_t *ts, small *mask_, int nc, int nt, int idx, short nodata, par_tsa_t *tsa, par_sen_t *sen, aux_emb_t *endmember){


  switch (tsa->index[idx]){
    case _IDX_BLU_:
      index_band(ard, mask_, ts, sen->blue, nc, nt, nodata);
      break;
    case _IDX_GRN_:
      index_band(ard, mask_, ts, sen->green, nc, nt, nodata);
      break;
    case _IDX_RED_:
      index_band(ard, mask_, ts, sen->red, nc, nt, nodata);
      break;
    case _IDX_NIR_:
      index_band(ard, mask_, ts, sen->nir, nc, nt, nodata);
      break;
    case _IDX_SW0_:
      index_band(ard, mask_, ts, sen->swir0, nc, nt, nodata);
      break;
    case _IDX_SW1_:
      index_band(ard, mask_, ts, sen->swir1, nc, nt, nodata);
      break;
    case _IDX_SW2_:
      index_band(ard, mask_, ts, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_RE1_:
      index_band(ard, mask_, ts, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_RE2_:
      index_band(ard, mask_, ts, sen->rededge2, nc, nt, nodata);
      break;
    case _IDX_RE3_:
      index_band(ard, mask_, ts, sen->rededge3, nc, nt, nodata);
      break;
    case _IDX_BNR_:
      index_band(ard, mask_, ts, sen->bnir, nc, nt, nodata);
      break;
    case _IDX_NDV_:
      cite_me(_CITE_NDVI_);
      index_differenced(ard, mask_, ts, sen->nir, sen->red, nc, nt, nodata);
      break;
    case _IDX_EVI_:
      cite_me(_CITE_EVI_);
      index_resistance(ard, mask_, ts, sen->nir, sen->red, sen->blue, 
                       2.5, 6.0, 7.5, 1.0, false, nc, nt, nodata);
      break;
    case _IDX_NBR_:
       cite_me(_CITE_NBR_);
      index_differenced(ard, mask_, ts, sen->nir, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_ARV_:
      cite_me(_CITE_SARVI_);
      index_resistance(ard, mask_, ts, sen->nir, sen->red, sen->blue, 
                       1.0, 1.0, 0.0, 0.0, true, nc, nt, nodata);
      break;
    case _IDX_SAV_:
      cite_me(_CITE_SARVI_);
      index_resistance(ard, mask_, ts, sen->nir, sen->red, sen->blue, 
                       1.5, 1.0, 0.0, 0.5, false, nc, nt, nodata);
      break;
    case _IDX_SRV_:
      cite_me(_CITE_SARVI_);
      index_resistance(ard, mask_, ts, sen->nir, sen->red, sen->blue, 
                       1.5, 1.0, 0.0, 0.5, true, nc, nt, nodata);
      break;
    case _IDX_TCB_:
      cite_me(_CITE_TCAP_);
      index_tasseled(ard, mask_, ts, TCB, sen->blue, sen->green, sen->red, 
                     sen->nir, sen->swir1, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_TCG_:
      cite_me(_CITE_TCAP_);
      index_tasseled(ard, mask_, ts, TCG, sen->blue, sen->green, sen->red, 
                     sen->nir, sen->swir1, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_TCW_:
      cite_me(_CITE_TCAP_);
      index_tasseled(ard, mask_, ts, TCW, sen->blue, sen->green, sen->red, 
                     sen->nir, sen->swir1, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_TCD_:
      cite_me(_CITE_DISTURBANCE_);
      index_tasseled(ard, mask_, ts, TCD, sen->blue, sen->green, sen->red, 
                     sen->nir, sen->swir1, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_NDB_:
      cite_me(_CITE_NDBI_);
      index_differenced(ard, mask_, ts, sen->swir1, sen->nir, nc, nt, nodata);
      break;
    case _IDX_NDW_:
      cite_me(_CITE_NDWI_);
      index_differenced(ard, mask_, ts, sen->green, sen->nir, nc, nt, nodata);
      break;
    case _IDX_MNW_:
      cite_me(_CITE_MNDWI_);
      index_differenced(ard, mask_, ts, sen->green, sen->swir1, nc, nt, nodata);
      break;
    case _IDX_NDS_:
      cite_me(_CITE_NDSI_);
      index_differenced(ard, mask_, ts, sen->green, sen->swir1, nc, nt, nodata);
      break;
    case _IDX_SMA_:
      cite_me(_CITE_SMA_);
      index_unmixed(ard, mask_, ts, nc, nt, nodata, &tsa->sma, endmember);
      break;
    case _IDX_BVV_:
      index_band(ard, mask_, ts, sen->vv, nc, nt, nodata);
      break;
    case _IDX_BVH_:
      index_band(ard, mask_, ts, sen->vh, nc, nt, nodata);
      break;
    case _IDX_NDT_:
      cite_me(_CITE_NDTI_);
      index_differenced(ard, mask_, ts, sen->swir1, sen->swir2, nc, nt, nodata);
      break;
    case _IDX_NDM_:
      cite_me(_CITE_NDMI_);
      index_differenced(ard, mask_, ts, sen->nir, sen->swir1, nc, nt, nodata);
      break;
    case _IDX_KNV_:
      cite_me(_CITE_KNDVI_);
      index_kernelized(ard, mask_, ts, sen->nir, sen->red, nc, nt, nodata);
      break;
    case _IDX_ND1_:
      cite_me(_CITE_NDRE1_);
      index_differenced(ard, mask_, ts, sen->rededge2, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_ND2_:
      cite_me(_CITE_NDRE2_);
      index_differenced(ard, mask_, ts, sen->rededge3, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_CRE_:
      cite_me(_CITE_CIre_);
      index_ratio_minus1(ard, mask_, ts, sen->rededge3, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_NR1_:
      cite_me(_CITE_NDVIre1_);
      index_differenced(ard, mask_, ts, sen->bnir, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_NR2_:
      cite_me(_CITE_NDVIre2_);
      index_differenced(ard, mask_, ts, sen->bnir, sen->rededge2, nc, nt, nodata);
      break;
    case _IDX_NR3_:
      cite_me(_CITE_NDVIre3_);
      index_differenced(ard, mask_, ts, sen->bnir, sen->rededge3, nc, nt, nodata);
      break;
    case _IDX_N1n_:
      cite_me(_CITE_NDVIre1n_);
      index_differenced(ard, mask_, ts, sen->nir, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_N2n_:
      cite_me(_CITE_NDVIre2n_);
      index_differenced(ard, mask_, ts, sen->nir, sen->rededge2, nc, nt, nodata);
      break;
    case _IDX_N3n_:
      cite_me(_CITE_NDVIre3n_);
      index_differenced(ard, mask_, ts, sen->nir, sen->rededge3, nc, nt, nodata);
      break;
    case _IDX_Mre_:
      cite_me(_CITE_MSRre_);
      index_msrre(ard, mask_, ts, sen->bnir, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_Mrn_:
      cite_me(_CITE_MSRren_);
      index_msrre(ard, mask_, ts, sen->nir, sen->rededge1, nc, nt, nodata);
      break;
    case _IDX_CCI_:
      cite_me(_CITE_CCI_);
      index_differenced(ard, mask_, ts, sen->green, sen->red, nc, nt, nodata);
      break;
    case _IDX_EV2_:
      cite_me(_CITE_EV2_);
      index_resistance(ard, mask_, ts, sen->nir, sen->red, sen->red, 
                       2.4, 1.0, 0.0, 1.0, false, nc, nt, nodata);
      break;
    case _IDX_CSW_:
      index_cont_remove(ard, mask_, ts, sen->swir1, sen->nir, sen->swir2, 
                        sen->w_swir1, sen->w_nir, sen->w_swir2, nc, nt, nodata);
      break;
    default:
      printf("unknown INDEX\n");
      break;      
  }

  
  return SUCCESS;
}

