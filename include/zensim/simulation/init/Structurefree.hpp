#pragma once
#include "zensim/TypeAlias.hpp"
#include "zensim/container/Vector.hpp"
#include "zensim/math/Vec.h"
#include "zensim/types/Polymorphism.h"

namespace zs {

  template <typename ValueT = f32, int d = 3> struct Particles {
    using T = ValueT;
    using TV = ValueT[d];
    using TM = ValueT[d][d];
    using TMAffine = ValueT[d + 1][d + 1];
    static constexpr int dim = d;
    constexpr MemoryHandle handle() const noexcept { return static_cast<MemoryHandle>(X); }
    constexpr memsrc_e space() const noexcept { return X.memspace(); }
    constexpr ProcID devid() const noexcept { return X.devid(); }
    constexpr std::size_t size() const noexcept { return X.size(); }
    Vector<T> M;
    Vector<TV> X, V;
    Vector<T> J;
    Vector<TM> F;
  };

  using GeneralParticles
      = variant<Particles<f32, 2>, Particles<f64, 2>, Particles<f32, 3>, Particles<f64, 3>>;

  template <execspace_e, typename ParticlesT, typename = void> struct ParticlesProxy;
  template <execspace_e space, typename ParticlesT> struct ParticlesProxy<space, ParticlesT> {
    using T = typename ParticlesT::T;
    using TV = typename ParticlesT::TV;
    using TM = typename ParticlesT::TM;
    static constexpr int dim = ParticlesT::dim;
    using size_type = std::size_t;

    constexpr ParticlesProxy() = default;
    ~ParticlesProxy() = default;
    explicit ParticlesProxy(ParticlesT &particles)
        : _M{particles.M.data()},
          _X{particles.X.data()},
          _V{particles.V.data()},
          _J{particles.J.data()},
          _F{particles.F.data()},
          _particleCount{particles.size()} {}

    constexpr auto &mass(size_type parid) { return _M[parid]; }
    constexpr auto mass(size_type parid) const { return _M[parid]; }
    constexpr auto &pos(size_type parid) { return _X[parid]; }
    constexpr const auto &pos(size_type parid) const { return _X[parid]; }
    constexpr auto &vel(size_type parid) { return _V[parid]; }
    constexpr const auto &vel(size_type parid) const { return _V[parid]; }
    /// deformation for water
    constexpr auto &J(size_type parid) { return _J[parid]; }
    constexpr const auto &J(size_type parid) const { return _J[parid]; }
    /// deformation for solid
    constexpr auto &F(size_type parid) { return _F[parid]; }
    constexpr const auto &F(size_type parid) const { return _F[parid]; }
    constexpr auto size() const noexcept { return _particleCount; }

  protected:
    T *_M;
    TV *_X, *_V;
    T *_J;
    TM *_F;
    size_type _particleCount;
  };

  template <execspace_e ExecSpace, typename V, int d>
  decltype(auto) proxy(Particles<V, d> &particles) {
    return ParticlesProxy<ExecSpace, Particles<V, d>>{particles};
  }

  ///

  /// sizeof(float) = 4
  /// bin_size = 64
  /// attrib_size = 16
  template <typename V = dat32, int channel_bits = 4, int counter_bits = 6> struct ParticleBin {
    constexpr decltype(auto) operator[](int c) noexcept { return _data[c]; }
    constexpr decltype(auto) operator[](int c) const noexcept { return _data[c]; }
    constexpr auto &operator()(int c, int pid) noexcept { return _data[c][pid]; }
    constexpr auto operator()(int c, int pid) const noexcept { return _data[c][pid]; }

  protected:
    V _data[1 << channel_bits][1 << counter_bits];
  };

  template <typename PBin, int bin_bits = 4> struct ParticleGrid;
  template <typename V, int chnbits, int cntbits, int bin_bits>
  struct ParticleGrid<ParticleBin<V, chnbits, cntbits>, bin_bits> {
    static constexpr int nbins = (1 << bin_bits);
    using Bin = ParticleBin<V, chnbits, cntbits>;
    using Block = Bin[nbins];

    Vector<Block> blocks;
    Vector<int> ppbs;
  };
  template <typename V, int chnbits, int cntbits>
  struct ParticleGrid<ParticleBin<V, chnbits, cntbits>, -1> {
    using Bin = ParticleBin<V, chnbits, cntbits>;

    Vector<Bin> bins;
    Vector<int> cnts, offsets;
    Vector<int> ppbs;
  };

}  // namespace zs