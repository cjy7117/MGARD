/*
 * Copyright 2021, Oak Ridge National Laboratory.
 * MGARD-GPU: MultiGrid Adaptive Reduction of Data Accelerated by GPUs
 * Author: Jieyang Chen (chenj3@ornl.gov)
 * Date: April 2, 2021
 */

#ifndef MGRAD_CUDA_LINEAR_PROCESSING_KERNEL_TEMPLATE
#define MGRAD_CUDA_LINEAR_PROCESSING_KERNEL_TEMPLATE

#include "LinearProcessingKernel.h"
namespace mgard_cuda {
template <uint32_t D, typename T, int R, int C, int F>
__global__ void
_lpk_reo_1(int *shape, int *shape_c, int *ldvs, int *ldws, int processed_n,
           int *processed_dims, int curr_dim_r, int curr_dim_c, int curr_dim_f,
           T *ddist_f, T *dratio_f, T *dv1, int lddv11, int lddv12, T *dv2,
           int lddv21, int lddv22, T *dw, int lddw1, int lddw2) {

  // bool debug = false;
  // if (blockIdx.z == 0 && blockIdx.y == 0 && blockIdx.x == 0 &&
  // threadIdx.z == 0 && threadIdx.y == 0 ) debug = false;

  // bool debug2 = false;
  // if (threadIdx.z == 0 && threadIdx.y == 0 && threadIdx.x == 0 ) debug2 =
  // false;

  size_t threadId = (threadIdx.z * (blockDim.x * blockDim.y)) +
                    (threadIdx.y * blockDim.x) + threadIdx.x;

  T *sm = SharedMemory<T>();
  // extern __shared__ double sm[]; // size: (blockDim.x + 1) * (blockDim.y + 1)
  // * (blockDim.z + 1)
  int ldsm1 = F * 2 + 3;
  int ldsm2 = C;
  T *v_sm = sm;
  T *dist_f_sm = sm + ldsm1 * ldsm2 * R;
  T *ratio_f_sm = dist_f_sm + ldsm1;
  int *shape_sm = (int *)(ratio_f_sm + ldsm1);
  int *shape_c_sm = shape_sm + D;
  int *processed_dims_sm = shape_c_sm + D;
  int *ldvs_sm = processed_dims_sm + D;
  int *ldws_sm = ldvs_sm + D;
  int idx[D];
  if (threadId < D) {
    shape_sm[threadId] = shape[threadId];
    shape_c_sm[threadId] = shape_c[threadId];
    ldvs_sm[threadId] = ldvs[threadId];
    ldws_sm[threadId] = ldws[threadId];
  }
  if (threadId < processed_n) {
    processed_dims_sm[threadId] = processed_dims[threadId];
  }
  __syncthreads();

  for (int d = 0; d < D; d++)
    idx[d] = 0;

  int nr = shape_sm[curr_dim_r];
  int nc = shape_sm[curr_dim_c];
  int nf = shape_sm[curr_dim_f];
  int nf_c = shape_c_sm[curr_dim_f];

  bool zero_other = true;
  bool PADDING = (nf % 2 == 0);

  int bidx = blockIdx.x;
  int firstD;
  if (nf_c % 2 == 1) {
    firstD = div_roundup(nf_c, blockDim.x);
  } else {
    firstD = div_roundup(nf_c, blockDim.x);
  }
  int blockId = bidx % firstD;
  bidx /= firstD;

  for (int d = 0; d < D; d++) {
    if (d != curr_dim_r && d != curr_dim_c && d != curr_dim_f) {
      int t = shape_sm[d];
      for (int k = 0; k < processed_n; k++) {
        if (d == processed_dims[k]) {
          t = shape_c_sm[d];
        }
      }
      idx[d] = bidx % t;
      bidx /= t;
      if (idx[d] >= shape_c_sm[d])
        zero_other = false;
    }
  }

  int zero_r = shape_c_sm[curr_dim_r];
  int zero_c = shape_c_sm[curr_dim_c];
  int zero_f = shape_c_sm[curr_dim_f];

  if (D < 3) {
    nr = 1;
    zero_r = 1;
  }
  if (D < 2) {
    nc = 1;
    zero_c = 1;
  }

  size_t other_offset_v = get_idx<D>(ldvs_sm, idx);
  size_t other_offset_w = get_idx<D>(ldws_sm, idx);

  dv1 = dv1 + other_offset_v;
  dv2 = dv2 + other_offset_v;
  dw = dw + other_offset_w;

  // if (debug2) {
  //   printf("idx: %d %d %d %d\n", idx[3], idx[2], idx[1], idx[0]);
  //   printf("other_offset_v: %llu\n", other_offset_v);
  //   printf("other_offset_w: %llu\n", other_offset_w);
  // }
  register int r_gl = blockIdx.z * blockDim.z + threadIdx.z;
  register int c_gl = blockIdx.y * blockDim.y + threadIdx.y;
  register int f_gl = blockId * blockDim.x + threadIdx.x;

  register int r_sm = threadIdx.z;
  register int c_sm = threadIdx.y;
  register int f_sm = threadIdx.x;

  int actual_F = F;
  if (nf_c - blockId * blockDim.x < F) {
    actual_F = nf_c - blockId * blockDim.x;
  }

  // if (nf_c % 2 == 1){
  //   if(nf_c-1 - blockId * blockDim.x < F) { actual_F = nf_c - 1 - blockId *
  //   blockDim.x; }
  // } else {
  //   if(nf_c - blockId * blockDim.x < F) { actual_F = nf_c - blockId *
  //   blockDim.x; }
  // }

  // if (debug) printf("actual_F %d\n", actual_F);

  if (r_gl < nr && c_gl < nc && f_gl < nf_c) {
    if (r_gl < zero_r && c_gl < zero_c && f_gl < zero_f) {
      // if (debug) printf("load left vsm[%d]: 0.0\n", f_sm * 2 + 2);
      v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 2)] = 0.0;
    } else {
      // if (debug) printf("load left vsm[%d]<-dv1[%d, %d, %d]: %f\n", f_sm * 2
      // + 2, r_gl, c_gl, f_gl, dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)]);
      v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 2)] =
          dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)];
    }

    if (f_sm == actual_F - 1) {
      if (r_gl < zero_r && c_gl < zero_c && f_gl < zero_f) {
        // if (debug) printf("load left+1 vsm[%d]: 0.0\n", actual_F * 2 + 2);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 2)] = 0.0;
      } else {
        if (f_gl + 1 < nf_c) {
          // if (debug) printf("load left+1 vsm[%d]: %f\n", actual_F * 2 + 2,
          // dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl + 1)]);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 2)] =
              dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl + 1)];
        } else {
          // if (debug) printf("load left+1 vsm[%d]: 0.0\n", actual_F * 2 + 2);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 2)] = 0.0;
        }
      }
    }

    if (f_sm == 0) {
      // left
      if (r_gl < zero_r && c_gl < zero_c && f_gl < zero_f) {
        // coarse (-1)
        // if (debug) printf("load left-1 vsm[0]: 0.0\n");
        v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 0)] = 0.0;
      } else {
        if (f_gl - 1 >= 0) {
          // other (-1)
          // if (debug) printf("load left-1 vsm[0]: %f\n", dv1[get_idx(lddv11,
          // lddv12, r_gl, c_gl, f_gl-1)]);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 0)] =
              dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl - 1)];
        } else {
          // other (-1)
          // if (debug) printf("load left-1 vsm[0]: 0.0\n");
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 0)] = 0.0;
        }
      }
    }

    // right
    if (!PADDING) {
      if (nf_c % 2 != 0) {
        if (f_gl - 1 >= 0 && f_gl - 1 < nf_c - 1) {
          // if (debug) printf("load right vsm[%d]: %f <- %d %d %d\n", f_sm * 2
          // + 1, dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - 1)], r_gl,
          // c_gl, f_gl - 1);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 1)] =
              dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - 1)];
        } else {
          // if (debug) printf("load right vsm[%d]: 0\n", f_sm * 2 + 1);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 1)] = 0.0;
        }
      } else { // nf_c % 2 == 0
        if (f_gl < nf_c - 1) {
          // if (debug) printf("load right vsm[%d]: %f <- %d %d %d\n", f_sm * 2
          // + 3, dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)], r_gl, c_gl,
          // f_gl);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 3)] =
              dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
        } else {
          // if (debug) printf("load right vsm[%d]: 0\n", f_sm * 2 + 3);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 3)] = 0.0;
        }
      }
    } else { // PADDING
      if (nf_c % 2 != 0) {
        if (f_gl - 1 >= 0 && f_gl - 1 < nf_c - 2) {
          // if (debug) printf("load right vsm[%d]: %f <- %d %d %d\n", f_sm * 2
          // + 1, dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - 1)], r_gl,
          // c_gl, f_gl - 1);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 1)] =
              dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - 1)];
        } else {
          // if (debug) printf("load right vsm[%d]: 0\n", f_sm * 2 + 1);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 1)] = 0.0;
        }
      } else { // nf_c % 2 == 0
        if (f_gl < nf_c - 2) {
          // if (debug) printf("load right vsm[%d]: %f <- %d %d %d\n", f_sm * 2
          // + 3, dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)], r_gl, c_gl,
          // f_gl);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 3)] =
              dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
        } else {
          // if (debug) printf("load right vsm[%d]: 0\n", f_sm * 2 + 3);
          v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 3)] = 0.0;
        }
      }
    }

    if (f_sm == actual_F - 1) {
      // right (+1)
      if (!PADDING) {
        if (nf_c % 2 != 0) {
          if (f_gl < nf_c - 1) {
            // if (debug) printf("load right+1 vsm[%d]: %f <- %d %d %d\n",
            // actual_F * 2 + 1, dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)],
            // r_gl, c_gl, f_gl);
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 1)] =
                dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
          } else {
            // if (debug) printf("load right+1 vsm[%d]: 0.0\n", actual_F * 2 +
            // 1);
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 1)] = 0.0;
          }
        } else { // nf_c % 2 == 0
          if (f_gl - actual_F >= 0) {
            // if (debug) printf("load right-1 vsm[1]: %f <- %d %d %d\n",
            // dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - actual_F)], r_gl,
            // c_gl, f_gl - actual_F);
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 1)] =
                dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - actual_F)];
          } else {
            // if (debug) printf("load right-1 vsm[1]: 0.0\n");
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 1)] = 0.0;
          }
        }
      } else {
        if (nf_c % 2 != 0) {
          if (f_gl < nf_c - 2) {
            // if (debug) printf("actual_F(%d), load right+1 vsm[%d]: %f <- %d
            // %d %d\n", actual_F, actual_F * 2 + 1, dv2[get_idx(lddv21, lddv22,
            // r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 1)] =
                dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
          } else {
            // if (debug) printf("load right+1 vsm[%d]: 0.0\n", actual_F * 2 +
            // 1);
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, actual_F * 2 + 1)] = 0.0;
          }
        } else { // nf_c % 2 == 0
          if (f_gl - actual_F >= 0 && f_gl - actual_F < nf_c - 2) {
            // if (debug) printf("load right-1 vsm[1]: %f <- %d %d %d\n",
            // dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - actual_F)], r_gl,
            // c_gl, f_gl - actual_F);
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 1)] =
                dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl - actual_F)];
          } else {
            // if (debug) printf("load right-1 vsm[1]: 0.0\n");
            v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, 1)] = 0.0;
          }
        }
      }
    }
  }

  if (r_sm == 0 && c_sm == 0 && f_sm < actual_F) {
    if (blockId * F * 2 + f_sm < nf - 1) {
      dist_f_sm[2 + f_sm] = ddist_f[blockId * F * 2 + f_sm];
      ratio_f_sm[2 + f_sm] = dratio_f[blockId * F * 2 + f_sm];
    } else {
      dist_f_sm[2 + f_sm] = 0.0;
      ratio_f_sm[2 + f_sm] = 0.0;
    }

    if (blockId * F * 2 + actual_F + f_sm < nf - 2) {
      dist_f_sm[2 + actual_F + f_sm] =
          ddist_f[blockId * F * 2 + actual_F + f_sm];
      ratio_f_sm[2 + actual_F + f_sm] =
          dratio_f[blockId * F * 2 + actual_F + f_sm];
    } else {
      dist_f_sm[2 + actual_F + f_sm] = 0.0;
      ratio_f_sm[2 + actual_F + f_sm] = 0.0;
    }
    // dist_f_sm[2 + f_sm] = ddist_f[f_gl];
    // dist_f_sm[2 + actual_F + f_sm] = ddist_f[actual_F + f_gl];
    // ratio_f_sm[2 + f_sm] = dratio_f[f_gl];
    // ratio_f_sm[2 + actual_F + f_sm] = dratio_f[actual_F + f_gl];
  }

  if (blockId > 0) {
    if (f_sm < 2) {
      dist_f_sm[f_sm] = ddist_f[f_gl - 2];
      ratio_f_sm[f_sm] = dratio_f[f_gl - 2];
    }
  } else {
    if (f_sm < 2) {
      dist_f_sm[f_sm] = 0.0;
      ratio_f_sm[f_sm] = 0.0;
    }
  }

  __syncthreads();

  if (r_gl < nr && c_gl < nc && f_gl < nf_c) {
    T h1 = dist_f_sm[f_sm * 2];
    T h2 = dist_f_sm[f_sm * 2 + 1];
    T h3 = dist_f_sm[f_sm * 2 + 2];
    T h4 = dist_f_sm[f_sm * 2 + 3];
    T r1 = ratio_f_sm[f_sm * 2];
    T r2 = ratio_f_sm[f_sm * 2 + 1];
    T r3 = ratio_f_sm[f_sm * 2 + 2];
    T r4 = 1 - r3;
    T a = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2)];
    T b = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 1)];
    T c = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 2)];
    T d = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 3)];
    T e = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm, f_sm * 2 + 4)];

    // if (debug) {
    //   printf("f_sm(%d) %f %f %f %f %f\n",f_sm, a,b,c,d,e);
    //   printf("f_sm_h(%d) %f %f %f %f\n",f_sm, h1,h2,h3,h4);
    //   printf("f_sm_r(%d) %f %f %f %f\n",f_sm, r1,r2,r3,r4);
    // }

    // T tb = a * h1 + b * 2 * (h1+h2) + c * h2;
    // T tc = b * h2 + c * 2 * (h2+h3) + d * h3;
    // T td = c * h3 + d * 2 * (h3+h4) + e * h4;

    // if (debug) printf("f_sm(%d) tb tc td tc: %f %f %f %f\n", f_sm, tb, tc,
    // td, tc+tb * r1 + td * r4);

    // tc += tb * r1 + td * r4;

    dw[get_idx(lddw1, lddw2, r_gl, c_gl, f_gl)] =
        mass_trans(a, b, c, d, e, h1, h2, h3, h4, r1, r2, r3, r4);

    // if (debug) printf("store[%d %d %d] %f \n", r_gl, c_gl, f_gl,
    //           mass_trans(a, b, c, d, e, h1, h2, h3, h4, r1, r2, r3, r4));

    // printf("test block %d F %d nf %d\n", blockId, F, nf);
    // if (f_gl+1 == nf_c-1) {

    //     // T te = h4 * d + 2 * h4 * e;
    //     //printf("f_sm(%d) mm-e: %f\n", f_sm, te);
    //     // te += td * r3;
    //     dw[get_idx(lddw1, lddw2, r_gl, c_gl, f_gl+1)] =
    //       mass_trans(c, d, e, (T)0.0, (T)0.0, h1, h2, (T)0.0, (T)0.0, r1, r2,
    //       (T)0.0, (T)0.0);
    // }
  }
}

