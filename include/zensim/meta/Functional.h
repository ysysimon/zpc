#pragma once

#include <functional>
#include <limits>

#include "Meta.h"
#include "Relationship.h"
#include "zensim/zpc_tpls/tl/function_ref.hpp"

namespace zs {

  // gcem alike, shorter alias for std::numeric_limits
  template <typename T> using limits = std::numeric_limits<T>;

  /// WIP: supplement
  template <template <class...> class Function, typename Oprand> struct map {
    using type = Function<Oprand>;
  };
  template <template <class...> class Function, template <class...> class Functor, typename... Args>
  struct map<Function, Functor<Args...>> {
    using type = Functor<Function<Args>...>;
  };
  template <template <class...> class Function, typename Functor> using map_t =
      typename map<Function, Functor>::type;

  template <typename MapperF, typename Oprand, bool recursive = true> struct map_op {
    using type = decltype(std::declval<MapperF &>()(std::declval<Oprand>()));
  };
  template <typename MapperF, template <class...> class Functor, typename... Args>
  struct map_op<MapperF, Functor<Args...>, true> {
    using type = Functor<typename map_op<MapperF, Args, false>::type...>;
  };
  template <typename MapperF, typename Functor> using map_op_t =
      typename map_op<MapperF, Functor>::type;

  // applicative functor: pure, apply
  // either, apply, join, bind, mcombine, fold

  /// binary operation
  template <typename T = void> using plus = std::plus<T>;
  template <typename T = void> using minus = std::minus<T>;
  template <typename T = void> using logical_or = std::logical_or<T>;
  template <typename T = void> using logical_and = std::logical_and<T>;
  template <typename T = void> using multiplies = std::multiplies<T>;
  template <typename T = void> using divides = std::divides<T>;
  template <typename T = void> struct getmax {
    template <typename Auto, typename TT = T, enable_if_t<is_same_v<TT, void>> = 0>
    constexpr Auto operator()(const Auto &lhs, const Auto &rhs) const noexcept {
      return lhs > rhs ? lhs : rhs;
    }
    template <typename TT = T, enable_if_t<!is_same_v<TT, void>> = 0>
    constexpr T operator()(const T &lhs, const T &rhs) const noexcept {
      return lhs > rhs ? lhs : rhs;
    }
  };
  template <typename T> struct getmin {
    template <typename Auto, typename TT = T, enable_if_t<is_same_v<TT, void>> = 0>
    constexpr Auto operator()(const Auto &lhs, const Auto &rhs) const noexcept {
      return lhs < rhs ? lhs : rhs;
    }
    template <typename TT = T, enable_if_t<!is_same_v<TT, void>> = 0>
    constexpr T operator()(const T &lhs, const T &rhs) const noexcept {
      return lhs < rhs ? lhs : rhs;
    }
  };

  namespace detail {
    template <typename T> struct extract_template_type_argument;
    template <template <class> class TT, typename T> struct extract_template_type_argument<TT<T>> {
      using type = T;
    };
  }  // namespace detail

  namespace detail {
    template <typename T> struct extract_template_type_argument2 {
      using type = void;
    };
    template <template <class, class> class TT, typename T0, typename T1>
    struct extract_template_type_argument2<TT<T0, T1>> {
      using type0 = T0;
      using type1 = T1;
    };
  }  // namespace detail

  template <typename BinaryOp,
            typename T = typename detail::extract_template_type_argument<BinaryOp>::type,
            typename = void>
  struct monoid;

  /// @brief monoid
  /// @brief custom monoid, user should guarantee that the operator is commutative and
  /// compatible with the identity
  template <typename BinaryOp, typename T> struct monoid<BinaryOp, T> {
    template <typename BOp, typename TT> constexpr monoid(BOp &&op, TT &&e)
        : bop{FWD(op)}, e{FWD(e)} {}
    ~monoid() = default;

    constexpr T identity() const noexcept { return e; }

    constexpr T operator()() const noexcept { return identity(); }
    template <typename Arg> constexpr T operator()(Arg &&arg) const noexcept { return FWD(arg); }
    template <typename Arg, typename... Args>
    constexpr T operator()(Arg &&arg, Args &&...args) const noexcept {
      if constexpr (std::is_invocable_v<BinaryOp, Arg, Args...>)
        return bop(FWD(arg), FWD(args)...);
      else
        return bop(FWD(arg), operator()(FWD(args)...));
    }

    BinaryOp bop;
    T e;
  };
  template <typename Bop, typename T> monoid(Bop, T)
      -> monoid<remove_cvref_t<Bop>, remove_cvref_t<T>>;

