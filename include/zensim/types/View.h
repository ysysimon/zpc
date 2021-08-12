#pragma once
#include <zensim/types/SmallVector.hpp>

#include "Property.h"
#include "zensim/meta/Meta.h"

namespace zs {

  ///
  /// dof traits for dofview
  ///
  template <execspace_e space, typename Structure, typename = void> struct dof_traits {
    struct dof_detail {
      template <typename _Tp> using value_t = typename _Tp::value_type;
      template <typename _Tp> using T_t = typename _Tp::T;
      template <typename _Tp> using size_t = typename _Tp::size_type;
      template <typename _Tp> using index_t = typename _Tp::index_t;
      template <typename _Tp> using counter_t = typename _Tp::channel_counter_type;
      template <typename _Tp> using dim_t = typename std::integral_constant<int, _Tp::dim>;
      template <typename _Tp> using extent_t = typename std::integral_constant<int, _Tp::extent>;
    };

    using structure_view_t = decltype(proxy<space>(std::declval<Structure>()));
    using structure_type = remove_cvref_t<Structure>;
    using value_type = detected_or_t<detected_or_t<float, dof_detail::T_t, structure_type>,
                                     dof_detail::value_t, structure_type>;
    using size_type = detected_or_t<detected_or_t<std::size_t, dof_detail::index_t, structure_type>,
                                    dof_detail::size_t, structure_type>;
    using channel_counter_type
        = detected_or_t<unsigned char, dof_detail::counter_t, structure_type>;
    static constexpr attrib_e entry_e
        = std::is_arithmetic_v<value_type> ? attrib_e::scalar : attrib_e::vector;
    static constexpr int deduced_dim = detected_or_t<
        detected_or_t<std::integral_constant<int, 1>, dof_detail::extent_t, value_type>,
        dof_detail::dim_t, structure_type>::value;

    /// access by entry index
    template <typename svt, enable_if_t<is_same_v<svt, structure_view_t>> = 0>
    static constexpr auto get(svt obj, size_type i)
        -> std::enable_if_t<std::is_convertible_v<RM_CVREF_T(obj(i)), value_type>, value_type> {
      return obj(i);
    }
    template <typename svt, enable_if_t<is_same_v<svt, structure_view_t>> = 0>
    static constexpr auto ref(svt obj, size_type i) -> std::enable_if_t<
        std::is_reference_v<decltype(obj(i))> && is_same_v<decltype(get(obj, i)), value_type>,
        decltype(obj(i))> {
      return obj(i);
    }
    template <typename svt, enable_if_t<is_same_v<svt, structure_view_t>> = 0>
    static constexpr auto set(svt obj, size_type i, const value_type& v)
        -> std::enable_if_t<std::is_assignable_v<decltype(ref(obj, i)), value_type>> {
      ref(obj, i) = v;
    }

    /// access by channel and entry index
    template <typename svt, enable_if_t<is_same_v<svt, structure_view_t>> = 0>
    static constexpr auto get(svt obj, channel_counter_type chn, size_type i)
        -> std::enable_if_t<std::is_convertible_v<RM_CVREF_T(obj(chn, i)), value_type>,
                            value_type> {
      return obj(chn, i);
    }
    template <typename svt, enable_if_t<is_same_v<svt, structure_view_t>> = 0>
    static constexpr auto ref(svt obj, channel_counter_type chn, size_type i)
        -> std::enable_if_t<std::is_reference_v<decltype(obj(
                                chn, i))> && is_same_v<decltype(get(obj, chn, i)), value_type>,
                            decltype(obj(chn, i))> {
      return obj(chn, i);
    }
    template <typename svt, enable_if_t<is_same_v<svt, structure_view_t>> = 0>
    static constexpr auto set(svt obj, channel_counter_type chn, size_type i, const value_type& v)
        -> std::enable_if_t<std::is_assignable_v<decltype(ref(obj, chn, i)), value_type>> {
      ref(obj, chn, i) = v;
    }
  };

  template <execspace_e space, typename Structure, int dim_ = 3, bool WithChannel = false>
  struct DofView {
    using structure_t = Structure;
    using structure_view_t = decltype(proxy<space>(std::declval<Structure>()));
    using traits = dof_traits<space, structure_t>;
    using value_type = typename traits::value_type;
    using size_type = typename traits::size_type;
    using channel_counter_type = typename traits::channel_counter_type;
    static constexpr auto entry_e = traits::entry_e;
    static constexpr int dim = dim_;

    static_assert(entry_e == attrib_e::scalar
                      || (entry_e == attrib_e::vector && dim == traits::deduced_dim),
                  "dofview dim and entry dim conflicts!");
    static_assert(entry_e == attrib_e::scalar || entry_e == attrib_e::vector,
                  "dofview entry can only be a scalar or a vector");

