#include "zensim/execution/ExecutionPolicy.hpp"
#include "zensim/math/matrix/SparseMatrix.hpp"
#include "zensim/py_interop/SmallVec.hpp"

extern "C" {

#define INSTANTIATE_SPMAT_CAPIS(T, RowMajor, Ti, Tn)                                               \
  void build_from_triplets##__##seq##_##spm##_##T##_##RowMajor##_##Ti##_##Tn(                      \
      zs::SequentialExecutionPolicy *ppol,                                                         \
      zs::SparseMatrix<T, RowMajor, Ti, Tn, zs::ZSPmrAllocator<false>> *spmat, Ti nrows, Ti ncols, \
      Ti *is, Ti *js, T *vals, Tn nnz) {                                                           \
    spmat->build(*ppol, nrows, ncols, zs::range(is, is + nnz), zs::range(js, js + nnz),            \
                 zs::range(vals, vals + nnz));                                                     \
  }                                                                                                \
  void build_from_doublets##__##seq##_##spm##_##T##_##RowMajor##_##Ti##_##Tn(                      \
      zs::SequentialExecutionPolicy *ppol,                                                         \
      zs::SparseMatrix<T, RowMajor, Ti, Tn, zs::ZSPmrAllocator<false>> *spmat, Ti nrows, Ti ncols, \
      Ti *is, Ti *js, Tn nnz) {                                                                    \
    spmat->build(*ppol, nrows, ncols, zs::range(is, is + nnz), zs::range(js, js + nnz),            \
                 zs::false_c);                                                                     \
  }                                                                                                \
  void build_from_doublets_sym##__##seq##_##spm##_##T##_##RowMajor##_##Ti##_##Tn(                  \
      zs::SequentialExecutionPolicy *ppol,                                                         \
      zs::SparseMatrix<T, RowMajor, Ti, Tn, zs::ZSPmrAllocator<false>> *spmat, Ti nrows, Ti ncols, \
      Ti *is, Ti *js, Tn nnz) {                                                                    \
    spmat->build(*ppol, nrows, ncols, zs::range(is, is + nnz), zs::range(js, js + nnz),            \
                 zs::true_c);                                                                      \
  }                                                                                                \
  void fast_build_from_doublets##__##seq##_##spm##_##T##_##RowMajor##_##Ti##_##Tn(                 \
      zs::SequentialExecutionPolicy *ppol,                                                         \
      zs::SparseMatrix<T, RowMajor, Ti, Tn, zs::ZSPmrAllocator<false>> *spmat, Ti nrows, Ti ncols, \
      Ti *is, Ti *js, Tn nnz) {                                                                    \
    spmat->fastBuild(*ppol, nrows, ncols, zs::range(is, is + nnz), zs::range(js, js + nnz),        \
                     zs::false_c);                                                                 \
  }                                                                                                \
  void fast_build_from_doublets_sym##__##seq##_##spm##_##T##_##RowMajor##_##Ti##_##Tn(             \
      zs::SequentialExecutionPolicy *ppol,                                                         \
      zs::SparseMatrix<T, RowMajor, Ti, Tn, zs::ZSPmrAllocator<false>> *spmat, Ti nrows, Ti ncols, \
      Ti *is, Ti *js, Tn nnz) {                                                                    \
    spmat->fastBuild(*ppol, nrows, ncols, zs::range(is, is + nnz), zs::range(js, js + nnz),        \
                     zs::true_c);                                                                  \
  }                                                                                                \
  void transpose##__##seq##_##spm##_##T##_##RowMajor##_##Ti##_##Tn(                                \
      zs::SequentialExecutionPolicy *ppol,                                                         \
      zs::SparseMatrix<T, RowMajor, Ti, Tn, zs::ZSPmrAllocator<false>> *spmat) {                   \
    spmat->transposeFrom(*ppol, *spmat, zs::true_c);                                               \
  }

using mat33f = zs::vec<float, 3, 3>;
using mat33d = zs::vec<double, 3, 3>;

INSTANTIATE_SPMAT_CAPIS(float, false, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(float, true, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(double, false, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(double, true, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(mat33f, false, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(mat33f, true, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(mat33d, false, int, unsigned)
INSTANTIATE_SPMAT_CAPIS(mat33d, true, int, unsigned)
}