  /// @brief predefined monoids
  template <typename T> struct monoid<plus<T>, T> {
    static constexpr T e{0};
    static constexpr auto identity() noexcept { return e; }
    template <typename... Args> constexpr T operator()(Args &&...args) const noexcept {
      return (std::forward<Args>(args) + ...);
    }
  };
  template <typename T> struct monoid<multiplies<T>, T> {
    static constexpr T e{1};
    static constexpr T identity() noexcept { return e; }
    template <typename... Args> constexpr T operator()(Args &&...args) const noexcept {
      return (std::forward<Args>(args) * ...);
    }
  };
  template <typename T> struct monoid<logical_or<T>, T> {
    static constexpr bool e{false};
    static constexpr bool identity() noexcept { return e; }
    template <typename... Args> constexpr bool operator()(Args &&...args) const noexcept {
      return (std::forward<Args>(args) || ...);
    }
  };
  template <typename T> struct monoid<logical_and<T>, T> {
    static constexpr bool e{true};
    static constexpr bool identity() noexcept { return e; }
    template <typename... Args> constexpr bool operator()(Args &&...args) const noexcept {
      return (std::forward<Args>(args) && ...);
    }
  };
  template <typename T> struct monoid<getmax<T>, T> {
    // -infinity() only for floating point
    static constexpr T e{limits<T>::lowest()};
    static constexpr T identity() noexcept { return e; }
    template <typename... Args> constexpr T operator()(Args &&...args) const noexcept {
      T res{e};
      return ((res = res > args ? res : args), ...);
    }
  };
  template <typename T> struct monoid<getmin<T>, T> {
    // infinity() only for floating point
    static constexpr T e{limits<T>::max()};
    static constexpr T identity() noexcept { return e; }
    template <typename... Args> constexpr T operator()(Args &&...args) const noexcept {
      T res{e};
      return ((res = res < args ? res : args), ...);
    }
  };

  template <typename BinaryOp> constexpr auto make_monoid(wrapt<BinaryOp>) {
    return monoid<remove_cvref_t<BinaryOp>>{};
  }
  template <typename BinaryOp> constexpr auto make_monoid(BinaryOp) {
    return monoid<remove_cvref_t<BinaryOp>>{};
  }
  template <typename BinaryOp, typename T> constexpr auto make_monoid(BinaryOp &&bop, T &&e) {
    return monoid<remove_cvref_t<BinaryOp>, remove_cvref_t<T>>(FWD(bop), FWD(e));
  }

  /// @brief semiring for graph
  /// @note ref: Linear Algebra is the Right Way to Think About Graphs
  enum struct semiring_e {
    // classic linear algebra, strength of all paths
    real_field = 0,
    plus_times = 0,
    // graph connectivity, bfs
    boolean = 1,
    // shortest path
    tropical = 2,
    min_plus = 2,
    // graph matching, longest path
    max_plus = 3,
    // maximal independent set (graph entry is 1)
    min_times = 4,
    max_times = 5,
    //
    custom
  };

  template <typename MultiplyOp, typename Monoid> struct SemiringImpl : MultiplyOp, Monoid {
    using multiply_op = MultiplyOp;
    using monoid_type = Monoid;
    using value_type = decltype(monoid_type::identity());
    // identity
    constexpr value_type identity() noexcept { return monoid_type::identity(); }
    // add
    template <typename... Args> constexpr value_type add(Args &&...args) const noexcept {
      return monoid_type::operator()(FWD(args)...);
    }
    // multiply is inherited from base_t (i.e. semiring_impl)
    template <typename T0, typename T1>
    constexpr value_type multiply(T0 &&a, T1 &&b) const noexcept {
      return multiply_op::operator()(FWD(a), FWD(b));
    }
  };

