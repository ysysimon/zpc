#pragma once
#include "zensim/cuda/execution/ExecutionPolicy.cuh"
#include "zensim/cuda/physics/ConstitutiveModel.hpp"
#include "zensim/execution/ExecutionPolicy.hpp"
#include "zensim/geometry/Structurefree.hpp"
#include "zensim/physics/ConstitutiveModel.hpp"
#include "zensim/simulation/Utils.hpp"
#include "zensim/simulation/transfer/P2G.hpp"

namespace zs {

  template <transfer_scheme_e scheme, typename ModelT, typename ParticlesT, typename TableT,
            typename GridBlocksT>
  struct P2GTransfer<scheme, ModelT, ParticlesView<execspace_e::cuda, ParticlesT>,
                     HashTableView<execspace_e::cuda, TableT>,
                     GridBlocksView<execspace_e::cuda, GridBlocksT>> {
    using model_t = ModelT;  ///< constitutive model
    using particles_t = ParticlesView<execspace_e::cuda, ParticlesT>;
    using partition_t = HashTableView<execspace_e::cuda, TableT>;
    using gridblocks_t = GridBlocksView<execspace_e::cuda, GridBlocksT>;
    using gridblock_t = typename gridblocks_t::block_t;
    static_assert(particles_t::dim == partition_t::dim && particles_t::dim == gridblocks_t::dim,
                  "[particle-partition-grid] dimension mismatch");

    explicit P2GTransfer(wrapv<execspace_e::cuda>, wrapv<scheme>, float dt, const ModelT& model,
                         ParticlesT& particles, TableT& table, GridBlocksT& gridblocks)
        : model{model},
          particles{proxy<execspace_e::cuda>(particles)},
          partition{proxy<execspace_e::cuda>(table)},
          gridblocks{proxy<execspace_e::cuda>(gridblocks)},
          dt{dt} {}

    constexpr float dxinv() const {
      return static_cast<decltype(gridblocks._dx.asFloat())>(1.0) / gridblocks._dx.asFloat();
    }

    __forceinline__ __device__ void operator()(typename particles_t::size_type parid) noexcept {
      float const dx = gridblocks._dx.asFloat();
      float const dx_inv = dxinv();
      if constexpr (particles_t::dim == 3) {
        float const D_inv = 4.f * dx_inv * dx_inv;
        using ivec3 = vec<int, particles_t::dim>;
        using vec3 = vec<float, particles_t::dim>;
        using vec9 = vec<float, particles_t::dim * particles_t::dim>;
        using vec3x3 = vec<float, particles_t::dim, particles_t::dim>;

        vec3 local_pos{particles.pos(parid)};
        vec3 vel{particles.vel(parid)};
        float mass = particles.mass(parid);
        vec9 contrib{}, C{particles.C(parid)};

        if constexpr (is_same_v<model_t, EquationOfStateConfig>) {
          float J = particles.J(parid);
          float vol = model.volume * J;
          float pressure = model.bulk;
          {
            float J2 = J * J;
            float J4 = J2 * J2;
            // pressure = pressure * (powf(J, -model.gamma) - 1.f);
            pressure = pressure * (1 / (J * J2 * J4) - 1);  // from Bow
          }
          contrib[0] = ((C[0] + C[0]) * model.viscosity - pressure) * vol;
          contrib[1] = (C[1] + C[3]) * model.viscosity * vol;
          contrib[2] = (C[2] + C[6]) * model.viscosity * vol;

          contrib[3] = (C[3] + C[1]) * model.viscosity * vol;
          contrib[4] = ((C[4] + C[4]) * model.viscosity - pressure) * vol;
          contrib[5] = (C[5] + C[7]) * model.viscosity * vol;

          contrib[6] = (C[6] + C[2]) * model.viscosity * vol;
          contrib[7] = (C[7] + C[5]) * model.viscosity * vol;
          contrib[8] = ((C[8] + C[8]) * model.viscosity - pressure) * vol;

        } else {
          const auto [mu, lambda] = lame_parameters(model.E, model.nu);
          vec9 F{particles.F(parid)};
          if constexpr (is_same_v<model_t, FixedCorotatedConfig>) {
            compute_stress_fixedcorotated(model.volume, mu, lambda, F, contrib);
          } else if constexpr (is_same_v<model_t, VonMisesFixedCorotatedConfig>) {
            compute_stress_vonmisesfixedcorotated(model.volume, mu, lambda, model.yieldStress, F,
                                                  contrib);
          } else {
            /// with plasticity additionally
            float logJp = particles.logJp(parid);
            if constexpr (is_same_v<model_t, DruckerPragerConfig>) {
              compute_stress_sand(model.volume, mu, lambda, model.cohesion, model.beta,
                                  model.yieldSurface, model.volumeCorrection, logJp, F, contrib);
            } else if constexpr (is_same_v<model_t, NACCConfig>) {
              compute_stress_nacc(model.volume, mu, lambda, model.bulk(), model.xi, model.beta,
                                  model.Msqr(), model.hardeningOn, logJp, F, contrib);
            }
            particles.logJp(parid) = logJp;
          }
        }

        contrib = C * mass - contrib * dt * D_inv;

        using VT
            = std::decay_t<decltype(std::declval<typename gridblock_t::value_type>().asFloat())>;
        auto arena = make_local_arena((VT)dx, local_pos);
        for (auto loc : arena.range()) {
          auto [grid_block, local_index] = unpack_coord_in_grid(
              arena.coord(loc), gridblock_t::side_length(), partition, gridblocks);
          auto xixp = arena.diff(loc);
          VT W = arena.weight(loc);
          atomicAdd(&grid_block(0, local_index).asFloat(), mass * W);
          for (int d = 0; d < particles_t::dim; ++d)
            atomicAdd(&grid_block(d + 1, local_index).asFloat(),
                      (VT)(W
                           * (mass * vel[d]
                              + (contrib[d] * xixp[0] + contrib[3 + d] * xixp[1]
                                 + contrib[6 + d] * xixp[2]))));
        }
      }
    }

    model_t model;
    particles_t particles;
    partition_t partition;
    gridblocks_t gridblocks;
    float dt;
  };