template <uint32_t D, typename T, int R, int C, int F>
void lpk_reo_1_adaptive_launcher(
    Handle<D, T> &handle, thrust::device_vector<int> shape,
    thrust::device_vector<int> shape_c, thrust::device_vector<int> ldvs,
    thrust::device_vector<int> ldws, thrust::device_vector<int> processed_dims,
    int curr_dim_r, int curr_dim_c, int curr_dim_f, T *ddist_f, T *dratio_f,
    T *dv1, int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
    int lddw1, int lddw2, int queue_idx) {
  int nr = shape[curr_dim_r];
  int nc = shape[curr_dim_c];
  int nf = shape[curr_dim_f];
  int nf_c = shape_c[curr_dim_f];

  int total_thread_z = nr;
  int total_thread_y = nc;
  int total_thread_x = nf_c;
  // if (nf_c % 2 == 1) { total_thread_x = nf_c - 1; }
  // else { total_thread_x = nf_c; }
  int tbx, tby, tbz, gridx, gridy, gridz;
  dim3 threadsPerBlock, blockPerGrid;
  size_t sm_size;

  tbz = std::min(R, total_thread_z);
  tby = std::min(C, total_thread_y);
  tbx = std::min(F, total_thread_x);
  sm_size = (R * C * (F * 2 + 3) + (F * 2 + 3) * 2) * sizeof(T);
  sm_size += (D * 5) * sizeof(int);
  gridz = ceil((float)total_thread_z / tbz);
  gridy = ceil((float)total_thread_y / tby);
  gridx = ceil((float)total_thread_x / tbx);
  for (int d = 0; d < D; d++) {
    if (d != curr_dim_f && d != curr_dim_c && d != curr_dim_r) {
      int t = shape[d];
      for (int k = 0; k < processed_dims.size(); k++) {
        if (d == processed_dims[k]) {
          t = shape_c[d];
        }
      }
      gridx *= t;
    }
  }
  threadsPerBlock = dim3(tbx, tby, tbz);
  blockPerGrid = dim3(gridx, gridy, gridz);
  // printf("lpk_reo_1 exec config (%d %d %d) (%d %d %d)\n", tbx, tby, tbz,
  // gridx, gridy, gridz);
  _lpk_reo_1<D, T, R, C, F><<<blockPerGrid, threadsPerBlock, sm_size,
                              *(cudaStream_t *)handle.get(queue_idx)>>>(
      thrust::raw_pointer_cast(shape.data()),
      thrust::raw_pointer_cast(shape_c.data()),
      thrust::raw_pointer_cast(ldvs.data()),
      thrust::raw_pointer_cast(ldws.data()), processed_dims.size(),
      thrust::raw_pointer_cast(processed_dims.data()), curr_dim_r, curr_dim_c,
      curr_dim_f, ddist_f, dratio_f, dv1, lddv11, lddv12, dv2, lddv21, lddv22,
      dw, lddw1, lddw2);
  gpuErrchk(cudaGetLastError());
#ifdef MGARD_CUDA_DEBUG
  gpuErrchk(cudaDeviceSynchronize());
#endif
}

