#pragma once

#include "image.h"
#include <vector>
#include <memory>
#include <mutex>

namespace PixelForge {

class FrameBufferAllocator : public std::enable_shared_from_this<FrameBufferAllocator> {
private:
    size_t m_maxCapacity;
    std::mutex m_mutex;

    struct PoolEntry {
        std::shared_ptr<Image> image;
        bool active;
    };
    std::vector<PoolEntry> m_pool;

    void releaseImage(Image* img);

public:
    explicit FrameBufferAllocator(size_t maxCapacity);
    ~FrameBufferAllocator();

    static std::shared_ptr<FrameBufferAllocator> create(size_t maxCapacity) {
        return std::make_shared<FrameBufferAllocator>(maxCapacity);
    }

    std::shared_ptr<Image> acquire(uint32_t width, uint32_t height, PixelFormat format);

    size_t getActiveCount();
    size_t getFreeCount();
    void reset();
};

} // namespace PixelForge
