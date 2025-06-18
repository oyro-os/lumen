#include <lumen/memory/allocator.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <stdexcept>

#ifdef _WIN32
#include <malloc.h>
#else
#include <stdlib.h>
#endif

// Include allocator headers if available
#ifdef LUMEN_USE_MIMALLOC
#include <mimalloc.h>
#endif

#ifdef LUMEN_USE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif

#ifdef LUMEN_USE_TCMALLOC
#include <gperftools/tcmalloc.h>
#endif

namespace lumen {

namespace {
std::unique_ptr<Allocator> g_allocator;
std::once_flag g_allocator_init_flag;

void initialize_default_allocator() {
#ifdef LUMEN_USE_MIMALLOC
    g_allocator = std::make_unique<MimallocAllocator>();
#elif defined(LUMEN_USE_JEMALLOC)
    g_allocator = std::make_unique<JemallocAllocator>();
#elif defined(LUMEN_USE_TCMALLOC)
    g_allocator = std::make_unique<TcmallocAllocator>();
#else
    g_allocator = std::make_unique<SystemAllocator>();
#endif
}
}  // namespace

// System allocator implementation
void* SystemAllocator::allocate(size_t size, size_t alignment) {
    if (size == 0) {
        return nullptr;
    }

    void* ptr = nullptr;

    // posix_memalign requires alignment to be a power of 2 and at least sizeof(void*)
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }

    // Ensure alignment is power of 2
    alignment =
        alignment ? (1 << (sizeof(size_t) * 8 - __builtin_clzl(alignment - 1))) : sizeof(void*);

#ifdef _WIN32
    ptr = _aligned_malloc(size, alignment);
#else
    if (posix_memalign(&ptr, alignment, size) != 0) {
        ptr = nullptr;
    }
#endif

    if (ptr) {
        allocated_size_ += size;
        allocation_count_++;

        size_t current = allocated_size_.load();
        size_t peak = peak_allocated_size_.load();
        while (current > peak && !peak_allocated_size_.compare_exchange_weak(peak, current)) {
            // Keep trying until we update the peak
        }
    }

    return ptr;
}

void SystemAllocator::deallocate(void* ptr, size_t size) {
    if (!ptr) {
        return;
    }

#ifdef _WIN32
    _aligned_free(ptr);
#else
    free(ptr);
#endif

    allocated_size_ -= size;
}

#ifdef LUMEN_USE_MIMALLOC
void* MimallocAllocator::allocate(size_t size, size_t alignment) {
    return mi_malloc_aligned(size, alignment);
}

void MimallocAllocator::deallocate(void* ptr, size_t) {
    mi_free(ptr);
}

size_t MimallocAllocator::allocated_size() const {
    // Get stats from mimalloc
    size_t elapsed_msecs;
    size_t user_msecs;
    size_t system_msecs;
    size_t current_rss;
    size_t peak_rss;
    size_t current_commit;
    size_t peak_commit;
    size_t page_faults;
    mi_process_info(&elapsed_msecs, &user_msecs, &system_msecs, &current_rss, &peak_rss,
                    &current_commit, &peak_commit, &page_faults);
    return current_commit;
}

size_t MimallocAllocator::peak_allocated_size() const {
    size_t elapsed_msecs;
    size_t user_msecs;
    size_t system_msecs;
    size_t current_rss;
    size_t peak_rss;
    size_t current_commit;
    size_t peak_commit;
    size_t page_faults;
    mi_process_info(&elapsed_msecs, &user_msecs, &system_msecs, &current_rss, &peak_rss,
                    &current_commit, &peak_commit, &page_faults);
    return peak_commit;
}

size_t MimallocAllocator::allocation_count() const {
    // mimalloc doesn't directly expose allocation count
    return 0;
}
#endif

#ifdef LUMEN_USE_JEMALLOC
void* JemallocAllocator::allocate(size_t size, size_t alignment) {
    return je_aligned_alloc(alignment, size);
}

