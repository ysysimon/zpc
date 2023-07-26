#pragma once
#include "SparseGrid.hpp"
#include "zensim/types/Mask.hpp"

namespace zs {

  template <int dim, typename ValueT, typename SideLengthBits = index_sequence<3, 4, 5>,
            typename Indices = index_sequence<0, 1, 2>, typename AllocatorT = ZSPmrAllocator<>>
  struct AdaptiveGridImpl;

  template <int dim, typename ValueT, size_t... Ns> using AdaptiveGrid
      = AdaptiveGridImpl<dim, ValueT, index_sequence<Ns...>, make_index_sequence<sizeof...(Ns)>,
                         ZSPmrAllocator<>>;

  /// @brief stores all leaf blocks of an adaptive octree including halo regions
  template <int dim_, typename ValueT, size_t... SideLengthBits, size_t... Is>
  struct AdaptiveGridImpl<dim_, ValueT, index_sequence<SideLengthBits...>, index_sequence<Is...>,
                          ZSPmrAllocator<>> {
    using value_type = ValueT;
    using allocator_type = ZSPmrAllocator<>;
    using size_type = size_t;
    using index_type = zs::make_signed_t<size_type>;  // associated with the number of blocks
    using integer_coord_component_type = int;

    static constexpr auto deduce_basic_value_type() noexcept {
      if constexpr (is_vec<value_type>::value)
        return wrapt<typename value_type::value_type>{};
      else
        return wrapt<value_type>{};
    }
    using coord_component_type = typename RM_CVREF_T(deduce_basic_value_type())::type;
    static_assert(is_floating_point_v<coord_component_type>,
                  "coord type should be floating point.");
    ///
    static constexpr int dim = dim_;
    static constexpr size_t num_levels = sizeof...(SideLengthBits);

    // length_bits
    using length_bit_counts_type = value_seq<SideLengthBits...>;
    static constexpr integer_coord_component_type length_bit_counts[num_levels]
        = {(integer_coord_component_type)length_bit_counts_type::template value<Is>...};
    using global_length_bit_counts_type
        = decltype(declval<length_bit_counts_type>().template scan<1>());
    static constexpr integer_coord_component_type global_length_bit_counts[num_levels]
        = {(integer_coord_component_type)global_length_bit_counts_type::template value<Is>...};

    // side_lengths
    struct impl_two_pow {
      constexpr integer_coord_component_type operator()(integer_coord_component_type b) noexcept {
        return (integer_coord_component_type)1 << b;
      }
    };
    using side_lengths_type
        = decltype(declval<length_bit_counts_type>().transform(declval<impl_two_pow>()));
    static constexpr integer_coord_component_type side_lengths[num_levels]
        = {side_lengths_type::template value<Is>...};
    using global_side_lengths_type
        = decltype(declval<global_length_bit_counts_type>().transform(declval<impl_two_pow>()));
    static constexpr integer_coord_component_type global_side_lengths[num_levels]
        = {global_side_lengths_type::template value<Is>...};

    // block_sizes
    struct impl_dim_pow {
      constexpr size_t operator()(size_t sl) noexcept { return math::pow_integral(sl, dim); }
    };
    using block_sizes_type
        = decltype(declval<side_lengths_type>().transform(declval<impl_dim_pow>()));
    static constexpr size_t block_sizes[num_levels]
        = {(size_t)block_sizes_type::template value<Is>...};
    using global_block_sizes_type
        = decltype(declval<global_side_lengths_type>().transform(declval<impl_dim_pow>()));
    static constexpr size_t global_block_sizes[num_levels]
        = {(size_t)global_block_sizes_type::template value<Is>...};

    using integer_coord_type = vec<integer_coord_component_type, dim>;
    using coord_type = vec<coord_component_type, dim>;
    using packed_value_type = vec<value_type, dim>;

    template <size_type bs> using grid_storage_type = TileVector<value_type, bs, allocator_type>;
    template <size_type bs> using mask_storage_type = Vector<bit_mask<bs>, allocator_type>;
    using table_type = bht<integer_coord_component_type, dim, int, 16, allocator_type>;

    template <int level_> struct Level {
      static constexpr int level = level_;
      static constexpr integer_coord_component_type length_bit_count = length_bit_counts[level];
      static constexpr integer_coord_component_type global_length_bit_count
          = global_length_bit_counts[level];
      static constexpr integer_coord_component_type side_length = side_lengths[level];
      static constexpr integer_coord_component_type global_side_length = global_side_lengths[level];
      static constexpr size_t block_size = block_sizes[level];

      /// @note used for global coords
      static constexpr make_unsigned_t<integer_coord_component_type> block_mask
          = global_side_length - 1;
      static constexpr make_unsigned_t<integer_coord_component_type> origin_mask = ~block_mask;

      using grid_type = grid_storage_type<block_size>;
      using mask_type = mask_storage_type<block_size>;
      Level(const allocator_type &allocator, const std::vector<PropertyTag> &propTags, size_t count)
          : table{allocator, count},
            grid{allocator, propTags, count * block_size},
            valueMask{allocator, count},
            childMask{allocator, count} {
        grid.reset(0);
        childMask.reset(0);
        valueMask.reset(0);
      }
      Level(const allocator_type &allocator, size_t count)
          : Level(allocator, {{"sdf", 1}}, count) {}
      Level(size_t count = 0, memsrc_e mre = memsrc_e::host, ProcID devid = -1)
          : Level(get_default_allocator(mre, devid), count) {}
      Level(const std::vector<PropertyTag> &propTags, size_t count, memsrc_e mre = memsrc_e::host,
            ProcID devid = -1)
          : Level(get_default_allocator(mre, devid), propTags, count) {}

      ~Level() = default;

      Level(const Level &o) = default;
      Level(Level &&o) noexcept = default;
      Level &operator=(const Level &o) = default;
      Level &operator=(Level &&o) noexcept = default;

      auto numBlocks() const { return table.size(); }
      auto numReservedBlocks() const noexcept { return grid.numReservedTiles(); }

      template <typename ExecPolicy>
      void resize(ExecPolicy &&policy, size_type numBlocks, bool resizeGrid = true) {
        table.resize(FWD(policy), numBlocks);
        if (resizeGrid) grid.resize(numBlocks * (size_type)block_size);
      }
      template <typename ExecPolicy>
      void resizePartition(ExecPolicy &&policy, size_type numBlocks) {
        table.resize(FWD(policy), numBlocks);
      }
      void resizeGrid(size_type numBlocks) { grid.resize(numBlocks * (size_type)block_size); }
      template <typename Policy>
      void append_channels(Policy &&policy, const std::vector<PropertyTag> &tags) {
        grid.append_channels(FWD(policy), tags);
      }
      // byte-wise reset
      void reset(value_type val) { grid.reset(val); }
      // value-wise reset
      template <typename Policy> void reset(Policy &&policy, value_type val) {
        grid.reset(FWD(policy), val);
      }

      Level clone(const allocator_type &allocator) const {
        Level ret{};
        ret.table = table.clone(allocator);
        ret.grid = grid.clone(allocator);
        ret.childMask = childMask.clone(allocator);
        ret.valueMask = valueMask.clone(allocator);
        return ret;
      }

      auto originRange() const {
        auto bg = table._activeKeys.begin();
        return detail::iter_range(bg, bg + numBlocks());
      }
      auto originRange() {
        auto bg = table._activeKeys.begin();
        return detail::iter_range(bg, bg + numBlocks());
      }
      table_type table;
      grid_type grid;
      /// @note for levelset, valueMask indicates inside/outside
      /// @note for leaf level, childMask reserved for special use cases
      mask_type valueMask, childMask;
    };
    using transform_type = math::Transform<coord_component_type, dim>;

    template <auto I = 0> Level<I> &level(wrapv<I>) { return zs::get<I>(_levels); }
    template <auto I = 0> const Level<I> &level(wrapv<I>) const { return zs::get<I>(_levels); }
    template <auto I = 0> Level<I> &level(value_seq<I>) { return zs::get<I>(_levels); }
    template <auto I = 0> const Level<I> &level(value_seq<I>) const { return zs::get<I>(_levels); }

    constexpr MemoryLocation memoryLocation() const noexcept {
      return get<0>(_levels)._table.memoryLocation();
    }
    constexpr ProcID devid() const noexcept { return get<0>(_levels).table.devid(); }
    constexpr memsrc_e memspace() const noexcept { return get<0>(_levels).table.memspace(); }
    decltype(auto) get_allocator() const noexcept { return get<0>(_levels).table.get_allocator(); }
    static decltype(auto) get_default_allocator(memsrc_e mre, ProcID devid) {
      return get_memory_source(mre, devid);
    }

    template <auto I = 0> constexpr auto numBlocks(wrapv<I> = {}) const noexcept {
      return get<I>(_levels).numBlocks();
    }
    template <auto I = 0> constexpr auto numBlocks(value_seq<I> = {}) const noexcept {
      return get<I>(_levels).numBlocks();
    }
    constexpr size_t numTotalBlocks() const noexcept {
      size_t ret = 0;
      (void)((ret += numBlocks(wrapv<Is>{})), ...);
      return ret;
    }
    template <size_t I = 0> constexpr auto numReservedBlocks() const noexcept {
      return get<I>(_levels).numReservedBlocks();
    }
    template <typename Policy>
    void append_channels(Policy &&policy, const std::vector<PropertyTag> &tags) {
      (void)(get<Is>(_levels).append_channels(FWD(policy), tags), ...);
    }
    void reset(value_type val) { (void)(get<Is>(_levels).reset(val), ...); }
    // value-wise reset
    template <typename Policy> void reset(Policy &&policy, value_type val) {
      (void)(get<Is>(_levels).reset(policy, val), ...);
    }
    constexpr auto numChannels() const noexcept {
      return level(dim_c<num_levels - 1>).grid.numChannels();
    }
    bool hasProperty(const SmallString &str) const noexcept {
      return level(dim_c<num_levels - 1>).grid.hasProperty(str);
    }
    constexpr size_type getPropertySize(const SmallString &str) const {
      return level(dim_c<num_levels - 1>).grid.getPropertySize(str);
    }
    constexpr size_type getPropertyOffset(const SmallString &str) const {
      return level(dim_c<num_levels - 1>).grid.getPropertyOffset(str);
    }
    constexpr PropertyTag getPropertyTag(size_type i = 0) const {
      return level(dim_c<num_levels - 1>).grid.getPropertyTag(i);
    }
    constexpr const auto &getPropertyTags() const {
      return level(dim_c<num_levels - 1>).grid.getPropertyTags();
    }

    constexpr coord_type voxelSize() const {
      // does not consider shearing here
      coord_type ret{};
      for (int i = 0; i != dim; ++i) {
        coord_component_type sum = 0;
        for (int d = 0; d != dim; ++d) sum += zs::sqr(_transform(i, d));
        ret.val(i) = std::sqrt(sum);
      }
      return ret;
    }
    static constexpr auto zeroValue() noexcept {
      if constexpr (is_vec<value_type>::value)
        return value_type::zeros();
      else
        return (value_type)0;
    }

    /// @brief maintenance
    AdaptiveGridImpl() = default;
    ~AdaptiveGridImpl() = default;

    AdaptiveGridImpl clone(const allocator_type &allocator) const {
      AdaptiveGridImpl ret{};
      (void)((get<Is>(ret._levels) = get<Is>(_levels).clone(allocator)), ...);
      ret._transform = _transform;
      ret._background = _background;
      return ret;
    }
    AdaptiveGridImpl clone(const zs::MemoryLocation &mloc) const {
      return clone(get_default_allocator(mloc.memspace(), mloc.devid()));
    }

    /// @brief transformation
    template <typename VecTM,
              enable_if_all<VecTM::dim == 2, VecTM::template range_t<0>::value == dim + 1,
                            VecTM::template range_t<1>::value == dim + 1,
                            is_floating_point_v<typename VecTM::value_type>>
              = 0>
    void resetTransformation(const VecInterface<VecTM> &i2w) {
      _transform.self() = i2w;
    }
    auto getIndexToWorldTransformation() const { return _transform.self(); }
    template <typename VecT, enable_if_all<VecT::dim == 1, VecT::extent == dim> = 0>
    void translate(const VecInterface<VecT> &t) noexcept {
      _transform.postTranslate(t);
    }
    template <typename VecT, enable_if_all<VecT::dim == 2, VecT::template range_t<0>::value == dim,
                                           VecT::template range_t<1>::value == dim>
                             = 0>
    void rotate(const VecInterface<VecT> &r) noexcept {
      _transform.preRotate(Rotation<typename VecT::value_type, dim>{r});
    }
    template <typename VecT, enable_if_all<VecT::dim == 1, VecT::extent == dim> = 0>
    void scale(const VecInterface<VecT> &s) {
      _transform.preScale(s);
    }
    void scale(const value_type s) { scale(s * coord_type::constant(1)); }

    zs::tuple<Level<Is>...> _levels;
    transform_type _transform;
    value_type _background;  // background value
  };

  // special construct for blockwise access (with halos)

}  // namespace zs