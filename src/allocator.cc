#include "allocator.h"
#include <algorithm>

namespace PixelForge {

FrameBufferAllocator::FrameBufferAllocator(size_t maxCapacity) : m_maxCapacity(maxCapacity) {}

FrameBufferAllocator::~FrameBufferAllocator() {
    reset();
}

void FrameBufferAllocator::releaseImage(Image* img) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& entry : m_pool) {
        if (entry.image.get() == img) {
            entry.active = false;
            return;
        }
    }
    // Fallback if not found in pool
    delete img;
}

std::shared_ptr<Image> FrameBufferAllocator::acquire(uint32_t width, uint32_t height, PixelFormat format) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Look for a matching inactive image in the pool
    for (auto& entry : m_pool) {
        if (!entry.active) {
            if (entry.image->getFormat() == format) {
                entry.image->allocate(width, height, format);
                entry.active = true;
                
                std::weak_ptr<FrameBufferAllocator> weak_alloc = shared_from_this();
                Image* rawImgPtr = entry.image.get();
                return std::shared_ptr<Image>(rawImgPtr, [weak_alloc](Image* img) {
                    if (auto alloc = weak_alloc.lock()) {
                        alloc->releaseImage(img);
                    }
                });
            }
        }
    }

    // Allocate a new image if pool is not full
    auto newImg = std::make_shared<Image>();
    newImg->allocate(width, height, format);

    if (m_pool.size() < m_maxCapacity) {
        m_pool.push_back(PoolEntry{newImg, true});
        std::weak_ptr<FrameBufferAllocator> weak_alloc = shared_from_this();
        return std::shared_ptr<Image>(newImg.get(), [weak_alloc](Image* img) {
            if (auto alloc = weak_alloc.lock()) {
                alloc->releaseImage(img);
            }
        });
    }

    // Return unpooled image if pool capacity exceeded
    return newImg;
}

size_t FrameBufferAllocator::getActiveCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& entry : m_pool) {
        if (entry.active) count++;
    }
    return count;
}

size_t FrameBufferAllocator::getFreeCount() {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& entry : m_pool) {
        if (!entry.active) count++;
    }
    return count;
}

void FrameBufferAllocator::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.clear();
}

} // namespace PixelForge
