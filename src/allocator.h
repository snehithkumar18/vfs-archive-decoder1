#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>

struct SlabBlock {
    uint8_t* memory;
    bool* used;
    size_t chunk_size;
    size_t num_chunks;
};

class VFSNodeAllocator {
private:
    std::mutex allocator_mutex;
    std::vector<SlabBlock> slabs;
    size_t default_slab_chunks;
    
    void create_new_slab(size_t chunk_size, size_t num_chunks);

public:
    VFSNodeAllocator();
    ~VFSNodeAllocator();

    void* allocate(size_t size);
    void deallocate(void* ptr);
    void reset();
    
    size_t get_active_slabs_count() const;
};

#endif // ALLOCATOR_H
