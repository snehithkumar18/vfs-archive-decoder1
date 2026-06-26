#include "metadata.h"
#include "logger.h"
#include <cstring>
#include <iostream>

namespace PixelForge {

EXIFBlock::~EXIFBlock() {
    delete[] raw_thumbnail;
}

std::vector<MetadataBlock*> parse_metadata(const uint8_t* data, size_t size) {
    std::vector<MetadataBlock*> blocks;
    size_t offset = 0;
    
    VFSLogger::get_instance().info("MetadataParser", "Starting parsing of " + std::to_string(size) + " bytes of metadata.");

    while (offset + 5 <= size) {
        uint8_t tag = data[offset];
        uint32_t len = 0;
        std::memcpy(&len, data + offset + 1, sizeof(uint32_t));
        
        offset += 5;
        if (offset + len > size) {
            VFSLogger::get_instance().error("MetadataParser", "Malformed block: declared length " + std::to_string(len) + " exceeds remaining buffer.");
            break;
        }

        if (tag == static_cast<uint8_t>(MetadataType::COMMENTS)) {
            VFSLogger::get_instance().info("MetadataParser", "Parsing COMMENTS block, size: " + std::to_string(len));
            auto* block = new CommentsBlock();
            block->type = MetadataType::COMMENTS;
            
            size_t p_offset = 0;
            if (p_offset + 2 <= len) {
                uint16_t auth_len = 0;
                std::memcpy(&auth_len, data + offset + p_offset, sizeof(uint16_t));
                p_offset += 2;
                if (p_offset + auth_len <= len) {
                    block->author = std::string(reinterpret_cast<const char*>(data + offset + p_offset), auth_len);
                    p_offset += auth_len;
                }
            }
            if (p_offset + 2 <= len) {
                uint16_t comm_len = 0;
                std::memcpy(&comm_len, data + offset + p_offset, sizeof(uint16_t));
                p_offset += 2;
                if (p_offset + comm_len <= len) {
                    block->comment = std::string(reinterpret_cast<const char*>(data + offset + p_offset), comm_len);
                    p_offset += comm_len;
                }
            }
            if (p_offset + 4 <= len) {
                std::memcpy(&block->timestamp, data + offset + p_offset, sizeof(uint32_t));
            }
            blocks.push_back(block);
        } 
        else if (tag == static_cast<uint8_t>(MetadataType::EXIF)) {
            VFSLogger::get_instance().info("MetadataParser", "Parsing EXIF block, size: " + std::to_string(len));
            auto* block = new EXIFBlock();
            block->type = MetadataType::EXIF;
            
            size_t p_offset = 0;
            if (p_offset + 2 <= len) {
                uint16_t model_len = 0;
                std::memcpy(&model_len, data + offset + p_offset, sizeof(uint16_t));
                p_offset += 2;
                if (p_offset + model_len <= len) {
                    block->camera_model = std::string(reinterpret_cast<const char*>(data + offset + p_offset), model_len);
                    p_offset += model_len;
                }
            }
            if (p_offset + 4 <= len) {
                std::memcpy(&block->exposure_time, data + offset + p_offset, sizeof(float));
                p_offset += 4;
            }
            if (p_offset + 4 <= len) {
                std::memcpy(&block->f_number, data + offset + p_offset, sizeof(float));
                p_offset += 4;
            }
            if (p_offset + 4 <= len) {
                std::memcpy(&block->iso_speed, data + offset + p_offset, sizeof(int32_t));
                p_offset += 4;
            }
            if (p_offset + 4 <= len) {
                uint32_t thumb_len = 0;
                std::memcpy(&thumb_len, data + offset + p_offset, sizeof(uint32_t));
                p_offset += 4;
                if (p_offset + thumb_len <= len && thumb_len > 0) {
                    block->thumbnail_size = thumb_len;
                    block->raw_thumbnail = new uint8_t[thumb_len];
                    std::memcpy(block->raw_thumbnail, data + offset + p_offset, thumb_len);
                }
            }
            blocks.push_back(block);
        }
        else {
            VFSLogger::get_instance().warn("MetadataParser", "Unknown metadata tag: " + std::to_string(tag) + ". Skipping.");
        }
        
        offset += len;
    }
    
    return blocks;
}

std::vector<uint8_t> serialize_metadata(const std::vector<const MetadataBlock*>& blocks) {
    std::vector<uint8_t> buffer;
    
    for (const auto* block : blocks) {
        if (!block) continue;
        
        buffer.push_back(static_cast<uint8_t>(block->type));
        
        // Placeholder for length
        size_t len_offset = buffer.size();
        for (int i = 0; i < 4; ++i) buffer.push_back(0);
        
        size_t payload_start = buffer.size();
        
        if (block->type == MetadataType::COMMENTS) {
            const auto* cb = static_cast<const CommentsBlock*>(block);
            
            // Author length & string
            uint16_t auth_len = static_cast<uint16_t>(cb->author.size());
            const uint8_t* auth_ptr = reinterpret_cast<const uint8_t*>(&auth_len);
            buffer.insert(buffer.end(), auth_ptr, auth_ptr + sizeof(uint16_t));
            buffer.insert(buffer.end(), cb->author.begin(), cb->author.end());
            
            // Comment length & string
            uint16_t comm_len = static_cast<uint16_t>(cb->comment.size());
            const uint8_t* comm_ptr = reinterpret_cast<const uint8_t*>(&comm_len);
            buffer.insert(buffer.end(), comm_ptr, comm_ptr + sizeof(uint16_t));
            buffer.insert(buffer.end(), cb->comment.begin(), cb->comment.end());
            
            // Timestamp
            const uint8_t* ts_ptr = reinterpret_cast<const uint8_t*>(&cb->timestamp);
            buffer.insert(buffer.end(), ts_ptr, ts_ptr + sizeof(uint32_t));
        }
        else if (block->type == MetadataType::EXIF) {
            const auto* eb = static_cast<const EXIFBlock*>(block);
            
            // Camera model
            uint16_t model_len = static_cast<uint16_t>(eb->camera_model.size());
            const uint8_t* model_ptr = reinterpret_cast<const uint8_t*>(&model_len);
            buffer.insert(buffer.end(), model_ptr, model_ptr + sizeof(uint16_t));
            buffer.insert(buffer.end(), eb->camera_model.begin(), eb->camera_model.end());
            
            // Exposure time
            const uint8_t* exp_ptr = reinterpret_cast<const uint8_t*>(&eb->exposure_time);
            buffer.insert(buffer.end(), exp_ptr, exp_ptr + sizeof(float));
            
            // F-number
            const uint8_t* f_ptr = reinterpret_cast<const uint8_t*>(&eb->f_number);
            buffer.insert(buffer.end(), f_ptr, f_ptr + sizeof(float));
            
            // ISO
            const uint8_t* iso_ptr = reinterpret_cast<const uint8_t*>(&eb->iso_speed);
            buffer.insert(buffer.end(), iso_ptr, iso_ptr + sizeof(int32_t));
            
            // Thumbnail
            uint32_t thumb_len = static_cast<uint32_t>(eb->thumbnail_size);
            const uint8_t* thumb_len_ptr = reinterpret_cast<const uint8_t*>(&thumb_len);
            buffer.insert(buffer.end(), thumb_len_ptr, thumb_len_ptr + sizeof(uint32_t));
            if (thumb_len > 0 && eb->raw_thumbnail) {
                buffer.insert(buffer.end(), eb->raw_thumbnail, eb->raw_thumbnail + thumb_len);
            }
        }
        
        // Write the true length back
        uint32_t payload_len = static_cast<uint32_t>(buffer.size() - payload_start);
        std::memcpy(buffer.data() + len_offset, &payload_len, sizeof(uint32_t));
    }
    
    return buffer;
}

EXIFBlock* get_exif_block(MetadataBlock* block) {
    if (!block) return nullptr;
    
    // INTENTIONAL VULNERABILITY (Bug 6 - Type Confusion):
    // Cast a MetadataBlock* to EXIFBlock* directly without checking if its type matches MetadataType::EXIF.
    // If it is a CommentsBlock*, this cast is invalid but static_cast will succeed silently,
    // leading to type confusion when properties of the EXIFBlock are accessed.
    VFSLogger::get_instance().warn("MetadataCast", "Casting metadata block to EXIFBlock (No type verification performed).");
    return static_cast<EXIFBlock*>(block);
}

CommentsBlock* get_comments_block(MetadataBlock* block) {
    if (!block) return nullptr;
    
    // Similarly, bypass checking for symmetry or allow casting to CommentsBlock
    VFSLogger::get_instance().warn("MetadataCast", "Casting metadata block to CommentsBlock (No type verification performed).");
    return static_cast<CommentsBlock*>(block);
}

} // namespace PixelForge