template <uint32_t D, typename T>
void lpk_reo_1(Handle<D, T> &handle, thrust::device_vector<int> shape,
               thrust::device_vector<int> shape_c,
               thrust::device_vector<int> ldvs, thrust::device_vector<int> ldws,
               thrust::device_vector<int> processed_dims, int curr_dim_r,
               int curr_dim_c, int curr_dim_f, T *ddist_f, T *dratio_f, T *dv1,
               int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
               int lddw1, int lddw2, int queue_idx, int config) {

#define LPK(R, C, F)                                                           \
  {                                                                            \
    lpk_reo_1_adaptive_launcher<D, T, R, C, F>(                                \
        handle, shape, shape_c, ldvs, ldws, processed_dims, curr_dim_r,        \
        curr_dim_c, curr_dim_f, ddist_f, dratio_f, dv1, lddv11, lddv12, dv2,   \
        lddv21, lddv22, dw, lddw1, lddw2, queue_idx);                          \
  }

  bool profile = false;
#ifdef MGARD_CUDA_KERNEL_PROFILE
  profile = true;
#endif
  if (D >= 3) {
    if (profile || config == 6) {
      LPK(2, 2, 128)
    }
    if (profile || config == 5) {
      LPK(2, 2, 64)
    }
    if (profile || config == 4) {
      LPK(2, 2, 32)
    }
    if (profile || config == 3) {
      LPK(4, 4, 16)
    }
    if (profile || config == 2) {
      LPK(8, 8, 8)
    }
    if (profile || config == 1) {
      LPK(4, 4, 4)
    }
    if (profile || config == 0) {
      LPK(2, 2, 2)
    }
  } else if (D == 2) {
    if (profile || config == 6) {
      LPK(1, 2, 128)
    }
    if (profile || config == 5) {
      LPK(1, 2, 64)
    }
    if (profile || config == 4) {
      LPK(1, 2, 32)
    }
    if (profile || config == 3) {
      LPK(1, 4, 16)
    }
    if (profile || config == 2) {
      LPK(1, 8, 8)
    }
    if (profile || config == 1) {
      LPK(1, 4, 4)
    }
    if (profile || config == 0) {
      LPK(1, 2, 4)
    }
  } else if (D == 1) {
    if (profile || config == 6) {
      LPK(1, 1, 128)
    }
    if (profile || config == 5) {
      LPK(1, 1, 64)
    }
    if (profile || config == 4) {
      LPK(1, 1, 32)
    }
    if (profile || config == 3) {
      LPK(1, 1, 16)
    }
    if (profile || config == 2) {
      LPK(1, 1, 8)
    }
    if (profile || config == 1) {
      LPK(1, 1, 8)
    }
    if (profile || config == 0) {
      LPK(1, 1, 8)
    }
  }

#undef LPK
}

template <uint32_t D, typename T, int R, int C, int F>
void lpk_reo_1_adaptive_launcher(Handle<D, T> &handle, int *shape_h,
                                 int *shape_c_h, int *shape_d, int *shape_c_d,
                                 int *ldvs, int *ldws, int processed_n,
                                 int *processed_dims_h, int *processed_dims_d,
                                 int curr_dim_r, int curr_dim_c, int curr_dim_f,
                                 T *ddist_f, T *dratio_f, T *dv1, int lddv11,
                                 int lddv12, T *dv2, int lddv21, int lddv22,
                                 T *dw, int lddw1, int lddw2, int queue_idx) {
  int nr = shape_h[curr_dim_r];
  int nc = shape_h[curr_dim_c];
  int nf = shape_h[curr_dim_f];
  int nf_c = shape_c_h[curr_dim_f];

  int total_thread_z = nr;
  int total_thread_y = nc;
  int total_thread_x = nf_c;
  // if (nf_c % 2 == 1) { total_thread_x = nf_c - 1; }
  // else { total_thread_x = nf_c; }
  int tbx, tby, tbz, gridx, gridy, gridz;
  dim3 threadsPerBlock, blockPerGrid;
  size_t sm_size;

  tbz = std::min(R, total_thread_z);
  tby = std::min(C, total_thread_y);
  tbx = std::min(F, total_thread_x);
  sm_size = (R * C * (F * 2 + 3) + (F * 2 + 3) * 2) * sizeof(T);
  sm_size += (D * 5) * sizeof(int);
  gridz = ceil((float)total_thread_z / tbz);
  gridy = ceil((float)total_thread_y / tby);
  gridx = ceil((float)total_thread_x / tbx);
  for (int d = 0; d < D; d++) {
    if (d != curr_dim_f && d != curr_dim_c && d != curr_dim_r) {
      int t = shape_h[d];
      for (int k = 0; k < processed_n; k++) {
        if (d == processed_dims_h[k]) {
          t = shape_c_h[d];
        }
      }
      gridx *= t;
    }
  }
  threadsPerBlock = dim3(tbx, tby, tbz);
  blockPerGrid = dim3(gridx, gridy, gridz);

  // printf("lpk_reo_1 exec config (%d %d %d) (%d %d %d)\n", tbx, tby, tbz,
  // gridx, gridy, gridz);
  _lpk_reo_1<D, T, R, C, F><<<blockPerGrid, threadsPerBlock, sm_size,
                              *(cudaStream_t *)handle.get(queue_idx)>>>(
      shape_d, shape_c_d, ldvs, ldws, processed_n, processed_dims_d, curr_dim_r,
      curr_dim_c, curr_dim_f, ddist_f, dratio_f, dv1, lddv11, lddv12, dv2,
      lddv21, lddv22, dw, lddw1, lddw2);
  gpuErrchk(cudaGetLastError());
#ifdef MGARD_CUDA_DEBUG
  gpuErrchk(cudaDeviceSynchronize());
#endif
}

template <uint32_t D, typename T>
void lpk_reo_1(Handle<D, T> &handle, int *shape_h, int *shape_c_h, int *shape_d,
               int *shape_c_d, int *ldvs, int *ldws, int processed_n,
               int *processed_dims_h, int *processed_dims_d, int curr_dim_r,
               int curr_dim_c, int curr_dim_f, T *ddist_f, T *dratio_f, T *dv1,
               int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
               int lddw1, int lddw2, int queue_idx, int config) {

#define LPK(R, C, F)                                                           \
  {                                                                            \
    lpk_reo_1_adaptive_launcher<D, T, R, C, F>(                                \
        handle, shape_h, shape_c_h, shape_d, shape_c_d, ldvs, ldws,            \
        processed_n, processed_dims_h, processed_dims_d, curr_dim_r,           \
        curr_dim_c, curr_dim_f, ddist_f, dratio_f, dv1, lddv11, lddv12, dv2,   \
        lddv21, lddv22, dw, lddw1, lddw2, queue_idx);                          \
  }

  bool profile = false;
#ifdef MGARD_CUDA_KERNEL_PROFILE
  profile = true;
#endif
  if (D >= 3) {
    if (profile || config == 6) {
      LPK(2, 2, 128)
    }
    if (profile || config == 5) {
      LPK(2, 2, 64)
    }
    if (profile || config == 4) {
      LPK(2, 2, 32)
    }
    if (profile || config == 3) {
      LPK(4, 4, 16)
    }
    if (profile || config == 2) {
      LPK(8, 8, 8)
    }
    if (profile || config == 1) {
      LPK(4, 4, 4)
    }
    if (profile || config == 0) {
      LPK(2, 2, 2)
    }
  } else if (D == 2) {
    if (profile || config == 6) {
      LPK(1, 2, 128)
    }
    if (profile || config == 5) {
      LPK(1, 2, 64)
    }
    if (profile || config == 4) {
      LPK(1, 2, 32)
    }
    if (profile || config == 3) {
      LPK(1, 4, 16)
    }
    if (profile || config == 2) {
      LPK(1, 8, 8)
    }
    if (profile || config == 1) {
      LPK(1, 4, 4)
    }
    if (profile || config == 0) {
      LPK(1, 2, 4)
    }
  } else if (D == 1) {
    if (profile || config == 6) {
      LPK(1, 1, 128)
    }
    if (profile || config == 5) {
      LPK(1, 1, 64)
    }
    if (profile || config == 4) {
      LPK(1, 1, 32)
    }
    if (profile || config == 3) {
      LPK(1, 1, 16)
    }
    if (profile || config == 2) {
      LPK(1, 1, 8)
    }
    if (profile || config == 1) {
      LPK(1, 1, 8)
    }
    if (profile || config == 0) {
      LPK(1, 1, 8)
    }
  }

#undef LPK
}

