/*
 * Copyright 2021, Oak Ridge National Laboratory.
 * MGARD-GPU: MultiGrid Adaptive Reduction of Data Accelerated by GPUs
 * Author: Jieyang Chen (chenj3@ornl.gov)
 * Date: April 2, 2021
 */

#include "cuda/GridProcessingKernel.h"
#include "cuda/GridProcessingKernel.hpp"

namespace mgard_cuda {

#define KERNELS(D_GLOBAL, D_LOCAL, T, INTERPOLATION, COEFF_RESTORE, TYPE)      \
  template void                                                                \
  gpk_rev<D_GLOBAL, D_LOCAL, T, INTERPOLATION, COEFF_RESTORE, TYPE>(           \
      Handle<D_GLOBAL, T> & handle, thrust::device_vector<int> shape,          \
      thrust::device_vector<int> shape_c, thrust::device_vector<int> ldvs,     \
      thrust::device_vector<int> ldws,                                         \
      thrust::device_vector<int> unprocessed_dims, int curr_dim_r,             \
      int curr_dim_c, int curr_dim_f, T *dratio_r, T *dratio_c, T *dratio_f,   \
      T *dv, int lddv1, int lddv2, T *dw, int lddw1, int lddw2, T *dwf,        \
      int lddwf1, int lddwf2, T *dwc, int lddwc1, int lddwc2, T *dwr,          \
      int lddwr1, int lddwr2, T *dwcf, int lddwcf1, int lddwcf2, T *dwrf,      \
      int lddwrf1, int lddwrf2, T *dwrc, int lddwrc1, int lddwrc2, T *dwrcf,   \
      int lddwrcf1, int lddwrcf2, int svr, int svc, int svf, int nvr, int nvc, \
      int nvf, int queue_idx, int config);

KERNELS(1, 1, double, true, false, 1)
KERNELS(1, 1, float, true, false, 1)
KERNELS(2, 2, double, true, false, 1)
KERNELS(2, 2, float, true, false, 1)
KERNELS(3, 3, double, true, false, 1)
KERNELS(3, 3, float, true, false, 1)

KERNELS(4, 3, double, true, false, 1)
KERNELS(4, 3, float, true, false, 1)
KERNELS(5, 3, double, true, false, 1)
KERNELS(5, 3, float, true, false, 1)

KERNELS(4, 2, double, true, false, 2)
KERNELS(4, 2, float, true, false, 2)
KERNELS(5, 2, double, true, false, 2)
KERNELS(5, 2, float, true, false, 2)

KERNELS(4, 3, double, true, false, 2)
KERNELS(4, 3, float, true, false, 2)
KERNELS(5, 3, double, true, false, 2)
KERNELS(5, 3, float, true, false, 2)

#undef KERNELS

#define KERNELS(D_GLOBAL, D_LOCAL, T, INTERPOLATION, COEFF_RESTORE, TYPE)      \
  template void                                                                \
  gpk_rev<D_GLOBAL, D_LOCAL, T, INTERPOLATION, COEFF_RESTORE, TYPE>(           \
      Handle<D_GLOBAL, T> & handle, int *shape_h, int *shape_d,                \
      int *shape_c_d, int *ldvs, int *ldws, int unprocessed_n,                 \
      int *unprocessed_dims, int curr_dim_r, int curr_dim_c, int curr_dim_f,   \
      T *dratio_r, T *dratio_c, T *dratio_f, T *dv, int lddv1, int lddv2,      \
      T *dw, int lddw1, int lddw2, T *dwf, int lddwf1, int lddwf2, T *dwc,     \
      int lddwc1, int lddwc2, T *dwr, int lddwr1, int lddwr2, T *dwcf,         \
      int lddwcf1, int lddwcf2, T *dwrf, int lddwrf1, int lddwrf2, T *dwrc,    \
      int lddwrc1, int lddwrc2, T *dwrcf, int lddwrcf1, int lddwrcf2, int svr, \
      int svc, int svf, int nvr, int nvc, int nvf, int queue_idx, int config);

KERNELS(1, 1, double, true, false, 1)
KERNELS(1, 1, float, true, false, 1)
KERNELS(2, 2, double, true, false, 1)
KERNELS(2, 2, float, true, false, 1)
KERNELS(3, 3, double, true, false, 1)
KERNELS(3, 3, float, true, false, 1)

KERNELS(4, 3, double, true, false, 1)
KERNELS(4, 3, float, true, false, 1)
KERNELS(5, 3, double, true, false, 1)
KERNELS(5, 3, float, true, false, 1)

KERNELS(4, 2, double, true, false, 2)
KERNELS(4, 2, float, true, false, 2)
KERNELS(5, 2, double, true, false, 2)
KERNELS(5, 2, float, true, false, 2)

KERNELS(4, 3, double, true, false, 2)
KERNELS(4, 3, float, true, false, 2)
KERNELS(5, 3, double, true, false, 2)
KERNELS(5, 3, float, true, false, 2)

#undef KERNELS

} // namespace mgard_cuda