void JemallocAllocator::deallocate(void* ptr, size_t) {
    je_free(ptr);
}

size_t JemallocAllocator::allocated_size() const {
    size_t allocated;
    size_t sz = sizeof(size_t);
    je_mallctl("stats.allocated", &allocated, &sz, nullptr, 0);
    return allocated;
}

size_t JemallocAllocator::peak_allocated_size() const {
    // jemalloc doesn't directly track peak allocation
    return allocated_size();
}

size_t JemallocAllocator::allocation_count() const {
    // jemalloc doesn't directly expose allocation count
    return 0;
}
#endif

#ifdef LUMEN_USE_TCMALLOC
void* TcmallocAllocator::allocate(size_t size, size_t alignment) {
    return tc_memalign(alignment, size);
}

void TcmallocAllocator::deallocate(void* ptr, size_t) {
    tc_free(ptr);
}

size_t TcmallocAllocator::allocated_size() const {
    size_t value;
    MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes", &value);
    return value;
}

size_t TcmallocAllocator::peak_allocated_size() const {
    size_t value;
    MallocExtension::instance()->GetNumericProperty("generic.peak_allocated_bytes", &value);
    return value;
}

size_t TcmallocAllocator::allocation_count() const {
    // tcmalloc doesn't directly expose allocation count
    return 0;
}
#endif

// Global allocator management
Allocator* get_allocator() {
    std::call_once(g_allocator_init_flag, initialize_default_allocator);
    return g_allocator.get();
}

void set_allocator(std::unique_ptr<Allocator> allocator) {
    if (!allocator) {
        throw std::invalid_argument("Cannot set null allocator");
    }
    g_allocator = std::move(allocator);
}

// Memory pool implementation
template<size_t BlockSize, size_t BlocksPerChunk>
MemoryPool<BlockSize, BlocksPerChunk>::MemoryPool() = default;

template<size_t BlockSize, size_t BlocksPerChunk>
MemoryPool<BlockSize, BlocksPerChunk>::~MemoryPool() {
    // Free all chunks
    Chunk* chunk = chunk_list_;
    while (chunk) {
        Chunk* next = chunk->next;
        lumen::deallocate(chunk, sizeof(Chunk));
        chunk = next;
    }
}

template<size_t BlockSize, size_t BlocksPerChunk>
void* MemoryPool<BlockSize, BlocksPerChunk>::allocate() {
    // If free list is empty, allocate a new chunk
    if (!free_list_) {
        Chunk* new_chunk = static_cast<Chunk*>(lumen::allocate(sizeof(Chunk)));
        new_chunk->next = chunk_list_;
        chunk_list_ = new_chunk;

        // Initialize free list with blocks from new chunk
        char* block_ptr = new_chunk->data;
        for (size_t i = 0; i < blocks_per_chunk; ++i) {
            Block* block = reinterpret_cast<Block*>(block_ptr);
            block->next = free_list_;
            free_list_ = block;
            block_ptr += block_size;
        }

        total_blocks_ += blocks_per_chunk;
    }

    // Pop from free list
    Block* block = free_list_;
    free_list_ = block->next;
    allocated_blocks_++;

    return block;
}

template<size_t BlockSize, size_t BlocksPerChunk>
void MemoryPool<BlockSize, BlocksPerChunk>::deallocate(void* ptr) {
    if (!ptr) {
        return;
    }

    // Add to free list
    Block* block = static_cast<Block*>(ptr);
    block->next = free_list_;
    free_list_ = block;
    allocated_blocks_--;
}

// Explicit instantiations for common pool sizes
template class MemoryPool<64, 256>;
template class MemoryPool<128, 256>;
template class MemoryPool<256, 128>;
template class MemoryPool<512, 64>;
template class MemoryPool<1024, 32>;
template class MemoryPool<4096, 8>;

}  // namespace lumen