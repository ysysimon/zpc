#pragma once
#include "zensim/memory/Allocator.h"
#include "zensim/resource/Resource.h"
#include "zensim/tpls/magic_enum.hpp"
#include "zensim/types/Iterator.h"
#include "zensim/types/RuntimeStructurals.hpp"

namespace zs {

  template <typename Snode, typename Index = std::size_t> using vector_snode
      = ds::snode_t<ds::static_decorator<>, ds::uniform_domain<0, Index, 1, index_seq<0>>,
                    tuple<Snode>, vseq_t<1>>;
  template <typename T, typename Index = std::size_t> using vector_instance
      = ds::instance_t<ds::dense, vector_snode<wrapt<T>, Index>>;

#define VEC_ITER_OP(IterT, OP) \
  constexpr bool operator OP(const IterT &o) const noexcept { return _idx OP o._idx; }

  template <typename T>
  struct Vector : Inherit<Object, Vector<T>>, vector_instance<T>, MemoryHandle {
    /// according to rule of 5(6)/0
    /// is_trivial<T> has to be true
    static_assert(std::is_default_constructible_v<T> && std::is_trivially_copyable_v<T>,
                  "element is not default-constructible or trivially-copyable!");
    using base_t = vector_instance<T>;
    using value_type = remove_cvref_t<T>;
    using allocator_type = typename umpire::strategy::MixedPool;
    using pointer = value_type *;
    using const_pointer = const pointer;
    using reference = value_type &;
    using const_reference = const T &;
    using size_type = typename std::size_t;
    using difference_type = std::make_signed_t<size_type>;
    using iterator_category = std::random_access_iterator_tag;  // std::contiguous_iterator_tag;

    constexpr MemoryHandle &base() noexcept { return static_cast<MemoryHandle &>(*this); }
    constexpr const MemoryHandle &base() const noexcept {
      return static_cast<const MemoryHandle &>(*this);
    }
    constexpr base_t &self() noexcept { return static_cast<base_t &>(*this); }
    constexpr const base_t &self() const noexcept { return static_cast<const base_t &>(*this); }

    constexpr Vector(memsrc_e mre = memsrc_e::host, ProcID devid = -1, std::size_t alignment = 0)
        : MemoryHandle{mre, devid},
          base_t{buildInstance(mre, devid, 0)},
          _size{0},
          _align{alignment} {}
    Vector(size_type count, memsrc_e mre = memsrc_e::host, ProcID devid = -1,
           std::size_t alignment = 0)
        : MemoryHandle{mre, devid},
          base_t{buildInstance(mre, devid, count + count / 2)},
          _size{count},
          _align{alignment} {}

    ~Vector() {
      if (head()) self().dealloc();
    }

    struct iterator : IteratorInterface<iterator> {
      constexpr iterator(const base_t &range, size_type idx) : _range{range}, _idx{idx} {}

      constexpr reference dereference() { return _range(_idx); }
      constexpr bool equal_to(iterator it) const noexcept { return it._idx == _idx; }
      constexpr void advance(difference_type offset) noexcept { _idx += offset; }
      constexpr difference_type distance_to(iterator it) const noexcept { return it._idx - _idx; }

    protected:
      base_t _range{};
      size_type _idx{0};
    };
    struct const_iterator : IteratorInterface<const_iterator> {
      constexpr const_iterator(const base_t &range, size_type idx) : _range{range}, _idx{idx} {}

      constexpr const_reference dereference() { return _range(_idx); }
      constexpr bool equal_to(const_iterator it) const noexcept { return it._idx == _idx; }
      constexpr void advance(difference_type offset) noexcept { _idx += offset; }
      constexpr difference_type distance_to(const_iterator it) const noexcept {
        return it._idx - _idx;
      }

    protected:
      base_t _range{};
      size_type _idx{0};
    };

    constexpr auto begin() noexcept { return make_iterator<iterator>(self(), 0); }
    constexpr auto end() noexcept { return make_iterator<iterator>(self(), size()); }
    constexpr auto begin() const noexcept { return make_iterator<const_iterator>(self(), 0); }
    constexpr auto end() const noexcept { return make_iterator<const_iterator>(self(), size()); }

    void debug() const {
      fmt::print("procid: {}, memspace: {}, size: {}, capacity: {}\n", static_cast<int>(devid()),
                 static_cast<int>(memspace()), size(), capacity());
    }

    /// capacity
    constexpr size_type size() const noexcept { return _size; }
    constexpr size_type capacity() const noexcept { return self().node().extent(); }
    constexpr bool empty() noexcept { return size() == 0; }
    constexpr pointer head() const noexcept { return reinterpret_cast<pointer>(self().address()); }
    constexpr pointer tail() const noexcept { return reinterpret_cast<pointer>(head() + size()); }