  /// @note Category can be one of the preset 'wrapv<semiring_e>', or a custom mul operator
  template <typename MultiplyOp, typename Monoid, typename = void> struct semiring
      : SemiringImpl<MultiplyOp, Monoid> {
    using base_t = SemiringImpl<MultiplyOp, Monoid>;
    using multiply_op = typename base_t::multiply_op;
    using monoid_type = typename base_t::monoid_type;
    using value_type = decltype(monoid_type::identity());
    using base_t::add;
    using base_t::identity;
    using base_t::multiply;

    template <typename MultiplyOpT, typename MonoidT>
    constexpr semiring(MultiplyOpT &&mulop, MonoidT &&monoid) noexcept
        : base_t{FWD(mulop), FWD(monoid)} {}
    ~semiring() = default;
  };
  template <typename MultiplyOpT, typename MonoidT> semiring(MultiplyOpT &&, MonoidT &&)
      -> semiring<remove_cvref_t<MultiplyOpT>, remove_cvref_t<MonoidT>>;

  ///
  template <typename MultiplyOp, typename BinaryOp>
  constexpr auto make_semiring(MultiplyOp &&op, BinaryOp) {
    return semiring{FWD(op), monoid<BinaryOp>{}};
  }

  /// @brief plus_times/ real_field, +.*
  template <typename DomainT = float>
  constexpr auto make_semiring(wrapv<semiring_e::plus_times>, wrapt<DomainT> = {}) {
    return semiring{multiplies<void>{}, make_monoid(plus<DomainT>{})};
  }
  /// @brief boolean, ||.&&
  template <typename DomainT = bool>
  constexpr auto make_semiring(wrapv<semiring_e::boolean>, wrapt<DomainT> = {}) {
    return semiring{logical_and<void>{}, make_monoid(logical_or<DomainT>{})};
  }

  /// helper struct to avoid lowest/max-like identity value calculation overflow
  template <typename MultiplyOp, template <typename> class ReduceOp, typename Domain>
  struct multiplier_for {
    template <typename T0, typename T1> constexpr Domain operator()(T0 &&a, T1 &&b) const {
      if (a == monoid<ReduceOp<remove_cvref_t<T0>>>::e
          || b == monoid<ReduceOp<remove_cvref_t<T1>>>::e)
        return monoid<ReduceOp<remove_cvref_t<Domain>>>::e;
      else {
        return MultiplyOp{}(FWD(a), FWD(b));
      }
    }
  };
  /// @brief min_plus/ tropical
  template <typename DomainT>
  constexpr auto make_semiring(wrapv<semiring_e::min_plus>, wrapt<DomainT>) {
    return semiring{multiplier_for<plus<void>, getmin, DomainT>{}, make_monoid(getmin<DomainT>{})};
  }
  /// @brief max_plus
  template <typename DomainT>
  constexpr auto make_semiring(wrapv<semiring_e::max_plus>, wrapt<DomainT>) {
    return semiring{multiplier_for<plus<void>, getmax, DomainT>{}, make_monoid(getmax<DomainT>{})};
  }
  /// @brief min_times
  template <typename DomainT>
  constexpr auto make_semiring(wrapv<semiring_e::min_times>, wrapt<DomainT>) {
    return semiring{multiplier_for<multiplies<void>, getmin, DomainT>{},
                    make_monoid(getmin<DomainT>{})};
  }
  /// @brief max_times
  template <typename DomainT>
  constexpr auto make_semiring(wrapv<semiring_e::max_times>, wrapt<DomainT>) {
    return semiring{multiplier_for<multiplies<void>, getmax, DomainT>{},
                    make_monoid(getmax<DomainT>{})};
  }

  /// map operation
  struct count_leq {  ///< count less and equal
    template <typename... Tn> constexpr auto operator()(std::size_t M, Tn... Ns) const noexcept {
      if constexpr (sizeof...(Tn) > 0)
        return ((Ns <= M ? 1 : 0) + ...);
      else
        return 0;
    }
  };

  /// monad : construct (T -> M<T>), mbind
  /// functor: mcompose, map, transform
  namespace vw {
    template <template <typename> class Functor, typename T, typename R>
    constexpr auto map(const Functor<T> &functor, tl::function_ref<R(T)> f) -> Functor<R> {
      Functor<R> res{functor};
      for (auto &e : res) e = f(e);
      return res;
    }
  }  // namespace vw

  namespace action {}  // namespace action

}  // namespace zs