template <uint32_t D, typename T, int R, int C, int F>
__global__ void
_lpk_reo_2(int *shape, int *shape_c, int *ldvs, int *ldws, int processed_n,
           int *processed_dims, int curr_dim_r, int curr_dim_c, int curr_dim_f,
           T *ddist_c, T *dratio_c, T *dv1, int lddv11, int lddv12, T *dv2,
           int lddv21, int lddv22, T *dw, int lddw1, int lddw2) {

  // bool debug = false;
  // if (blockIdx.z == 0 && blockIdx.y == 0 && blockIdx.x == 0 &&
  // threadIdx.z == 0 && threadIdx.x == 0 ) debug = false;

  // bool debug2 = false;
  // if (threadIdx.z == 0 && threadIdx.y == 0 && threadIdx.x == 0 ) debug2 =
  // false;

  size_t threadId = (threadIdx.z * (blockDim.x * blockDim.y)) +
                    (threadIdx.y * blockDim.x) + threadIdx.x;

  T *sm = SharedMemory<T>();

  // extern __shared__ double sm[]; // size: (blockDim.x + 1) * (blockDim.y + 1)
  // * (blockDim.z + 1)
  int ldsm1 = F;
  int ldsm2 = C * 2 + 3;
  T *v_sm = sm;
  T *dist_c_sm = sm + ldsm1 * ldsm2 * R;
  T *ratio_c_sm = dist_c_sm + ldsm2;
  int *shape_sm = (int *)(ratio_c_sm + ldsm2);
  int *shape_c_sm = shape_sm + D;
  int *processed_dims_sm = shape_c_sm + D;
  int *ldvs_sm = processed_dims_sm + D;
  int *ldws_sm = ldvs_sm + D;
  int idx[D];
  if (threadId < D) {
    shape_sm[threadId] = shape[threadId];
    shape_c_sm[threadId] = shape_c[threadId];
    ldvs_sm[threadId] = ldvs[threadId];
    ldws_sm[threadId] = ldws[threadId];
  }
  if (threadId < processed_n) {
    processed_dims_sm[threadId] = processed_dims[threadId];
  }
  __syncthreads();

  for (int d = 0; d < D; d++)
    idx[d] = 0;

  int nr = shape_sm[curr_dim_r];
  int nc = shape_sm[curr_dim_c];
  int nf_c = shape_c_sm[curr_dim_f];
  int nc_c = shape_c_sm[curr_dim_c];
  bool PADDING = (nc % 2 == 0);

  if (D < 3) {
    nr = 1;
  }

  int bidx = blockIdx.x;
  int firstD = div_roundup(nf_c, blockDim.x);
  int blockId_f = bidx % firstD;
  bidx /= firstD;

  for (int d = 0; d < D; d++) {
    if (d != curr_dim_r && d != curr_dim_c && d != curr_dim_f) {
      int t = shape_sm[d];
      for (int k = 0; k < processed_n; k++) {
        if (d == processed_dims[k]) {
          t = shape_c_sm[d];
        }
      }
      idx[d] = bidx % t;
      bidx /= t;
    }
  }

  size_t other_offset_v = get_idx<D>(ldvs_sm, idx);
  size_t other_offset_w = get_idx<D>(ldws_sm, idx);

  dv1 = dv1 + other_offset_v;
  dv2 = dv2 + other_offset_v;
  dw = dw + other_offset_w;

  // if (debug2) {
  //   printf("idx: %d %d %d %d\n", idx[3], idx[2], idx[1], idx[0]);
  //   printf("other_offset_v: %llu\n", other_offset_v);
  //   printf("other_offset_w: %llu\n", other_offset_w);
  // }

  register int r_gl = blockIdx.z * blockDim.z + threadIdx.z;
  register int c_gl = blockIdx.y * blockDim.y + threadIdx.y;
  register int f_gl = blockId_f * blockDim.x + threadIdx.x;

  register int blockId = blockIdx.y;

  register int r_sm = threadIdx.z;
  register int c_sm = threadIdx.y;
  register int f_sm = threadIdx.x;

  int actual_C = C;
  if (nc_c - blockIdx.y * blockDim.y < C) {
    actual_C = nc_c - blockIdx.y * blockDim.y;
  }

  // if (nc_c % 2 == 1){
  //   if(nc_c-1 - blockIdx.y * blockDim.y < C) { actual_C = nc_c - 1 -
  //   blockIdx.y * blockDim.y; }
  // } else {
  //   if(nc_c - blockIdx.y * blockDim.y < C) { actual_C = nc_c - blockIdx.y *
  //   blockDim.y; }
  // }

  // if (debug) printf("actual_C %d\n", actual_C);

  if (r_gl < nr && c_gl < nc_c && f_gl < nf_c) {
    // if (debug) printf("load up vsm[%d]: %f <- %d %d %d\n", c_sm * 2 + 2,
    // dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
    v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 2, f_sm)] =
        dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)];

    if (c_sm == actual_C - 1) {
      if (c_gl + 1 < nc_c) {
        // if (debug) printf("load up+1 vsm[%d]: %f <- %d %d %d\n", actual_C * 2
        // + 2, dv1[get_idx(lddv11, lddv12, r_gl, blockId * C + actual_C,
        // f_gl)], r_gl, blockId * C + actual_C, f_gl);
        // c_gl+1 == blockId * C + C
        v_sm[get_idx(ldsm1, ldsm2, r_sm, actual_C * 2 + 2, f_sm)] =
            dv1[get_idx(lddv11, lddv12, r_gl, c_gl + 1, f_gl)];
      } else {
        // if (debug) printf("load up+1 vsm[%d]: 0.0\n", actual_C * 2 + 2);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, actual_C * 2 + 2, f_sm)] = 0.0;
      }
    }

    if (c_sm == 0) {
      if (c_gl - 1 >= 0) {
        // if (debug) printf("load up-1 vsm[0]: %f <- %d %d %d\n",
        // dv1[get_idx(lddv11, lddv12, r_gl, c_gl-1, f_gl)], r_gl, c_gl-1,
        // f_gl);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, 0, f_sm)] =
            dv1[get_idx(lddv11, lddv12, r_gl, c_gl - 1, f_gl)];
      } else {
        // if (debug) printf("load up-1 vsm[0]: 0.0\n");
        v_sm[get_idx(ldsm1, ldsm2, r_sm, 0, f_sm)] = 0.0;
      }
    }

    if (!PADDING) {
      if (c_gl < nc_c - 1) {
        // if (debug) printf("load down vsm[%d]: %f <- %d %d %d\n", c_sm * 2 +
        // 3, dv2[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 3, f_sm)] =
            dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
      } else {
        // if (debug) printf("load down vsm[%d]: 0.0\n", c_sm * 2 + 3);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 3, f_sm)] = 0.0;
      }
    } else {
      if (c_gl < nc_c - 2) {
        // if (debug) printf("load down vsm[%d]: %f <- %d %d %d\n", c_sm * 2 +
        // 3, dv2[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 3, f_sm)] =
            dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
      } else {
        // if (debug) printf("load down vsm[%d]: 0.0\n", c_sm * 2 + 3);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 3, f_sm)] = 0.0;
      }
    }

    if (c_gl - 1 >= 0 &&
        (PADDING && c_gl - 1 < nc_c - 2 || !PADDING && c_gl - 1 < nc_c - 1)) {
      if (c_sm == 0) {
        // if (debug) printf("load down-1 vsm[1]: %f <- %d %d %d\n",
        // dv2[get_idx(lddv11, lddv12, r_gl, c_gl-1, f_gl)], r_gl, c_gl-1,
        // f_gl);
        v_sm[get_idx(ldsm1, ldsm2, r_sm, 1, f_sm)] =
            dv2[get_idx(lddv11, lddv12, r_gl, c_gl - 1, f_gl)];
      }
    } else {
      if (c_sm == 0) {
        // if (debug) printf("load down-1 vsm[1]: 0.0\n");
        v_sm[get_idx(ldsm1, ldsm2, r_sm, 1, f_sm)] = 0.0;
      }
    }
  }

  // load dist/ratio using f_sm for better performance
  // assumption F >= C
  if (r_sm == 0 && c_sm == 0 && f_sm < actual_C) {
    if (blockId * C * 2 + f_sm < nc - 1) {
      dist_c_sm[2 + f_sm] = ddist_c[blockId * C * 2 + f_sm];
      ratio_c_sm[2 + f_sm] = dratio_c[blockId * C * 2 + f_sm];
    } else {
      dist_c_sm[2 + f_sm] = 0.0;
      ratio_c_sm[2 + f_sm] = 0.0;
    }

    if (blockId * C * 2 + actual_C + f_sm < nc - 1) {
      dist_c_sm[2 + actual_C + f_sm] =
          ddist_c[blockId * C * 2 + actual_C + f_sm];
      ratio_c_sm[2 + actual_C + f_sm] =
          dratio_c[blockId * C * 2 + actual_C + f_sm];
    } else {
      dist_c_sm[2 + actual_C + f_sm] = 0.0;
      ratio_c_sm[2 + actual_C + f_sm] = 0.0;
    }
  }

  if (blockId > 0) {
    if (f_sm < 2) {
      dist_c_sm[f_sm] = ddist_c[blockId * C * 2 - 2 + f_sm];
      ratio_c_sm[f_sm] = dratio_c[blockId * C * 2 - 2 + f_sm];
    }
  } else {
    if (f_sm < 2) {
      dist_c_sm[f_sm] = 0.0;
      ratio_c_sm[f_sm] = 0.0;
    }
  }

  __syncthreads();

  if (r_gl < nr && c_gl < nc_c && f_gl < nf_c) {
    T h1 = dist_c_sm[c_sm * 2];
    T h2 = dist_c_sm[c_sm * 2 + 1];
    T h3 = dist_c_sm[c_sm * 2 + 2];
    T h4 = dist_c_sm[c_sm * 2 + 3];
    T r1 = ratio_c_sm[c_sm * 2];
    T r2 = ratio_c_sm[c_sm * 2 + 1];
    T r3 = ratio_c_sm[c_sm * 2 + 2];
    T r4 = 1 - r3;
    T a = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2, f_sm)];
    T b = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 1, f_sm)];
    T c = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 2, f_sm)];
    T d = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 3, f_sm)];
    T e = v_sm[get_idx(ldsm1, ldsm2, r_sm, c_sm * 2 + 4, f_sm)];

    // if (debug) {
    //   printf("c_sm(%d) %f %f %f %f %f\n",c_sm, a,b,c,d,e);
    //   printf("c_sm_h(%d) %f %f %f %f\n",c_sm, h1,h2,h3,h4);
    //   printf("c_sm_r(%d) %f %f %f %f\n",c_sm, r1,r2,r3,r4);
    // }

    // T tb = a * h1 + b * 2 * (h1+h2) + c * h2;
    // T tc = b * h2 + c * 2 * (h2+h3) + d * h3;
    // T td = c * h3 + d * 2 * (h3+h4) + e * h4;

    // if (debug) printf("c_sm(%d) tb tc td tc: %f %f %f %f\n", f_sm, tb, tc,
    // td, tc+tb * r1 + td * r4);

    // tc += tb * r1 + td * r4;

    // if (r_gl == 0 && f_gl == 0 && r_sm == 0 && f_sm == 0) {
    //   printf("mr2(%d) mm2: %f -> (%d %d %d)\n", c_sm, tc, r_gl, c_gl, f_gl);
    //   // printf("f_sm(%d) b c d: %f %f %f\n", f_sm, tb, tc, td);
    // }

    dw[get_idx(lddw1, lddw2, r_gl, c_gl, f_gl)] =
        mass_trans(a, b, c, d, e, h1, h2, h3, h4, r1, r2, r3, r4);

    // if (debug) printf("store[%d %d %d] %f \n", r_gl, c_gl, f_gl,
    //           mass_trans(a, b, c, d, e, h1, h2, h3, h4, r1, r2, r3, r4));

    // printf("%d %d %d\n", r_gl, c_gl, f_gl);
    // if (blockId * C + C == nc-1) {
    // if (c_gl + 1 == nc_c - 1) {
    //   // T te = h4 * d + 2 * h4 * e;
    //   // te += td * r3;
    //   dw[get_idx(lddw1, lddw2, r_gl, blockId * C + actual_C, f_gl)] =
    //     mass_trans(c, d, e, (T)0.0, (T)0.0,
    //       h1, h2, (T)0.0, (T)0.0, r1, r2, (T)0.0, (T)0.0);
    // }
    // }
  }
}