    DofView(wrapv<space>, Structure structure, wrapv<dim> = {})
        : _structure{proxy<space>(structure)}, _size{structure.size()} {}

    structure_view_t _structure;
    size_type _size;

    constexpr size_type size() const noexcept { return _size; }
    constexpr size_type numEntries() const noexcept {
      return entry_e == attrib_e::scalar ? _size : _size * dim;
    }
    constexpr decltype(auto) ref(size_type i) { return traits::ref(_structure, i); }

    struct iterator_impl : IteratorInterface<iterator_impl> {
      using traits = typename DofView::traits;
      using size_type = typename DofView::size_type;
      using difference_type = std::make_signed_t<size_type>;
      static constexpr auto entry_e = DofView::entry_e;
      static constexpr int dim = DofView::dim;

      constexpr iterator_impl(structure_view_t structure, size_type i)
          : _structure{structure}, _idx{i} {}

      // reference to a value_type
      constexpr decltype(auto) dereference() {
        if constexpr (entry_e == attrib_e::scalar)
          return traits::ref(_structure, _idx);
        else if constexpr (entry_e == attrib_e::vector)
          return traits::ref(_structure, _idx / dim)[_idx % dim];
      }
      constexpr bool equal_to(iterator_impl it) const noexcept { return it._idx == _idx; }
      constexpr void advance(difference_type offset) noexcept { _idx += offset; }
      constexpr difference_type distance_to(iterator_impl it) const noexcept {
        return it._idx - _idx;
      }

    protected:
      structure_view_t _structure;
      size_type _idx{0};
    };
    using iterator = LegacyIterator<iterator_impl>;
    constexpr auto begin() const noexcept { return make_iterator<iterator_impl>(_structure, 0); }
    constexpr auto end() const noexcept {
      return make_iterator<iterator_impl>(_structure, numEntries());
    }

    template <attrib_e AccessEntry = entry_e>
    constexpr auto get(size_type i, wrapv<AccessEntry> = {}) const {
      if constexpr (AccessEntry == entry_e)
        return traits::get(_structure, i);
      else if (AccessEntry == attrib_e::scalar && entry_e == attrib_e::vector)
        return traits::get(_structure, i / dim)[i % dim];
      else if (AccessEntry == attrib_e::vector && entry_e == attrib_e::scalar) {
        vec<value_type, dim> ret{};
        const size_type base = i * dim;
        for (int d = 0; d != dim; ++d) ret(d) = traits::get(_structure, base + d);
        return ret;
      }
    }
    template <typename V> constexpr auto set(size_type i, V&& v = {})
        -> std::enable_if_t<std::is_lvalue_reference_v<decltype(ref(i))>> {
      if constexpr (std::is_assignable_v<decltype(ref(i)), V>) traits::set(_structure, i, FWD(v));
      // V is a scalar
      else if (std::is_arithmetic_v<remove_cvref_t<V>> && entry_e == attrib_e::vector)
        ref(i / dim)[i % dim] = FWD(v);
      // V is a vector
      else if (!std::is_arithmetic_v<remove_cvref_t<V>> && entry_e == attrib_e::scalar) {
        if constexpr (remove_cvref_t<V>::extent == dim) {
          const size_type base = i * dim;
          for (int d = 0; d != dim; ++d) ref(base + d) = v[d];
        }
      }
    }
  };

  template <execspace_e space, typename Structure, int dim_>
  struct DofView<space, Structure, dim_, true> {
    using structure_t = Structure;
    using structure_view_t = decltype(proxy<space>(std::declval<Structure>()));
    using traits = dof_traits<space, structure_t>;
    using value_type = typename traits::value_type;
    using size_type = typename traits::size_type;
    using channel_counter_type = typename traits::channel_counter_type;
    static constexpr auto entry_e = traits::entry_e;
    static constexpr int dim = dim_;

    static_assert(entry_e == attrib_e::scalar
                      || (entry_e == attrib_e::vector && dim == traits::deduced_dim),
                  "dofview dim and entry dim conflicts!");
    static_assert(entry_e == attrib_e::scalar || entry_e == attrib_e::vector,
                  "dofview entry can only be a scalar or a vector");

    DofView(wrapv<space>, Structure structure, channel_counter_type chn, wrapv<dim> = {})
        : _structure{proxy<space>(structure)}, _size{structure.size()}, _chn{chn} {}

    structure_view_t _structure;
    size_type _size;
    channel_counter_type _chn;

    constexpr size_type size() const noexcept { return _size; }
    constexpr size_type numEntries() const noexcept { return _size * dim; }
    constexpr decltype(auto) ref(size_type i) { return traits::ref(_structure, _chn, i); }

