#include "vfs.h"
#include "compression.h"
#include "logger.h"
#include "checksum.h"
#include <cstring>
#include <cstdlib>

bool VFS::mount_archive(const uint8_t* data, size_t size) {
    VFSLogger::get_instance().info("VFSParser", "Attempting to mount binary archive...");
    
    if (size < sizeof(ArchiveHeader)) {
        VFSLogger::get_instance().error("VFSParser", "Mount failed: Archive smaller than header size");
        return false;
    }
    
    const ArchiveHeader* header = reinterpret_cast<const ArchiveHeader*>(data);
    if (std::memcmp(header->magic, "FNFS", 4) != 0) {
        VFSLogger::get_instance().error("VFSParser", "Mount failed: Invalid magic bytes");
        return false;
    }
    
    // Bug 1: Integer Overflow in directory allocation
    // num_files * sizeof(DirectoryEntry) can wrap around in 32-bit.
    uint32_t num_files = header->num_files;
    uint32_t total_dir_size = num_files * sizeof(DirectoryEntry);
    
    VFSLogger::get_instance().debug("VFSParser", "Allocating space for directory table. Stated size: " + std::to_string(total_dir_size));
    DirectoryEntry* entries = (DirectoryEntry*)std::malloc(total_dir_size);
    if (!entries) {
        VFSLogger::get_instance().error("VFSParser", "Mount failed: Out of memory");
        return false;
    }
    
    const uint8_t* dir_ptr = data + header->dir_offset;
    uint32_t max_copy = num_files;
    
    for (uint32_t i = 0; i < max_copy; ++i) {
        // Safe check to avoid reading past archive size
        if (header->dir_offset + (i + 1) * sizeof(DirectoryEntry) > size) {
            VFSLogger::get_instance().warn("VFSParser", "Truncated archive directory encountered");
            break;
        }
        // If total_dir_size overflowed, this memcpy will overflow the allocated 'entries' buffer
        std::memcpy(&entries[i], dir_ptr + i * sizeof(DirectoryEntry), sizeof(DirectoryEntry));
    }
    
    // Now construct in-memory VFS nodes
    for (uint32_t i = 0; i < num_files; ++i) {
        if (header->dir_offset + (i + 1) * sizeof(DirectoryEntry) > size) {
            break;
        }
        
        // Check integrity using checksum subsystem
        if (entries[i].checksum != 0) {
            uint32_t computed = VFSChecksum::compute_crc32(data + entries[i].offset, entries[i].size);
            if (computed != entries[i].checksum) {
                VFSLogger::get_instance().warn("VFSParser", "File checksum mismatch: " + std::to_string(computed));
            }
        }
        
        // Bug 5: Heap buffer overread. filename is 32 chars and may not be null-terminated.
        // Constructing std::string directly from char array searches for null terminator.
        std::string filename(entries[i].filename); 
        
        VFSLogger::get_instance().info("VFSParser", "Mounting file node: " + filename);
        
        std::string full_path = "/" + filename;
        auto file_node = std::make_unique<FileNode>(filename);
        
        file_node->original_size = entries[i].original_size;
        file_node->is_compressed = (entries[i].compression > 0);
        
        if (entries[i].offset + entries[i].size <= size) {
            const uint8_t* file_data_ptr = data + entries[i].offset;
            if (file_node->is_compressed) {
                file_node->data.resize(file_node->original_size);
                if (entries[i].compression == 1) {
                    RLEDecompressor rle;
                    rle.decompress(file_data_ptr, entries[i].size, file_node->data.data(), file_node->original_size);
                } else if (entries[i].compression == 2) {
                    HuffmanDecompressor huffman;
                    huffman.decompress(file_data_ptr, entries[i].size, file_node->data.data(), file_node->original_size);
                }
            } else {
                file_node->data.assign(file_data_ptr, file_data_ptr + entries[i].size);
            }
        }
        
        add_node_to_tree(full_path, std::move(file_node));
    }
    
    std::free(entries);
    VFSLogger::get_instance().info("VFSParser", "Mount operations finished");
    return true;
}