template <uint32_t D, typename T, int R, int C, int F>
void lpk_reo_2_adaptive_launcher(
    Handle<D, T> &handle, thrust::device_vector<int> shape,
    thrust::device_vector<int> shape_c, thrust::device_vector<int> ldvs,
    thrust::device_vector<int> ldws, thrust::device_vector<int> processed_dims,
    int curr_dim_r, int curr_dim_c, int curr_dim_f, T *ddist_c, T *dratio_c,
    T *dv1, int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
    int lddw1, int lddw2, int queue_idx) {

  int nr = shape[curr_dim_r];
  int nc = shape[curr_dim_c];
  int nf = shape[curr_dim_f];
  int nc_c = shape_c[curr_dim_c];
  int nf_c = shape_c[curr_dim_f];

  int total_thread_z = nr;
  int total_thread_y = nc_c;
  // if (nc_c % 2 == 1) { total_thread_y = nc_c - 1; }
  // else { total_thread_y = nc_c; }
  int total_thread_x = nf_c;
  int tbx, tby, tbz, gridx, gridy, gridz;
  dim3 threadsPerBlock, blockPerGrid;
  size_t sm_size;

  tbz = std::min(R, total_thread_z);
  tby = std::min(C, total_thread_y);
  tbx = std::min(F, total_thread_x);
  sm_size = (R * (C * 2 + 3) * F + (C * 2 + 3) * 2) * sizeof(T);
  sm_size += (D * 5) * sizeof(int);
  gridz = ceil((float)total_thread_z / tbz);
  gridy = ceil((float)total_thread_y / tby);
  gridx = ceil((float)total_thread_x / tbx);
  for (int d = 0; d < D; d++) {
    if (d != curr_dim_f && d != curr_dim_c && d != curr_dim_r) {
      int t = shape[d];
      for (int k = 0; k < processed_dims.size(); k++) {
        if (d == processed_dims[k]) {
          t = shape_c[d];
        }
      }
      gridx *= t;
    }
  }
  threadsPerBlock = dim3(tbx, tby, tbz);
  blockPerGrid = dim3(gridx, gridy, gridz);
  _lpk_reo_2<D, T, R, C, F><<<blockPerGrid, threadsPerBlock, sm_size,
                              *(cudaStream_t *)handle.get(queue_idx)>>>(
      thrust::raw_pointer_cast(shape.data()),
      thrust::raw_pointer_cast(shape_c.data()),
      thrust::raw_pointer_cast(ldvs.data()),
      thrust::raw_pointer_cast(ldws.data()), processed_dims.size(),
      thrust::raw_pointer_cast(processed_dims.data()), curr_dim_r, curr_dim_c,
      curr_dim_f, ddist_c, dratio_c, dv1, lddv11, lddv12, dv2, lddv21, lddv22,
      dw, lddw1, lddw2);
  gpuErrchk(cudaGetLastError());
#ifdef MGARD_CUDA_DEBUG
  gpuErrchk(cudaDeviceSynchronize());
#endif
}

template <uint32_t D, typename T>
void lpk_reo_2(Handle<D, T> &handle, thrust::device_vector<int> shape,
               thrust::device_vector<int> shape_c,
               thrust::device_vector<int> ldvs, thrust::device_vector<int> ldws,
               thrust::device_vector<int> processed_dims, int curr_dim_r,
               int curr_dim_c, int curr_dim_f, T *ddist_c, T *dratio_c, T *dv1,
               int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
               int lddw1, int lddw2, int queue_idx, int config) {

#define LPK(R, C, F)                                                           \
  {                                                                            \
    lpk_reo_2_adaptive_launcher<D, T, R, C, F>(                                \
        handle, shape, shape_c, ldvs, ldws, processed_dims, curr_dim_r,        \
        curr_dim_c, curr_dim_f, ddist_c, dratio_c, dv1, lddv11, lddv12, dv2,   \
        lddv21, lddv22, dw, lddw1, lddw2, queue_idx);                          \
  }

  bool profile = false;
#ifdef MGARD_CUDA_KERNEL_PROFILE
  profile = true;
#endif
  if (D >= 3) {
    if (profile || config == 6) {
      LPK(2, 2, 128)
    }
    if (profile || config == 5) {
      LPK(2, 2, 64)
    }
    if (profile || config == 4) {
      LPK(2, 2, 32)
    }
    if (profile || config == 3) {
      LPK(4, 4, 16)
    }
    if (profile || config == 2) {
      LPK(8, 8, 8)
    }
    if (profile || config == 1) {
      LPK(4, 4, 4)
    }
    if (profile || config == 0) {
      LPK(2, 2, 2)
    }
  } else if (D == 2) {
    if (profile || config == 6) {
      LPK(1, 2, 128)
    }
    if (profile || config == 5) {
      LPK(1, 2, 64)
    }
    if (profile || config == 4) {
      LPK(1, 2, 32)
    }
    if (profile || config == 3) {
      LPK(1, 4, 16)
    }
    if (profile || config == 2) {
      LPK(1, 8, 8)
    }
    if (profile || config == 1) {
      LPK(1, 4, 4)
    }
    if (profile || config == 0) {
      LPK(1, 2, 4)
    }
  } else {
    printf("Error: mass_trans_multiply_2_cpt is only for 3D and 2D data\n");
  }
#undef LPK
}

template <uint32_t D, typename T, int R, int C, int F>
void lpk_reo_2_adaptive_launcher(Handle<D, T> &handle, int *shape_h,
                                 int *shape_c_h, int *shape_d, int *shape_c_d,
                                 int *ldvs, int *ldws, int processed_n,
                                 int *processed_dims_h, int *processed_dims_d,
                                 int curr_dim_r, int curr_dim_c, int curr_dim_f,
                                 T *ddist_c, T *dratio_c, T *dv1, int lddv11,
                                 int lddv12, T *dv2, int lddv21, int lddv22,
                                 T *dw, int lddw1, int lddw2, int queue_idx) {

  int nr = shape_h[curr_dim_r];
  int nc = shape_h[curr_dim_c];
  int nf = shape_h[curr_dim_f];
  int nc_c = shape_c_h[curr_dim_c];
  int nf_c = shape_c_h[curr_dim_f];

  int total_thread_z = nr;
  int total_thread_y = nc_c;
  // if (nc_c % 2 == 1) { total_thread_y = nc_c - 1; }
  // else { total_thread_y = nc_c; }
  int total_thread_x = nf_c;
  int tbx, tby, tbz, gridx, gridy, gridz;
  dim3 threadsPerBlock, blockPerGrid;
  size_t sm_size;

  tbz = std::min(R, total_thread_z);
  tby = std::min(C, total_thread_y);
  tbx = std::min(F, total_thread_x);
  sm_size = (R * (C * 2 + 3) * F + (C * 2 + 3) * 2) * sizeof(T);
  sm_size += (D * 5) * sizeof(int);
  gridz = ceil((float)total_thread_z / tbz);
  gridy = ceil((float)total_thread_y / tby);
  gridx = ceil((float)total_thread_x / tbx);
  for (int d = 0; d < D; d++) {
    if (d != curr_dim_f && d != curr_dim_c && d != curr_dim_r) {
      int t = shape_h[d];
      for (int k = 0; k < processed_n; k++) {
        if (d == processed_dims_h[k]) {
          t = shape_c_h[d];
        }
      }
      gridx *= t;
    }
  }
  threadsPerBlock = dim3(tbx, tby, tbz);
  blockPerGrid = dim3(gridx, gridy, gridz);
  // printf("lpk_reo_2 exec config (%d %d %d) (%d %d %d)\n", tbx, tby, tbz,
  // gridx, gridy, gridz);

  _lpk_reo_2<D, T, R, C, F><<<blockPerGrid, threadsPerBlock, sm_size,
                              *(cudaStream_t *)handle.get(queue_idx)>>>(
      shape_d, shape_c_d, ldvs, ldws, processed_n, processed_dims_d, curr_dim_r,
      curr_dim_c, curr_dim_f, ddist_c, dratio_c, dv1, lddv11, lddv12, dv2,
      lddv21, lddv22, dw, lddw1, lddw2);
  gpuErrchk(cudaGetLastError());
#ifdef MGARD_CUDA_DEBUG
  gpuErrchk(cudaDeviceSynchronize());
#endif
}