    struct iterator_impl : IteratorInterface<iterator_impl> {
      using traits = typename DofView::traits;
      using size_type = typename DofView::size_type;
      using difference_type = std::make_signed_t<size_type>;
      static constexpr auto entry_e = DofView::entry_e;
      static constexpr int dim = DofView::dim;

      constexpr iterator_impl(structure_view_t structure, channel_counter_type chn, size_type i)
          : _structure{structure}, _idx{i}, _chn{chn} {}

      // reference to a value_type
      constexpr decltype(auto) dereference() {
        if constexpr (entry_e == attrib_e::scalar)
          return traits::ref(_structure, _chn + _idx % dim, _idx / dim);
        else if constexpr (entry_e == attrib_e::vector)
          return traits::ref(_structure, _chn, _idx / dim)[_idx % dim];
      }
      constexpr bool equal_to(iterator_impl it) const noexcept { return it._idx == _idx; }
      constexpr void advance(difference_type offset) noexcept { _idx += offset; }
      constexpr difference_type distance_to(iterator_impl it) const noexcept {
        return it._idx - _idx;
      }

    protected:
      structure_view_t _structure;
      size_type _idx{0};
      channel_counter_type _chn{};
    };
    using iterator = LegacyIterator<iterator_impl>;
    constexpr auto begin() const noexcept {
      return make_iterator<iterator_impl>(_structure, _chn, 0);
    }
    constexpr auto end() const noexcept {
      return make_iterator<iterator_impl>(_structure, _chn, numEntries());
    }

    template <attrib_e AccessEntry = entry_e>
    constexpr value_type get(size_type i, wrapv<AccessEntry> = {}) const {
      if constexpr (AccessEntry == entry_e)
        return traits::get(_structure, _chn, i);
      else if (AccessEntry == attrib_e::scalar && entry_e == attrib_e::vector)
        return traits::get(_structure, _chn, i / dim)[i % dim];
      else if (AccessEntry == attrib_e::vector && entry_e == attrib_e::scalar) {
        // different
        vec<value_type, dim> ret{};
        for (int d = 0; d != dim; ++d) ret(d) = traits::get(_structure, _chn + d, i);
        return ret;
      }
    }
    template <typename V> constexpr auto set(size_type i, V&& v = {})
        -> std::enable_if_t<std::is_lvalue_reference_v<decltype(ref(i))>> {
      if constexpr (std::is_assignable_v<decltype(ref(i)), V>)
        traits::set(_structure, _chn, i, FWD(v));
      // V is a scalar
      else if (std::is_arithmetic_v<remove_cvref_t<V>> && entry_e == attrib_e::vector)
        ref(i / dim)[i % dim] = FWD(v);
      // V is a vector
      else if (!std::is_arithmetic_v<remove_cvref_t<V>> && entry_e == attrib_e::scalar)
        if constexpr (remove_cvref_t<V>::extent == dim) {
          /// more complex strategy
          if constexpr (traits::deduced_dim == dim)
            for (int d = 0; d != dim; ++d) traits::ref(_structure, _chn + d, i) = v[d];
          else {
            const size_type base = i * dim;
            for (int d = 0; d != dim; ++d) ref(base + d) = v[d];
          }
        }
    }
  };

  ///
  template <execspace_e space, typename T> constexpr decltype(auto) dof_view(T&& t) {
    return DofView<space, T, dof_traits<space, T>::deduced_dim>{wrapv<space>{}, t};
  }
  template <execspace_e space, int dim, typename T> constexpr decltype(auto) dof_view(T&& t) {
    return DofView<space, T, dim>{wrapv<space>{}, t};
  }

  template <execspace_e space, typename T>
  constexpr decltype(auto) dof_view(T&& t, typename dof_traits<space, T>::channel_counter_type c) {
    return DofView<space, T, dof_traits<space, T>::deduced_dim, true>{wrapv<space>{}, t, c};
  }
  template <execspace_e space, int dim, typename T>
  constexpr decltype(auto) dof_view(T&& t, typename dof_traits<space, T>::channel_counter_type c) {
    return DofView<space, T, dim, true>{wrapv<space>{}, t, c};
  }

  template <execspace_e space, typename T>
  constexpr decltype(auto) dof_view(T&& t, const SmallString& str) {
    return DofView<space, T, dof_traits<space, T>::deduced_dim, true>{wrapv<space>{}, t,
                                                                      t.getChannelOffset(str)};
  }
  template <execspace_e space, int dim, typename T>
  constexpr decltype(auto) dof_view(T&& t, const SmallString& str) {
    return DofView<space, T, dim, true>{wrapv<space>{}, t, t.getChannelOffset(str)};
  }

}  // namespace zs