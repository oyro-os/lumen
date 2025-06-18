#ifndef LUMEN_MEMORY_ALLOCATOR_H
#define LUMEN_MEMORY_ALLOCATOR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace lumen {

// Memory allocation categories for tracking and debugging
enum class AllocationCategory : uint8_t {
    General = 0,
    Page = 1,
    Index = 2,
    Buffer = 3,
    Metadata = 4,
    Transaction = 5,
    Cache = 6,
    Vector = 7,
    Temporary = 8
};

// Memory allocator interface
class Allocator {
   public:
    virtual ~Allocator() = default;

    // Basic allocation/deallocation
    virtual void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) = 0;
    virtual void deallocate(void* ptr, size_t size) = 0;

    // Categorized allocation for tracking
    virtual void* allocate(size_t size, AllocationCategory category,
                           size_t alignment = alignof(std::max_align_t)) {
        return allocate(size, alignment);
    }

    // Bulk operations
    virtual void* allocate_bulk(size_t count, size_t size,
                                size_t alignment = alignof(std::max_align_t)) {
        return allocate(count * size, alignment);
    }

    virtual void deallocate_bulk(void* ptr, size_t count, size_t size) {
        deallocate(ptr, count * size);
    }

    // Memory statistics
    virtual size_t allocated_size() const = 0;
    virtual size_t peak_allocated_size() const = 0;
    virtual size_t allocation_count() const = 0;
};

// System allocator using malloc/free
class SystemAllocator : public Allocator {
   public:
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size) override;
    size_t allocated_size() const override {
        return allocated_size_;
    }
    size_t peak_allocated_size() const override {
        return peak_allocated_size_;
    }
    size_t allocation_count() const override {
        return allocation_count_;
    }

   private:
    std::atomic<size_t> allocated_size_{0};
    std::atomic<size_t> peak_allocated_size_{0};
    std::atomic<size_t> allocation_count_{0};
};

// Allocator wrapper for specific allocator implementations
#ifdef LUMEN_USE_MIMALLOC
class MimallocAllocator : public Allocator {
   public:
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size) override;
    size_t allocated_size() const override;
    size_t peak_allocated_size() const override;
    size_t allocation_count() const override;
};
#endif

#ifdef LUMEN_USE_JEMALLOC
class JemallocAllocator : public Allocator {
   public:
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size) override;
    size_t allocated_size() const override;
    size_t peak_allocated_size() const override;
    size_t allocation_count() const override;
};
#endif

#ifdef LUMEN_USE_TCMALLOC
class TcmallocAllocator : public Allocator {
   public:
    void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) override;
    void deallocate(void* ptr, size_t size) override;
    size_t allocated_size() const override;
    size_t peak_allocated_size() const override;
    size_t allocation_count() const override;
};
#endif

// Global allocator instance
Allocator* get_allocator();
void set_allocator(std::unique_ptr<Allocator> allocator);

// Convenience allocation functions
inline void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
    return get_allocator()->allocate(size, alignment);
}

inline void* allocate(size_t size, AllocationCategory category,
                      size_t alignment = alignof(std::max_align_t)) {
    return get_allocator()->allocate(size, category, alignment);
}

inline void deallocate(void* ptr, size_t size) {
    get_allocator()->deallocate(ptr, size);
}

// STL-compatible allocator
template<typename T>
class StlAllocator {
   public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    StlAllocator() = default;
    template<typename U>
    StlAllocator(const StlAllocator<U>&) noexcept {}

    T* allocate(size_type n) {
        if (n > std::size_t(-1) / sizeof(T)) {
            throw std::bad_alloc();
        }
        void* ptr = get_allocator()->allocate(n * sizeof(T), alignof(T));
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, size_type n) noexcept {
        get_allocator()->deallocate(ptr, n * sizeof(T));
    }

    template<typename U>
    bool operator==(const StlAllocator<U>&) const noexcept {
        return true;
    }

    template<typename U>
    bool operator!=(const StlAllocator<U>&) const noexcept {
        return false;
    }
};

// Memory pool for fixed-size allocations
template<size_t BlockSize, size_t BlocksPerChunk = 256>
class MemoryPool {
   public:
    static constexpr size_t block_size = BlockSize;
    static constexpr size_t blocks_per_chunk = BlocksPerChunk;

    MemoryPool();
    ~MemoryPool();

    void* allocate();
    void deallocate(void* ptr);

    size_t allocated_blocks() const {
        return allocated_blocks_;
    }
    size_t total_blocks() const {
        return total_blocks_;
    }

   private:
    struct Block {
        Block* next;
    };

    struct Chunk {
        Chunk* next;
        char data[BlockSize * BlocksPerChunk];
    };

    Block* free_list_{nullptr};
    Chunk* chunk_list_{nullptr};
    size_t allocated_blocks_{0};
    size_t total_blocks_{0};
};

// Aligned allocation helper
template<typename T>
T* allocate_aligned(size_t count = 1, size_t alignment = alignof(T)) {
    void* ptr = allocate(sizeof(T) * count, alignment);
    return static_cast<T*>(ptr);
}

template<typename T>
void deallocate_aligned(T* ptr, size_t count = 1) {
    deallocate(ptr, sizeof(T) * count);
}

}  // namespace lumen

#endif  // LUMEN_MEMORY_ALLOCATOR_H