template <uint32_t D, typename T>
void lpk_reo_2(Handle<D, T> &handle, int *shape_h, int *shape_c_h, int *shape_d,
               int *shape_c_d, int *ldvs, int *ldws, int processed_n,
               int *processed_dims_h, int *processed_dims_d, int curr_dim_r,
               int curr_dim_c, int curr_dim_f, T *ddist_c, T *dratio_c, T *dv1,
               int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
               int lddw1, int lddw2, int queue_idx, int config) {

#define LPK(R, C, F)                                                           \
  {                                                                            \
    lpk_reo_2_adaptive_launcher<D, T, R, C, F>(                                \
        handle, shape_h, shape_c_h, shape_d, shape_c_d, ldvs, ldws,            \
        processed_n, processed_dims_h, processed_dims_d,\ 
                               curr_dim_r,                                     \
        curr_dim_c, curr_dim_f, ddist_c, dratio_c, dv1, lddv11, lddv12, dv2,   \
        lddv21, lddv22, dw, lddw1, lddw2, queue_idx);                          \
  }

  bool profile = false;
#ifdef MGARD_CUDA_KERNEL_PROFILE
  profile = true;
#endif
  if (D >= 3) {
    if (profile || config == 6) {
      LPK(2, 2, 128)
    }
    if (profile || config == 5) {
      LPK(2, 2, 64)
    }
    if (profile || config == 4) {
      LPK(2, 2, 32)
    }
    if (profile || config == 3) {
      LPK(4, 4, 16)
    }
    if (profile || config == 2) {
      LPK(8, 8, 8)
    }
    if (profile || config == 1) {
      LPK(4, 4, 4)
    }
    if (profile || config == 0) {
      LPK(2, 2, 2)
    }
  } else if (D == 2) {
    if (profile || config == 6) {
      LPK(1, 2, 128)
    }
    if (profile || config == 5) {
      LPK(1, 2, 64)
    }
    if (profile || config == 4) {
      LPK(1, 2, 32)
    }
    if (profile || config == 3) {
      LPK(1, 4, 16)
    }
    if (profile || config == 2) {
      LPK(1, 8, 8)
    }
    if (profile || config == 1) {
      LPK(1, 4, 4)
    }
    if (profile || config == 0) {
      LPK(1, 2, 4)
    }
  } else {
    printf("Error: mass_trans_multiply_2_cpt is only for 3D and 2D data\n");
  }
#undef LPK
}