    /// element access
    constexpr reference operator[](size_type idx) noexcept { return self()(idx); }
    constexpr conditional_t<std::is_fundamental_v<value_type>, value_type, const_reference>
    operator[](size_type idx) const noexcept {
      return self()(idx);
    }
    /// ctor, assignment operator
    explicit Vector(const Vector &o) : MemoryHandle{o.base()}, _size{o.size()} {
      auto &rm = get_resource_manager().self();
      base_t tmp{buildInstance(o.memspace(), o.devid(), o.capacity())};
      if (o.size()) rm.copy((void *)tmp.address(), o.head(), o.usedBytes());
      self() = tmp;
    }
    Vector &operator=(const Vector &o) {
      if (this == &o) return *this;
      Vector tmp{o};
      swap(tmp);
      return *this;
    }
    /// assignment or destruction after std::move
    /// https://www.youtube.com/watch?v=ZG59Bqo7qX4
    /// explicit noexcept
    /// leave the source object in a valid (default constructed) state
    explicit Vector(Vector &&o) noexcept {
      const Vector defaultVector{};
      base() = std::exchange(o.base(), defaultVector.base());
      self() = std::exchange(o.self(), defaultVector.self());
      _size = std::exchange(o._size, defaultVector.size());
    }
    /// make move-assignment safe for self-assignment
    Vector &operator=(Vector &&o) noexcept {
      if (this == &o) return *this;
      Vector tmp{std::move(o)};
      swap(tmp);
      return *this;
    }
    void swap(Vector &o) noexcept {
      base().swap(o.base());
      std::swap(self(), o.self());
      std::swap(_size, o._size);
    }

    // constexpr operator base_t &() noexcept { return self(); }
    // constexpr operator base_t() const noexcept { return self(); }
    // void relocate(memsrc_e mre, ProcID devid) {}
    void clear() { resize(0); }
    void resize(size_type newSize) {
      const auto oldSize = size();
      if (newSize < oldSize) {
        if constexpr (!std::is_trivially_destructible_v<T>) {
          static_assert(!std::is_trivial_v<T>, "should not activate this scope");
          pointer ed = tail();
          for (pointer e = head() + newSize; e < ed; ++e) e->~T();
        }
        _size = newSize;
        return;
      }
      if (newSize > oldSize) {
        const auto oldCapacity = capacity();
        if (newSize > oldCapacity) {
          auto &rm = get_resource_manager().self();
          if (devid() != -1) {
            base_t tmp{buildInstance(memspace(), devid(), geometric_size_growth(newSize))};
            if (size()) rm.copy((void *)tmp.address(), (void *)head(), usedBytes());
            if (oldCapacity > 0) rm.deallocate((void *)head());

            self() = tmp;
          } else {
            /// expect this to throw if failed
            this->assign(rm.reallocate((void *)this->address(),
                                       sizeof(T) * geometric_size_growth(newSize),
                                       getCurrentAllocator()));
          }
          _size = newSize;
          return;
        }
      }
    }

    void push_back(const value_type &val) {
      if (size() >= capacity()) resize(size() + 1);
      (*this)(_size++) = val;
    }
    void push_back(value_type &&val) {
      if (size() >= capacity()) resize(size() + 1);
      (*this)(_size++) = std::move(val);
    }

    template <typename InputIter> iterator append(InputIter st, InputIter ed) {
      // difference_type count = std::distance(st, ed); //< def standard iterator
      difference_type count = ed - st;
      if (count <= 0) return end();
      auto &rm = get_resource_manager().self();
      size_type unusedCapacity = capacity() - size();
      // this is not optimal
      if (count > unusedCapacity) resize(size() + count);
      rm.copy(&end(), &(*st), sizeof(T) * count);
    }
    constexpr const_pointer data() const noexcept { return (pointer)head(); }
    constexpr pointer data() noexcept { return (pointer)head(); }
    constexpr reference front() noexcept { return (*this)(0); }
    constexpr const_reference front() const noexcept { (*this)(0); }
    constexpr reference back() noexcept { return (*this)(size() - 1); }
    constexpr const_reference back() const noexcept { (*this)(size() - 1); }

  protected:
    constexpr size_type usedBytes() const noexcept { return sizeof(T) * size(); }

    constexpr auto buildInstance(memsrc_e mre, ProcID devid, size_type capacity) {
      using namespace ds;
      constexpr auto dec = ds::static_decorator{};
      uniform_domain<0, size_type, 1, index_seq<0>> dom{wrapv<0>{}, capacity};
      vector_snode<wrapt<T>> node{dec, dom, zs::make_tuple(wrapt<T>{}), vseq_t<1>{}};
      auto inst = instance{wrapv<dense>{}, zs::make_tuple(node)};

      if (capacity) {
        auto memorySource = get_resource_manager().source(mre);
        if (mre == memsrc_e::um) memorySource = memorySource.advisor("PREFERRED_LOCATION", devid);
        /// additional parameters should match allocator_type
        inst.template alloc<allocator_type>(
            memorySource, 1 << 8, 1 << 17, 2ull << 20, 16, 512ull << 20, 1 << 10,
            _align > inst.maxAlignment() ? _align : inst.maxAlignment());
      }
      return inst;
    }
    constexpr std::size_t geometric_size_growth(std::size_t newSize) noexcept {
      size_type geometricSize = capacity();
      geometricSize = geometricSize + geometricSize / 2;
      if (newSize > geometricSize) return newSize;
      return geometricSize;
    }
    constexpr GeneralAllocator getCurrentAllocator() {
      auto memorySource = get_resource_manager().source(this->memspace());
      if (this->memspace() == memsrc_e::um)
        memorySource = memorySource.advisor("PREFERRED_LOCATION", this->devid());
      return memorySource.template allocator<allocator_type>(
          1 << 8, 1 << 17, 2ull << 20, 16, 512ull << 20, 1 << 10,
          _align > this->maxAlignment() ? _align : this->maxAlignment());
    }

    size_type _size{0};  // size
    size_type _align{0};
  };

}  // namespace zs