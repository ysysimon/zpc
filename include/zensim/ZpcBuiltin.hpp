#pragma once

#include "zensim/ZpcFunctional.hpp"
#include "zensim/ZpcIterator.hpp"
#include "zensim/ZpcMathUtils.hpp"
#include "zensim/ZpcReflection.hpp"
#include "zensim/ZpcTuple.hpp"
#include "zensim/math/Tensor.hpp"
#include "zensim/math/Vec.h"
//
#include "zensim/py_interop/TileVectorView.hpp"
#include "zensim/py_interop/VectorView.hpp"
//
#include "zensim/ZpcFunction.hpp"
#include "zensim/execution/Atomics.hpp"
#include "zensim/types/Property.h"
#include "zensim/types/SmallVector.hpp"
// #include "zensim/types/SourceLocation.hpp"
#if defined(ZS_ENABLE_OPENMP) && ZS_ENABLE_OPENMP == 1
#  include "zensim/omp/Omp.h"
#endif

namespace zs {
  template <class T, class = int> struct printf_target;

  template <class T> struct printf_target<T, enable_if_t<is_floating_point_v<T>>> {
    using type = float;
    constexpr static char placeholder[] = "%f";
  };

  template <class T> struct printf_target<T, enable_if_t<is_integral_v<T>>> {
    using type = int;
    constexpr static char placeholder[] = "%d";
  };

  template <class T> struct printf_target<
      T, enable_if_t<is_same_v<decay_t<T>, char*> || is_same_v<decay_t<T>, const char*>>> {
    using type = const char*;
    constexpr static char placeholder[] = "%s";
  };

  ZS_FUNCTION SmallString join(const SmallString& joinStr) { return SmallString{}; }

  template <class T> ZS_FUNCTION SmallString join(const SmallString& joinStr, T&& s0) { return s0; }

  template <class T, class... Types>
  ZS_FUNCTION SmallString join(const SmallString& joinStr, T&& s0, Types&&... args) {
    return s0 + joinStr + join(joinStr, FWD(args)...);
  }

  template <class... Types> ZS_FUNCTION void print_internal(Types&&... args) {
    auto formatStr = join(" ", SmallString{printf_target<remove_cvref_t<Types>>::placeholder}...);
    printf(formatStr.asChars(),
           static_cast<typename printf_target<remove_cvref_t<Types>>::type>(args)...);
  }

  template <class... Types> ZS_FUNCTION void print(Types&&... args) {
    print_internal(FWD(args)..., "\n");
  }

  ZS_FUNCTION auto tid() {
#ifdef __CUDACC__
    return blockIdx.x * blockDim.x + threadIdx.x;
#elif defined(_OPENMP)
    return ::omp_get_thread_num();
#else
    return 0;
#endif
  }

}  // namespace zs
using vec2i = zs::vec<int, 2>;
using vec3i = zs::vec<int, 3>;
using vec4i = zs::vec<int, 4>;
using vec2f = zs::vec<float, 2>;
using vec3f = zs::vec<float, 3>;
using vec4f = zs::vec<float, 4>;
using vec2d = zs::vec<double, 2>;
using vec3d = zs::vec<double, 3>;
using vec4d = zs::vec<double, 4>;
using mat2i = zs::vec<int, 2, 2>;
using mat3i = zs::vec<int, 3, 3>;
using mat4i = zs::vec<int, 4, 4>;
using mat2f = zs::vec<float, 2, 2>;
using mat3f = zs::vec<float, 3, 3>;
using mat4f = zs::vec<float, 4, 4>;
using mat2d = zs::vec<double, 2, 2>;
using mat3d = zs::vec<double, 3, 3>;
using mat4d = zs::vec<double, 4, 4>;