template <uint32_t D, typename T, int R, int C, int F>
__global__ void
_lpk_reo_3(int *shape, int *shape_c, int *ldvs, int *ldws, int processed_n,
           int *processed_dims, int curr_dim_r, int curr_dim_c, int curr_dim_f,
           T *ddist_r, T *dratio_r, T *dv1, int lddv11, int lddv12, T *dv2,
           int lddv21, int lddv22, T *dw, int lddw1, int lddw2) {

  // bool debug = false;
  // if (blockIdx.z == gridDim.z-1 && blockIdx.y == 0 && blockIdx.x == 0 &&
  // threadIdx.y == 0 && threadIdx.x == 0 ) debug = false;

  // bool debug2 = false;
  // if (blockIdx.z == gridDim.z-1 && blockIdx.y == 1 && blockIdx.x == 16)
  // debug2 = false;

  size_t threadId = (threadIdx.z * (blockDim.x * blockDim.y)) +
                    (threadIdx.y * blockDim.x) + threadIdx.x;

  T *sm = SharedMemory<T>();
  int ldsm1 = F;
  int ldsm2 = C;
  T *v_sm = sm;
  T *dist_r_sm = sm + ldsm1 * ldsm2 * (R * 2 + 3);
  T *ratio_r_sm = dist_r_sm + (R * 2 + 3);
  int *shape_sm = (int *)(ratio_r_sm + R * 2 + 3);
  int *shape_c_sm = shape_sm + D;
  int *processed_dims_sm = shape_c_sm + D;
  int *ldvs_sm = processed_dims_sm + D;
  int *ldws_sm = ldvs_sm + D;
  int idx[D];
  if (threadId < D) {
    shape_sm[threadId] = shape[threadId];
    shape_c_sm[threadId] = shape_c[threadId];
    ldvs_sm[threadId] = ldvs[threadId];
    ldws_sm[threadId] = ldws[threadId];
  }
  if (threadId < processed_n) {
    processed_dims_sm[threadId] = processed_dims[threadId];
  }
  __syncthreads();

  for (int d = 0; d < D; d++)
    idx[d] = 0;

  int nr = shape_sm[curr_dim_r];
  int nf_c = shape_c_sm[curr_dim_f];
  int nc_c = shape_c_sm[curr_dim_c];
  int nr_c = shape_c_sm[curr_dim_r];
  bool PADDING = (nr % 2 == 0);

  int bidx = blockIdx.x;
  int firstD = div_roundup(nf_c, blockDim.x);
  int blockId_f = bidx % firstD;
  bidx /= firstD;

  for (int d = 0; d < D; d++) {
    if (d != curr_dim_r && d != curr_dim_c && d != curr_dim_f) {
      int t = shape_sm[d];
      for (int k = 0; k < processed_n; k++) {
        if (d == processed_dims[k]) {
          t = shape_c_sm[d];
        }
      }
      idx[d] = bidx % t;
      bidx /= t;
    }
  }

  size_t other_offset_v = get_idx<D>(ldvs_sm, idx);
  size_t other_offset_w = get_idx<D>(ldws_sm, idx);

  dv1 = dv1 + other_offset_v;
  dv2 = dv2 + other_offset_v;
  dw = dw + other_offset_w;

  // if (debug2) {
  //   printf("idx: %d %d %d %d\n", idx[3], idx[2], idx[1], idx[0]);
  //   printf("other_offset_v: %llu\n", other_offset_v);
  //   printf("other_offset_w: %llu\n", other_offset_w);
  // }

  register int r_gl = blockIdx.z * blockDim.z + threadIdx.z;
  register int c_gl = blockIdx.y * blockDim.y + threadIdx.y;
  register int f_gl = blockId_f * blockDim.x + threadIdx.x;

  register int blockId = blockIdx.z;

  register int r_sm = threadIdx.z;
  register int c_sm = threadIdx.y;
  register int f_sm = threadIdx.x;

  int actual_R = R;
  if (nr_c - blockIdx.z * blockDim.z < R) {
    actual_R = nr_c - blockIdx.z * blockDim.z;
  }
  // if (nr_c % 2 == 1){
  //   if(nr_c-1 - blockIdx.z * blockDim.z < R) { actual_R = nr_c - 1 -
  //   blockIdx.z * blockDim.z; }
  // } else {
  //   if(nr_c - blockIdx.z * blockDim.z < R) { actual_R = nr_c - blockIdx.z *
  //   blockDim.z; }
  // }

  // if (debug) printf("actual_R %d\n", actual_R);

  // if (debug) printf("RCF: %d %d %d\n", R, C, F);
  if (r_gl < nr_c && c_gl < nc_c && f_gl < nf_c) {
    // if (debug) printf("load front vsm[%d]: %f <- %d %d %d\n", r_sm * 2 + 2,
    // dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
    v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 2, c_sm, f_sm)] =
        dv1[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)];

    if (r_sm == actual_R - 1) {
      if (r_gl + 1 < nr_c) {
        // if (debug) printf("load front+1 vsm[%d]: %f <- %d %d %d\n", actual_R
        // * 2 + 2, dv1[get_idx(lddv11, lddv12, blockId * R + actual_R, c_gl,
        // f_gl)], blockId * R + actual_R, c_gl, f_gl);
        v_sm[get_idx(ldsm1, ldsm2, actual_R * 2 + 2, c_sm, f_sm)] =
            dv1[get_idx(lddv11, lddv12, r_gl + 1, c_gl, f_gl)];
      } else {
        // if (debug) printf("load front+1 vsm[%d]: 0.0\n", actual_R * 2 + 2);
        v_sm[get_idx(ldsm1, ldsm2, actual_R * 2 + 2, c_sm, f_sm)] = 0.0;
      }
    }

    if (r_sm == 0) {
      if (r_gl - 1 >= 0) {
        // if (debug) printf("load front-1 vsm[0]: %f <- %d %d %d\n",
        // dv1[get_idx(lddv11, lddv12, r_gl-1, c_gl, f_gl)], r_gl-1, c_gl,
        // f_gl);
        v_sm[get_idx(ldsm1, ldsm2, 0, c_sm, f_sm)] =
            dv1[get_idx(lddv11, lddv12, r_gl - 1, c_gl, f_gl)];
      } else {
        // if (debug) printf("load front-1 vsm[0]: 0.0\n");
        v_sm[get_idx(ldsm1, ldsm2, 0, c_sm, f_sm)] = 0.0;
      }
    }

    if (!PADDING) {
      if (r_gl < nr_c - 1) {
        // if (debug) printf("load back vsm[%d]: 0.0\n", r_sm * 2 + 3);
        v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 3, c_sm, f_sm)] =
            dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
      } else {
        // if (debug) printf("load back vsm[%d]: %f <- %d %d %d\n", r_sm * 2 +
        // 3, dv2[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
        v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 3, c_sm, f_sm)] = 0.0;
      }
    } else {
      if (r_gl < nr_c - 2) {
        // if (debug) printf("load back vsm[%d]: %f <- %d %d %d\n", r_sm * 2 +
        // 3, dv2[get_idx(lddv11, lddv12, r_gl, c_gl, f_gl)], r_gl, c_gl, f_gl);
        v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 3, c_sm, f_sm)] =
            dv2[get_idx(lddv21, lddv22, r_gl, c_gl, f_gl)];
      } else {
        // if (debug) printf("load back vsm[%d]: 0.0\n", r_sm * 2 + 3);
        v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 3, c_sm, f_sm)] = 0.0;
      }
    }

    if (r_gl - 1 >= 0 &&
        (PADDING && r_gl - 1 < nr_c - 2 || !PADDING && r_gl - 1 < nr_c - 1)) {
      // if (blockId > 0) {
      if (r_sm == 0) {
        // if (debug) printf("load back-1 vsm[1]: %f <- %d %d %d\n",
        // dv2[get_idx(lddv11, lddv12, r_gl-1, c_gl, f_gl)], r_gl-1, c_gl,
        // f_gl);
        v_sm[get_idx(ldsm1, ldsm2, 1, c_sm, f_sm)] =
            dv2[get_idx(lddv11, lddv12, r_gl - 1, c_gl, f_gl)];
      }
    } else {
      if (r_sm == 0) {
        // if (debug) printf("load back-1 vsm[1]: 0.0\n");
        v_sm[get_idx(ldsm1, ldsm2, 1, c_sm, f_sm)] = 0.0;
      }
    }
  }

  // load dist/ratio using f_sm for better performance
  // assumption F >= R
  if (r_sm == 0 && c_sm == 0 && f_sm < actual_R) {
    if (blockId * R * 2 + f_sm < nr - 1) {
      dist_r_sm[2 + f_sm] = ddist_r[blockId * R * 2 + f_sm];
      // if (debug2 ) printf("load dist 1 [%d]: %f [%d]\n", 2 + f_sm,
      // dist_r_sm[2 + f_sm], blockId * R * 2 + f_sm);
      ratio_r_sm[2 + f_sm] = dratio_r[blockId * R * 2 + f_sm];
      // if (debug2 )printf("load ratio 1 [%d]: %f [%d]\n", 2 + f_sm,
      // ratio_r_sm[2 + f_sm], blockId * R * 2 + f_sm);
    } else {
      dist_r_sm[2 + f_sm] = 0.0;
      ratio_r_sm[2 + f_sm] = 0.0;
    }
    if (blockId * R * 2 + actual_R + f_sm < nr - 2) {
      dist_r_sm[2 + actual_R + f_sm] =
          ddist_r[blockId * R * 2 + actual_R + f_sm];
      // if (debug2 )printf("load dist 2 [%d]: %f [%d]\n", 2 + actual_R + f_sm,
      // dist_r_sm[2 + actual_R + f_sm], blockId * R * 2 + actual_R + f_sm);
      ratio_r_sm[2 + actual_R + f_sm] =
          dratio_r[blockId * R * 2 + actual_R + f_sm];
      // if (debug2 )printf("load ratio 2 [%d]: %f [%d]\n", 2 + actual_R + f_sm,
      // ratio_r_sm[2 + actual_R + f_sm], blockId * R * 2 + actual_R + f_sm);
    } else {
      dist_r_sm[2 + actual_R + f_sm] = 0.0;
      ratio_r_sm[2 + actual_R + f_sm] = 0.0;
    }
  }

  if (blockId > 0) {
    if (f_sm < 2) {
      dist_r_sm[f_sm] = ddist_r[blockId * R * 2 - 2 + f_sm];
      // if (debug2 )printf("load dist -1 [%d]: %f [%d]\n", f_sm,
      // dist_r_sm[f_sm], blockId * R * 2 - 2 + f_sm);
      ratio_r_sm[f_sm] = dratio_r[blockId * R * 2 - 2 + f_sm];
      // if (debug2 )printf("load ratio -1 [%d]: %f [%d]\n", f_sm,
      // ratio_r_sm[f_sm], blockId * R * 2 - 2 + f_sm);
    }
  } else {
    if (f_sm < 2) {
      dist_r_sm[f_sm] = 0.0;
      ratio_r_sm[f_sm] = 0.0;
    }
  }

  __syncthreads();

  int adjusted_nr_c = nr_c;
  if (r_gl < nr_c && c_gl < nc_c && f_gl < nf_c) {
    T h1 = dist_r_sm[r_sm * 2];
    T h2 = dist_r_sm[r_sm * 2 + 1];
    T h3 = dist_r_sm[r_sm * 2 + 2];
    T h4 = dist_r_sm[r_sm * 2 + 3];
    T r1 = ratio_r_sm[r_sm * 2];
    T r2 = ratio_r_sm[r_sm * 2 + 1];
    T r3 = ratio_r_sm[r_sm * 2 + 2];
    T r4 = 1 - r3;
    T a = v_sm[get_idx(ldsm1, ldsm2, r_sm * 2, c_sm, f_sm)];
    T b = v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 1, c_sm, f_sm)];
    T c = v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 2, c_sm, f_sm)];
    T d = v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 3, c_sm, f_sm)];
    T e = v_sm[get_idx(ldsm1, ldsm2, r_sm * 2 + 4, c_sm, f_sm)];

    // __syncthreads();
    // if (debug) {
    //   printf("r_sm(%d) %f %f %f %f %f\n",r_sm, a,b,c,d,e);
    //   printf("r_sm_h(%d) %f %f %f %f\n",r_sm, h1,h2,h3,h4);
    //   printf("r_sm_r(%d) %f %f %f %f\n",r_sm, r1,r2,r3,r4);
    // }
    // __syncthreads();

    // T tb = a * h1 + b * 2 * (h1+h2) + c * h2;
    // T tc = b * h2 + c * 2 * (h2+h3) + d * h3;
    // T td = c * h3 + d * 2 * (h3+h4) + e * h4;

    // if (debug) printf("f_sm(%d) tb tc td tc: %f %f %f %f\n", f_sm, tb, tc,
    // td, tc+tb * r1 + td * r4);

    // tc += tb * r1 + td * r4;

    dw[get_idx(lddw1, lddw2, r_gl, c_gl, f_gl)] =
        mass_trans(a, b, c, d, e, h1, h2, h3, h4, r1, r2, r3, r4);

    // if (debug) printf("store[%d %d %d] %f (%f)\n", r_gl, c_gl, f_gl,
    // mass_trans(a, b, c, d, e, h1, h2, h3, h4, r1, r2, r3, r4),
    //                 mass_trans(a, b, c, (T)0.0, (T)0.0, h1, (T)0.0, (T)0.0,
    //                 h4, r1, r2, (T)0.0, (T)0.0));
    // // printf("%d %d %d\n", r_gl, c_gl, f_gl);
    // if (blockId * R + R == nr-1) {
    // if (r_gl+1 == nr_c - 1) {
    // if (r_gl+1 == nr_c - 1) {
    //   // T te = h4 * d + 2 * h4 * e;
    //   // te += td * r3;
    //   dw[get_idx(lddw1, lddw2, blockId * R + actual_R, c_gl, f_gl)] =
    //     mass_trans(c, d, e, (T)0.0, (T)0.0,
    //       h1, h2, (T)0.0, (T)0.0, r1, r2, (T)0.0, (T)0.0);

    //   if (debug) printf("store-last[%d %d %d] %f\n", blockId * R + actual_R,
    //   c_gl, f_gl,
    //             mass_trans(c, d, e, (T)0.0, (T)0.0,
    //       h1, h2, (T)0.0, (T)0.0, r1, r2, (T)0.0, (T)0.0));
    // }
    //}
  }
}

