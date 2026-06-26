#include "allocator.h"
#include <cstdlib>
#include <cstring>
#include <iostream>

/*
 * ============================================================================
 * SLAB/CHUNK NODE ALLOCATOR DESIGN DOCUMENTATION
 * ============================================================================
 * This slab allocator is optimized for high-frequency, uniform-sized allocations
 * typical of filesystem nodes (FileNode, DirectoryNode, CacheNode).
 *
 * Design Principles:
 * 1. Memory Arena Pre-allocation: Re-uses large contiguous blocks of virtual memory
 *    (slabs) divided into equal-sized chunks. This eliminates heap fragmentation
 *    and simplifies fast allocation/deallocation lookup.
 * 2. 8-byte Alignment Guard: All requested sizes are padded to the nearest 8-byte
 *    boundary. This guarantees hardware compatibility across modern x86_64/ARM
 *    architectures and prevents alignment faults during cast operations.
 * 3. Slab Growth: If no active slab of the requested chunk size has free slots,
 *    a new SlabBlock of default capacity is dynamically allocated on the system
 *    heap and added to the slab pool.
 * 4. Deallocation Lookup: Compares pointer boundaries to resolve the home slab
 *    and clears the usage bitmask.
 * 5. Mutex Synchronization: Thread-safe locks ensure operations are atomic across
 *    concurrent VFS commands.
 * ============================================================================
 */

VFSNodeAllocator::VFSNodeAllocator() : default_slab_chunks(64) {}

VFSNodeAllocator::~VFSNodeAllocator() {
    reset();
}

void VFSNodeAllocator::create_new_slab(size_t chunk_size, size_t num_chunks) {
    uint8_t* memory = (uint8_t*)std::malloc(chunk_size * num_chunks);
    bool* used = new bool[num_chunks];
    std::memset(used, 0, sizeof(bool) * num_chunks);
    
    slabs.push_back(SlabBlock{memory, used, chunk_size, num_chunks});
}

void* VFSNodeAllocator::allocate(size_t size) {
    std::lock_guard<std::mutex> lock(allocator_mutex);
    
    // Aligns to 8 bytes
    size_t chunk_size = (size + 7) & ~7;
    
    // Find a slab with matching chunk_size and free capacity
    for (auto& slab : slabs) {
        if (slab.chunk_size == chunk_size) {
            for (size_t i = 0; i < slab.num_chunks; ++i) {
                if (!slab.used[i]) {
                    slab.used[i] = true;
                    return slab.memory + (i * chunk_size);
                }
            }
        }
    }
    
    // No matching slab has space, create a new one
    create_new_slab(chunk_size, default_slab_chunks);
    auto& slab = slabs.back();
    slab.used[0] = true;
    return slab.memory;
}

void VFSNodeAllocator::deallocate(void* ptr) {
    if (!ptr) return;
    
    std::lock_guard<std::mutex> lock(allocator_mutex);
    
    uint8_t* p = static_cast<uint8_t*>(ptr);
    for (auto& slab : slabs) {
        // Check if ptr is inside the boundaries of this slab
        uint8_t* slab_start = slab.memory;
        uint8_t* slab_end = slab.memory + (slab.chunk_size * slab.num_chunks);
        
        if (p >= slab_start && p < slab_end) {
            size_t offset = p - slab_start;
            size_t index = offset / slab.chunk_size;
            
            if (index < slab.num_chunks) {
                slab.used[index] = false;
                return;
            }
        }
    }
}

void VFSNodeAllocator::reset() {
    std::lock_guard<std::mutex> lock(allocator_mutex);
    
    for (auto& slab : slabs) {
        std::free(slab.memory);
        delete[] slab.used;
    }
    slabs.clear();
}

size_t VFSNodeAllocator::get_active_slabs_count() const {
    return slabs.size();
}
