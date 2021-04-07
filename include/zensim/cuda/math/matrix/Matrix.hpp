#include <cublas_v2.h>
#include <cuda_runtime_api.h>
#include <cusolverSp.h>
#include <cusolverSp_LOWLEVEL_PREVIEW.h>
#include <cusolver_common.h>
#include <cusparse_v2.h>

#include "zensim/cuda/execution/CudaLibExecutionPolicy.cuh"
#include "zensim/math/matrix/Matrix.hpp"
#include "zensim/types/Event.hpp"

namespace zs {

  template <typename ValueType, typename IndexType> struct CudaYaleSparseMatrix
      : YaleSparseMatrix<ValueType, IndexType>,
        MatrixAccessor<CudaYaleSparseMatrix<ValueType, IndexType>> {
    using base_t = YaleSparseMatrix<ValueType, IndexType>;
    using value_type = ValueType;
    using index_type = IndexType;

    cusparseMatDescr_t descr{0};
    cusparseSpMatDescr_t spmDescr{0};
    csrcholInfo_t cholInfo{nullptr};

    void analyze_pattern(const CudaLibComponentExecutionPolicy<culib_cusolversp> &pol);
    void factorize(const CudaLibComponentExecutionPolicy<culib_cusolversp> &pol);
    void solve(Vector<value_type> &, const CudaLibComponentExecutionPolicy<culib_cusolversp> &pol,
               const Vector<value_type> &);

    // Vector<char> auxSpmBuffer{};
    Vector<char> auxCholBuffer{};
  };

}  // namespace zs