template <uint32_t D, typename T, int R, int C, int F>
void lpk_reo_3_adaptive_launcher(
    Handle<D, T> &handle, thrust::device_vector<int> shape,
    thrust::device_vector<int> shape_c, thrust::device_vector<int> ldvs,
    thrust::device_vector<int> ldws, thrust::device_vector<int> processed_dims,
    int curr_dim_r, int curr_dim_c, int curr_dim_f, T *ddist_r, T *dratio_r,
    T *dv1, int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
    int lddw1, int lddw2, int queue_idx) {

  int nr = shape[curr_dim_r];
  int nc = shape[curr_dim_c];
  int nf = shape[curr_dim_f];
  int nr_c = shape_c[curr_dim_r];
  int nc_c = shape_c[curr_dim_c];
  int nf_c = shape_c[curr_dim_f];

  int total_thread_z = nr_c;
  // if (nr_c % 2 == 1){ total_thread_z = nr_c - 1; }
  // else { total_thread_z = nr_c; }
  int total_thread_y = nc_c;
  int total_thread_x = nf_c;

  int tbx, tby, tbz, gridx, gridy, gridz;
  dim3 threadsPerBlock, blockPerGrid;
  size_t sm_size;

  tbz = std::min(R, total_thread_z);
  tby = std::min(C, total_thread_y);
  tbx = std::min(F, total_thread_x);
  sm_size = ((R * 2 + 3) * C * F + (R * 2 + 3) * 2) * sizeof(T);
  sm_size += (D * 5) * sizeof(int);
  gridz = ceil((float)total_thread_z / tbz);
  gridy = ceil((float)total_thread_y / tby);
  gridx = ceil((float)total_thread_x / tbx);
  for (int d = 0; d < D; d++) {
    if (d != curr_dim_f && d != curr_dim_c && d != curr_dim_r) {
      int t = shape[d];
      for (int k = 0; k < processed_dims.size(); k++) {
        if (d == processed_dims[k]) {
          t = shape_c[d];
        }
      }
      gridx *= t;
    }
  }
  threadsPerBlock = dim3(tbx, tby, tbz);
  blockPerGrid = dim3(gridx, gridy, gridz);

  _lpk_reo_3<D, T, R, C, F><<<blockPerGrid, threadsPerBlock, sm_size,
                              *(cudaStream_t *)handle.get(queue_idx)>>>(
      thrust::raw_pointer_cast(shape.data()),
      thrust::raw_pointer_cast(shape_c.data()),
      thrust::raw_pointer_cast(ldvs.data()),
      thrust::raw_pointer_cast(ldws.data()), processed_dims.size(),
      thrust::raw_pointer_cast(processed_dims.data()), curr_dim_r, curr_dim_c,
      curr_dim_f, ddist_r, dratio_r, dv1, lddv11, lddv12, dv2, lddv21, lddv22,
      dw, lddw1, lddw2);
  gpuErrchk(cudaGetLastError());
#ifdef MGARD_CUDA_DEBUG
  gpuErrchk(cudaDeviceSynchronize());
#endif
}

template <uint32_t D, typename T>
void lpk_reo_3(Handle<D, T> &handle, thrust::device_vector<int> shape,
               thrust::device_vector<int> shape_c,
               thrust::device_vector<int> ldvs, thrust::device_vector<int> ldws,
               thrust::device_vector<int> processed_dims, int curr_dim_r,
               int curr_dim_c, int curr_dim_f, T *ddist_r, T *dratio_r, T *dv1,
               int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
               int lddw1, int lddw2, int queue_idx, int config) {

#define LPK(R, C, F)                                                           \
  {                                                                            \
    lpk_reo_3_adaptive_launcher<D, T, R, C, F>(                                \
        handle, shape, shape_c, ldvs, ldws, processed_dims, curr_dim_r,        \
        curr_dim_c, curr_dim_f, ddist_r, dratio_r, dv1, lddv11, lddv12, dv2,   \
        lddv21, lddv22, dw, lddw1, lddw2, queue_idx);                          \
  }
  bool profile = false;
#ifndef MGARD_CUDA_KERNEL_PROFILE
  profile = true;
#endif
  if (D >= 3) {
    if (profile || config == 6) {
      LPK(2, 2, 128)
    }
    if (profile || config == 5) {
      LPK(2, 2, 64)
    }
    if (profile || config == 4) {
      LPK(2, 2, 32)
    }
    if (profile || config == 3) {
      LPK(4, 4, 16)
    }
    if (profile || config == 2) {
      LPK(8, 8, 8)
    }
    if (profile || config == 1) {
      LPK(4, 4, 4)
    }
    if (profile || config == 0) {
      LPK(2, 2, 2)
    }
  } else {
    printf("Error: mass_trans_multiply_3_cpt is only for 3D data\n");
  }

#undef LPK
}

template <uint32_t D, typename T, int R, int C, int F>
void lpk_reo_3_adaptive_launcher(Handle<D, T> &handle, int *shape_h,
                                 int *shape_c_h, int *shape_d, int *shape_c_d,
                                 int *ldvs, int *ldws, int processed_n,
                                 int *processed_dims_h, int *processed_dims_d,
                                 int curr_dim_r, int curr_dim_c, int curr_dim_f,
                                 T *ddist_r, T *dratio_r, T *dv1, int lddv11,
                                 int lddv12, T *dv2, int lddv21, int lddv22,
                                 T *dw, int lddw1, int lddw2, int queue_idx) {

  int nr = shape_h[curr_dim_r];
  int nc = shape_h[curr_dim_c];
  int nf = shape_h[curr_dim_f];
  int nr_c = shape_c_h[curr_dim_r];
  int nc_c = shape_c_h[curr_dim_c];
  int nf_c = shape_c_h[curr_dim_f];

  int total_thread_z = nr_c;
  // if (nr_c % 2 == 1){ total_thread_z = nr_c - 1; }
  // else { total_thread_z = nr_c; }
  int total_thread_y = nc_c;
  int total_thread_x = nf_c;

  int tbx, tby, tbz, gridx, gridy, gridz;
  dim3 threadsPerBlock, blockPerGrid;
  size_t sm_size;

  tbz = std::min(R, total_thread_z);
  tby = std::min(C, total_thread_y);
  tbx = std::min(F, total_thread_x);
  sm_size = ((R * 2 + 3) * C * F + (R * 2 + 3) * 2) * sizeof(T);
  sm_size += (D * 5) * sizeof(int);
  gridz = ceil((float)total_thread_z / tbz);
  gridy = ceil((float)total_thread_y / tby);
  gridx = ceil((float)total_thread_x / tbx);
  for (int d = 0; d < D; d++) {
    if (d != curr_dim_f && d != curr_dim_c && d != curr_dim_r) {
      int t = shape_h[d];
      for (int k = 0; k < processed_n; k++) {
        if (d == processed_dims_h[k]) {
          t = shape_c_h[d];
        }
      }
      gridx *= t;
    }
  }
  threadsPerBlock = dim3(tbx, tby, tbz);
  blockPerGrid = dim3(gridx, gridy, gridz);
  // printf("lpk_reo_3 exec config (%d %d %d) (%d %d %d)\n", tbx, tby, tbz,
  // gridx, gridy, gridz);

  _lpk_reo_3<D, T, R, C, F><<<blockPerGrid, threadsPerBlock, sm_size,
                              *(cudaStream_t *)handle.get(queue_idx)>>>(
      shape_d, shape_c_d, ldvs, ldws, processed_n, processed_dims_d, curr_dim_r,
      curr_dim_c, curr_dim_f, ddist_r, dratio_r, dv1, lddv11, lddv12, dv2,
      lddv21, lddv22, dw, lddw1, lddw2);
  gpuErrchk(cudaGetLastError());
#ifdef MGARD_CUDA_DEBUG
  gpuErrchk(cudaDeviceSynchronize());
#endif
}

template <uint32_t D, typename T>
void lpk_reo_3(Handle<D, T> &handle, int *shape_h, int *shape_c_h, int *shape_d,
               int *shape_c_d, int *ldvs, int *ldws, int processed_n,
               int *processed_dims_h, int *processed_dims_d, int curr_dim_r,
               int curr_dim_c, int curr_dim_f, T *ddist_r, T *dratio_r, T *dv1,
               int lddv11, int lddv12, T *dv2, int lddv21, int lddv22, T *dw,
               int lddw1, int lddw2, int queue_idx, int config) {

#define LPK(R, C, F)                                                           \
  {                                                                            \
    lpk_reo_3_adaptive_launcher<D, T, R, C, F>(                                \
        handle, shape_h, shape_c_h, shape_d, shape_c_d, ldvs, ldws,            \
        processed_n, processed_dims_h, processed_dims_d,\ 
                               curr_dim_r,                                     \
        curr_dim_c, curr_dim_f, ddist_r, dratio_r, dv1, lddv11, lddv12, dv2,   \
        lddv21, lddv22, dw, lddw1, lddw2, queue_idx);                          \
  }
  bool profile = false;
#ifdef MGARD_CUDA_KERNEL_PROFILE
  profile = true;
#endif
  if (D >= 3) {
    if (profile || config == 6) {
      LPK(2, 2, 128)
    }
    if (profile || config == 5) {
      LPK(2, 2, 64)
    }
    if (profile || config == 4) {
      LPK(2, 2, 32)
    }
    if (profile || config == 3) {
      LPK(4, 4, 16)
    }
    if (profile || config == 2) {
      LPK(8, 8, 8)
    }
    if (profile || config == 1) {
      LPK(4, 4, 4)
    }
    if (profile || config == 0) {
      LPK(2, 2, 2)
    }
  } else {
    printf("Error: mass_trans_multiply_3_cpt is only for 3D data\n");
  }

#undef LPK
}

} // namespace mgard_cuda

#endif