  template <transfer_scheme_e scheme, typename ModelT, typename ParticlesT, typename TableT,
            typename GridsT>
  struct P2GTransfer<scheme, ModelT, ParticlesView<execspace_e::cuda, ParticlesT>,
                     HashTableView<execspace_e::cuda, TableT>,
                     GridsView<execspace_e::cuda, GridsT>> {
    using model_t = ModelT;  ///< constitutive model
    using particles_t = ParticlesView<execspace_e::cuda, ParticlesT>;
    using partition_t = HashTableView<execspace_e::cuda, TableT>;
    using grids_t = GridsView<execspace_e::cuda, GridsT>;
    static_assert(particles_t::dim == partition_t::dim && particles_t::dim == GridsT::dim,
                  "[particle-partition-grid] dimension mismatch");

    explicit P2GTransfer(wrapv<execspace_e::cuda>, wrapv<scheme>, float dt, const ModelT& model,
                         ParticlesT& particles, TableT& table, GridsT& grids)
        : model{model},
          particles{proxy<execspace_e::cuda>(particles)},
          partition{proxy<execspace_e::cuda>(table)},
          grids{proxy<execspace_e::cuda>(grids)},
          dt{dt} {}

    constexpr float dxinv() const { return static_cast<decltype(grids._dx)>(1.0) / grids._dx; }

    __forceinline__ __device__ void operator()(typename particles_t::size_type parid) noexcept {
      float const dx = grids._dx;
      float const dx_inv = dxinv();
      if constexpr (particles_t::dim == 3) {
        float const D_inv = 4.f * dx_inv * dx_inv;
        using ivec3 = vec<int, particles_t::dim>;
        using vec3 = vec<float, particles_t::dim>;
        using vec9 = vec<float, particles_t::dim * particles_t::dim>;
        using vec3x3 = vec<float, particles_t::dim, particles_t::dim>;

        vec3 local_pos{particles.pos(parid)};
        vec3 vel{particles.vel(parid)};
        float mass = particles.mass(parid);
        vec9 contrib{}, C{particles.C(parid)};

        if constexpr (is_same_v<model_t, EquationOfStateConfig>) {
          float J = particles.J(parid);
          float vol = model.volume * J;
          float pressure = model.bulk;
          {
            float J2 = J * J;
            float J4 = J2 * J2;
            // pressure = pressure * (powf(J, -model.gamma) - 1.f);
            pressure = pressure * (1 / (J * J2 * J4) - 1);  // from Bow
          }
          contrib[0] = ((C[0] + C[0]) * model.viscosity - pressure) * vol;
          contrib[1] = (C[1] + C[3]) * model.viscosity * vol;
          contrib[2] = (C[2] + C[6]) * model.viscosity * vol;

          contrib[3] = (C[3] + C[1]) * model.viscosity * vol;
          contrib[4] = ((C[4] + C[4]) * model.viscosity - pressure) * vol;
          contrib[5] = (C[5] + C[7]) * model.viscosity * vol;

          contrib[6] = (C[6] + C[2]) * model.viscosity * vol;
          contrib[7] = (C[7] + C[5]) * model.viscosity * vol;
          contrib[8] = ((C[8] + C[8]) * model.viscosity - pressure) * vol;

        } else {
          const auto [mu, lambda] = lame_parameters(model.E, model.nu);
          vec9 F{particles.F(parid)};
          if constexpr (is_same_v<model_t, FixedCorotatedConfig>) {
            compute_stress_fixedcorotated(model.volume, mu, lambda, F, contrib);
          } else if constexpr (is_same_v<model_t, VonMisesFixedCorotatedConfig>) {
            compute_stress_vonmisesfixedcorotated(model.volume, mu, lambda, model.yieldStress, F,
                                                  contrib);
          } else {
            /// with plasticity additionally
            float logJp = particles.logJp(parid);
            if constexpr (is_same_v<model_t, DruckerPragerConfig>) {
              compute_stress_sand(model.volume, mu, lambda, model.cohesion, model.beta,
                                  model.yieldSurface, model.volumeCorrection, logJp, F, contrib);
            } else if constexpr (is_same_v<model_t, NACCConfig>) {
              compute_stress_nacc(model.volume, mu, lambda, model.bulk(), model.xi, model.beta,
                                  model.Msqr(), model.hardeningOn, logJp, F, contrib);
            }
            particles.logJp(parid) = logJp;
          }
        }

        contrib = contrib * -dt * D_inv;

        using VT = typename grids_t::value_type;
        auto arena = make_local_arena((VT)dx, local_pos);
        for (auto loc : arena.range()) {
          auto [grid_block, local_index]
              = unpack_coord_in_grid(arena.coord(loc), grids_t::side_length, partition, grids);
          auto xixp = arena.diff(loc);
          VT W = arena.weight(loc);
          const auto cellid = grids_t::coord_to_cellid(local_index);
          atomicAdd(&grid_block(0, cellid), mass * W);
          for (int d = 0; d != particles_t::dim; ++d) {
            // vi: W m v + W m C (xi - xp)
            atomicAdd(
                &grid_block(1 + d, cellid),
                W * mass * (vel[d] + (C[d] * xixp[0] + C[3 + d] * xixp[1] + C[6 + d] * xixp[2])));
            // rhs: f * dt
            atomicAdd(
                &grid_block(particles_t::dim + 1 + d, cellid),
                (contrib[d] * xixp[0] + contrib[3 + d] * xixp[1] + contrib[6 + d] * xixp[2]) * W);
          }
        }
      }
    }

    model_t model;
    particles_t particles;
    partition_t partition;
    grids_t grids;
    float dt;
  };

}  // namespace zs