#include "Query.hpp"
#include "zensim/execution/ExecutionPolicy.hpp"
#include "zensim/simulation/sparsity/SparsityOp.hpp"

namespace zs {

  template <execspace_e space>
  GeneralIndexBuckets index_buckets_for_particles(const GeneralParticles &particles, float dx) {
    return match([dx](const auto &pars) -> GeneralIndexBuckets {
      using particles_t = remove_cvref_t<decltype(pars)>;
      // constexpr int dim = particles_t::dim;
      using indexbuckets_t = IndexBuckets<particles_t::dim>;
      using vector_t = typename indexbuckets_t::vector_t;
      const auto memLoc = pars.space();
      const auto did = pars.devid();

      indexbuckets_t indexBuckets{};
      indexBuckets._dx = dx;
      // table
      auto &table = indexBuckets._table;
      table = RM_CVREF_T(table){pars.size(), memLoc, did};

      constexpr auto execTag = wrapv<space>{};
      auto execPol = par_exec(execTag).sync(true);
      if constexpr (space == execspace_e::cuda) execPol.device(did);
      execPol(range(table._tableSize), CleanSparsity{execTag, table});
      execPol(range(pars.size()),
              ComputeSparsity{execTag, dx, 1, table, pars.attrVector("pos"), 0});
      /// counts, offsets, indices
      // counts
      auto &counts = indexBuckets._counts;
      auto numCells = table.size() + 1;

      counts = vector_t{(std::size_t)numCells, memLoc, did};
      match([&counts](auto &&memTag) {
        memset(memTag, counts.data(), 0, sizeof(typename vector_t::value_type) * counts.size());
      })(counts.memoryLocation().getTag());

      auto tmp = counts;  // zero-ed array
      execPol(range(pars.size()),
              SpatiallyCount{execTag, dx, table, pars.attrVector("pos"), counts, 1, 0});
      // offsets
      auto &offsets = indexBuckets._offsets;
      offsets = vector_t{(std::size_t)numCells, memLoc, did};
      exclusive_scan(execPol, counts.begin(), counts.end(), offsets.begin());
      // indices
      auto &indices = indexBuckets._indices;
      indices = vector_t{pars.size(), memLoc, did};
      execPol(range(pars.size()), SpatiallyDistribute{execTag, dx, table, pars.attrVector("pos"),
                                                      tmp, offsets, indices, 1, 0});
      return indexBuckets;
    })(particles);
  }

  template <typename ExecPolicy>
  GeneralIndexBuckets index_buckets_for_particles(const ExecPolicy &execPol,
                                                  const GeneralParticles &particles, float dx,
                                                  float displacement) {
    return match([&execPol, dx, displacement](const auto &pars) -> GeneralIndexBuckets {
      using particles_t = remove_cvref_t<decltype(pars)>;
      using indexbuckets_t
          = IndexBuckets<particles_t::dim,
                         conditional_t<(sizeof(typename particles_t::size_type) > 4), i64, i32>,
                         int>;
      using vector_t = typename indexbuckets_t::vector_t;

      const auto &allocator = pars.get_allocator();

      indexbuckets_t indexBuckets{};
      indexBuckets._dx = dx;
      // table
      auto &table = indexBuckets._table;
      table = RM_CVREF_T(table){allocator, pars.size()};

      constexpr execspace_e space = RM_CVREF_T(execPol)::exec_tag::value;
      constexpr auto execTag = wrapv<space>{};
      execPol(range(table._tableSize), CleanSparsity{execTag, table});
      execPol(range(pars.size()),
              ComputeSparsity{execTag, dx, 1, table, pars.attrVector("pos"), 0, displacement});
      /// counts, offsets, indices
      // counts
      auto &counts = indexBuckets._counts;
      auto numCells = table.size() + 1;

      counts = vector_t{allocator, (std::size_t)numCells};
      match([&counts](auto &&memTag) {
        memset(memTag, counts.data(), 0, sizeof(typename vector_t::value_type) * counts.size());
      })(counts.memoryLocation().getTag());

      auto tmp = counts;  // zero-ed array
      execPol(range(pars.size()), SpatiallyCount{execTag, dx, table, pars.attrVector("pos"), counts,
                                                 1, 0, displacement});
      // offsets
      auto &offsets = indexBuckets._offsets;
      offsets = vector_t{allocator, (std::size_t)numCells};
      exclusive_scan(execPol, counts.begin(), counts.end(), offsets.begin());
      // fmt::print("scanned num particles: {}, num buckets: {} ({})\n", offsets[numCells - 1],
      //           numCells - 1, indexBuckets.numBuckets());
      // indices
      auto &indices = indexBuckets._indices;
      indices = vector_t{allocator, pars.size()};
      execPol(range(pars.size()), SpatiallyDistribute{execTag, dx, table, pars.attrVector("pos"),
                                                      tmp, offsets, indices, 1, 0, displacement});
      return indexBuckets;
    })(particles);
  }

}  // namespace zs