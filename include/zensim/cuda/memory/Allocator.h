#pragma once
#include <any>
#include <string_view>
#include <unordered_map>

#include "MemOps.hpp"
#include "zensim/Singleton.h"
#include "zensim/memory/Allocator.h"
#include "zensim/memory/MemoryResource.h"
#include "zensim/types/Property.h"

namespace zs {

  template <> struct raw_memory_resource<device_mem_tag> : mr_t {
  private:
    raw_memory_resource() noexcept;

  public:
    using value_type = std::byte;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = true_type;
    using propagate_on_container_copy_assignment = true_type;
    using propagate_on_container_swap = true_type;

    ZPC_BACKEND_API static raw_memory_resource &instance();
    ~raw_memory_resource() = default;

    void *do_allocate(size_t bytes, size_t alignment) override {
      if (bytes) {
        auto ret = zs::allocate(mem_device, bytes, alignment);
        // record_allocation(MemTag{}, ret, demangle(*this), bytes, alignment);
        return ret;
      }
      return nullptr;
    }
    void do_deallocate(void *ptr, size_t bytes, size_t alignment) override {
      if (bytes) {
        zs::deallocate(mem_device, ptr, bytes, alignment);
        // erase_allocation(ptr);
      }
    }
    bool do_is_equal(const mr_t &other) const noexcept override { return this == &other; }
  };

  template <> struct temporary_memory_resource<device_mem_tag> : mr_t {
    using value_type = std::byte;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;

    temporary_memory_resource(void *c = nullptr, void *s = nullptr) : context{c}, stream{s} {}

    void *do_allocate(std::size_t bytes, std::size_t alignment) override;
    void do_deallocate(void *ptr, std::size_t bytes, std::size_t alignment) override;
    bool do_is_equal(const mr_t &other) const noexcept override { return this == &other; }

    void *context{nullptr};
    void *stream{nullptr};
  };

  template <> struct raw_memory_resource<um_mem_tag> : mr_t {
  private:
    raw_memory_resource() noexcept;

  public:
    using value_type = std::byte;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = true_type;
    using propagate_on_container_copy_assignment = true_type;
    using propagate_on_container_swap = true_type;

    ZPC_BACKEND_API static raw_memory_resource &instance();
    ~raw_memory_resource() = default;

    void *do_allocate(size_t bytes, size_t alignment) override {
      if (bytes) {
        auto ret = zs::allocate(mem_um, bytes, alignment);
        // record_allocation(MemTag{}, ret, demangle(*this), bytes, alignment);
        return ret;
      }
      return nullptr;
    }
    void do_deallocate(void *ptr, size_t bytes, size_t alignment) override {
      if (bytes) {
        zs::deallocate(mem_um, ptr, bytes, alignment);
        // erase_allocation(ptr);
      }
    }
    bool do_is_equal(const mr_t &other) const noexcept override { return this == &other; }
  };

  template <typename MemTag> struct stack_virtual_memory_resource;
  template <typename MemTag> struct arena_virtual_memory_resource;

#if 0
  // disable this impl for now
  template <> struct stack_virtual_memory_resource<device_mem_tag> : mr_t {
    stack_virtual_memory_resource(ProcID did = 0, std::string_view type = "DEVICE_PINNED");
    ~stack_virtual_memory_resource();
    void *do_allocate(size_t bytes, size_t alignment) override;
    void do_deallocate(void *ptr, size_t bytes, size_t alignment) override;
    bool do_is_equal(const mr_t &other) const noexcept override { return this == &other; }

    bool reserve(size_t desiredSpace);

    std::vector<std::pair<unsigned long long, zs::size_t>> _vaRanges;
    std::vector<unsigned long long> _allocHandles;
    std::vector<std::pair<void *, zs::size_t>> _allocationRanges;
    std::string _type;
    std::any _allocProp;
    std::any _accessDescr;
    size_t _granularity;
    void *_addr;
    size_t _offset, _reservedSpace, _allocatedSpace;
    ProcID _did;
  };
#else
  template <> struct stack_virtual_memory_resource<device_mem_tag> : vmr_t {
    stack_virtual_memory_resource(ProcID did = 0, size_t size = vmr_t::s_chunk_granularity);
    ~stack_virtual_memory_resource();

    bool do_check_residency(size_t offset, size_t bytes) const override;
    bool do_commit(size_t offset, size_t bytes) override;
    bool do_evict(size_t offset, size_t bytes) override;
    void *do_address(size_t offset) const override {
      return static_cast<void *>(static_cast<char *>(_addr) + offset);
    }

    void *do_allocate(size_t bytes, size_t alignment) override;
    void do_deallocate(void *ptr, size_t bytes, size_t alignment) override;
    bool do_is_equal(const mr_t &other) const noexcept override { return this == &other; }

    std::vector<unsigned long long> _allocHandles;
    std::vector<std::pair<zs::size_t, zs::size_t>> _allocRanges;
    std::any _allocProp;
    std::any _accessDescr;
    size_t _granularity;
    void *_addr;
    size_t _reservedSpace, _allocatedSpace;
    ProcID _did;
  };
#endif

  template <> struct arena_virtual_memory_resource<device_mem_tag>
      : vmr_t {  // default impl falls back to
    /// 2MB chunk granularity
    static constexpr size_t s_chunk_granularity_bits = vmr_t::s_chunk_granularity_bits;
    static constexpr size_t s_chunk_granularity = vmr_t::s_chunk_granularity;

    arena_virtual_memory_resource(ProcID did = -1, size_t space = s_chunk_granularity);
    ~arena_virtual_memory_resource();
    bool do_check_residency(size_t offset, size_t bytes) const override;
    bool do_commit(size_t offset, size_t bytes) override;
    bool do_evict(size_t offset, size_t bytes) override;
    void *do_address(size_t offset) const override {
      return static_cast<void *>(static_cast<char *>(_addr) + offset);
    }

    void *do_allocate(size_t /*bytes*/, size_t /*alignment*/) override { return _addr; }

    std::any _allocProp;
    std::any _accessDescr;
    size_t _granularity;
    const size_t _reservedSpace;
    void *_addr;
    std::vector<u64> _activeChunkMasks;
    std::unordered_map<zs::size_t, unsigned long long> _allocations;  // <offset, handle>
    ProcID _did;
  };

}  // namespace zs