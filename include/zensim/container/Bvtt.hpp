#pragma once
#include "Vector.hpp"
#include "zensim/execution/Atomics.hpp"
#include "zensim/execution/ExecutionPolicy.hpp"

namespace zs {

  template <typename PrimIdT = std::size_t, typename NodeIdT = PrimIdT> struct BvttFront {
    using allocator_type = ZSPmrAllocator<>;
    using prim_id_t = PrimIdT;
    using node_id_t = NodeIdT;
    using prim_vector_t = Vector<prim_id_t>;
    using node_vector_t = Vector<node_id_t>;
    using index_t = unsigned long long;
    using counter_t = Vector<index_t>;

    BvttFront(std::size_t numPrims, std::size_t estimatedCount, memsrc_e mre = memsrc_e::host,
              ProcID devid = -1)
        : _numPrims{numPrims},
          _primIds{estimatedCount, mre, devid},
          _nodeIds{estimatedCount, mre, devid},
          _cnt{1, mre, devid} {
      counter_t res{1, memsrc_e::host, -1};
      res[0] = static_cast<index_t>(0);
      copy(MemoryEntity{_cnt.base(), (void *)_cnt.data()},
           MemoryEntity{res.base(), (void *)res.data()}, sizeof(index_t));
    }

    inline auto size() const {
      counter_t res{1, memsrc_e::host, -1};
      copy(MemoryEntity{res.base(), (void *)res.data()},
           MemoryEntity{_cnt.base(), (void *)_cnt.data()}, sizeof(index_t));
      return res[0];
    }
#if 0
    BvttFront(const allocator_type &allocator, std::size_t estimatedCount,
              memsrc_e mre = memsrc_e::host, ProcID devid = -1)
        : numPrims{numPrims},
          primIds{allocator, estimatedCount, mre, devid},
          nodeIds{allocator, estimatedCount, mre, devid},
          cnt{allocator, 1, mre, devid} {}
#endif
    std::size_t _numPrims;
    prim_vector_t _primIds;
    counter_t _offsets;
    node_vector_t _nodeIds;
    counter_t _cnt;
  };

  template <execspace_e space, typename BvttFrontT, typename = void> struct BvttFrontView {
    using prim_id_t = typename BvttFrontT::prim_id_t;
    using node_id_t = typename BvttFrontT::node_id_t;
    using prim_vector_t = typename BvttFrontT::prim_vector_t;
    using node_vector_t = typename BvttFrontT::node_vector_t;
    using index_t = typename BvttFrontT::index_t;
    using counter_t = typename BvttFrontT::counter_t;

    constexpr BvttFrontView() = default;
    ~BvttFrontView() = default;
    explicit constexpr BvttFrontView(BvttFrontT &bvfront)
        : _bound{std::min(bvfront._primIds.size(), bvfront._nodeIds.size())},
          _prims{bvfront._primIds.data()},
          _nodes{bvfront._nodeIds.data()},
          _cnt{bvfront._cnt.data()} {}

    ZS_FUNCTION void push_back(prim_id_t prim, node_id_t node) {
      const auto no = atomic_add(wrapv<space>{}, _cnt, (index_t)1);
      if (no < _bound) {
        _prims[no] = prim;
        _nodes[no] = node;
      }
    }

    const index_t _bound;
    typename prim_vector_t::pointer _prims;
    typename node_vector_t::pointer _nodes;
    typename counter_t::pointer _cnt;
  };

  template <execspace_e space, typename BvttFrontT> struct BvttFrontView<space, const BvttFrontT> {
    using prim_id_t = typename BvttFrontT::prim_id_t;
    using node_id_t = typename BvttFrontT::node_id_t;
    using prim_vector_t = typename BvttFrontT::prim_vector_t;
    using node_vector_t = typename BvttFrontT::node_vector_t;
    using index_t = typename BvttFrontT::index_t;
    using counter_t = typename BvttFrontT::counter_t;

    explicit constexpr BvttFrontView(const BvttFrontT &bvfront)
        : _bound{bvfront.size()},
          _prims{bvfront._primIds.data()},
          _nodes{bvfront._nodeIds.data()} {}

    constexpr auto prim(index_t i) const {
      if (i < _bound) return _prims[i];
      return ~(index_t)0;
    }
    constexpr auto node(index_t i) const {
      if (i < _bound) return _nodes[i];
      return ~(index_t)0;
    }

    const index_t _bound;
    typename prim_vector_t::const_pointer _prims;
    typename node_vector_t::const_pointer _nodes;
  };

  template <execspace_e ExecSpace, typename PrimIdT, typename NodeIdT>
  constexpr decltype(auto) proxy(BvttFront<PrimIdT, NodeIdT> &front) {
    return BvttFrontView<ExecSpace, BvttFront<PrimIdT, NodeIdT>>{front};
  }
  template <execspace_e ExecSpace, typename PrimIdT, typename NodeIdT>
  constexpr decltype(auto) proxy(const BvttFront<PrimIdT, NodeIdT> &front) {
    return BvttFrontView<ExecSpace, const BvttFront<PrimIdT, NodeIdT>>{front};
  }

  template <execspace_e space, typename PrimIdT, typename NodeIdT>
  inline void reorder_bvtt_front(BvttFront<PrimIdT, NodeIdT> &front) {
    // count front nodes by prim id;
    // scan
    // reorder front
  }

}  // namespace zs