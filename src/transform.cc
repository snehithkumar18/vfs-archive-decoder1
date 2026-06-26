#include "transform.h"
#include "logger.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace PixelForge {

PixelForgeErrorCode Transform::FlipHorizontal(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t src_idx = (static_cast<size_t>(y) * w + x) * ch;
            size_t dst_idx = (static_cast<size_t>(y) * w + (w - 1 - x)) * ch;
            std::memcpy(dst_data.data() + dst_idx, src_data.data() + src_idx, ch);
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Transform::FlipVertical(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    for (uint32_t y = 0; y < h; ++y) {
        size_t src_row_offset = static_cast<size_t>(y) * w * ch;
        size_t dst_row_offset = static_cast<size_t>(h - 1 - y) * w * ch;
        std::memcpy(dst_data.data() + dst_row_offset, src_data.data() + src_row_offset, w * ch);
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Transform::Rotate90(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(h, w, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t src_idx = (static_cast<size_t>(y) * w + x) * ch;
            size_t dst_idx = (static_cast<size_t>(x) * h + (h - 1 - y)) * ch;
            std::memcpy(dst_data.data() + dst_idx, src_data.data() + src_idx, ch);
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Transform::Rotate180(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    size_t total_pixels = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < total_pixels; ++i) {
        size_t src_idx = i * ch;
        size_t dst_idx = (total_pixels - 1 - i) * ch;
        std::memcpy(dst_data.data() + dst_idx, src_data.data() + src_idx, ch);
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Transform::Rotate270(const Image& src, Image& dst) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(h, w, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t src_idx = (static_cast<size_t>(y) * w + x) * ch;
            size_t dst_idx = (static_cast<size_t>(w - 1 - x) * h + y) * ch;
            std::memcpy(dst_data.data() + dst_idx, src_data.data() + src_idx, ch);
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Transform::Rotate(const Image& src, Image& dst, float angle) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    float rad = angle * 3.14159265f / 180.0f;
    float cos_a = std::cos(rad);
    float sin_a = std::sin(rad);

    float fw = static_cast<float>(w);
    float fh = static_cast<float>(h);

    // Calculate new dimensions to fit the rotated image
    float x1 = -fw/2.0f * cos_a - -fh/2.0f * sin_a;
    float y1 = -fw/2.0f * sin_a + -fh/2.0f * cos_a;
    float x2 =  fw/2.0f * cos_a - -fh/2.0f * sin_a;
    float y2 =  fw/2.0f * sin_a + -fh/2.0f * cos_a;
    float x3 =  fw/2.0f * cos_a -  fh/2.0f * sin_a;
    float y3 =  fw/2.0f * sin_a +  fh/2.0f * cos_a;
    float x4 = -fw/2.0f * cos_a -  fh/2.0f * sin_a;
    float y4 = -fw/2.0f * sin_a +  fh/2.0f * cos_a;

    float min_x = std::min({x1, x2, x3, x4});
    float max_x = std::max({x1, x2, x3, x4});
    float min_y = std::min({y1, y2, y3, y4});
    float max_y = std::max({y1, y2, y3, y4});

    uint32_t new_w = static_cast<uint32_t>(std::ceil(max_x - min_x));
    uint32_t new_h = static_cast<uint32_t>(std::ceil(max_y - min_y));

    PixelForgeErrorCode err = dst.allocate(new_w, new_h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    float src_cx = w / 2.0f;
    float src_cy = h / 2.0f;
    float dst_cx = new_w / 2.0f;
    float dst_cy = new_h / 2.0f;

    for (uint32_t dy = 0; dy < new_h; ++dy) {
        for (uint32_t dx = 0; dx < new_w; ++dx) {
            float x_offset = dx - dst_cx;
            float y_offset = dy - dst_cy;

            // Rotate back to find source coordinates
            float sx = x_offset * cos_a + y_offset * sin_a + src_cx;
            float sy = -x_offset * sin_a + y_offset * cos_a + src_cy;

            int x0 = static_cast<int>(std::floor(sx));
            int y0 = static_cast<int>(std::floor(sy));
            int x1 = x0 + 1;
            int y1 = y0 + 1;

            if (x0 >= 0 && x1 < static_cast<int>(w) && y0 >= 0 && y1 < static_cast<int>(h)) {
                float tx = sx - x0;
                float ty = sy - y0;

                for (uint32_t c = 0; c < ch; ++c) {
                    float val_tl = src_data[(y0 * w + x0) * ch + c];
                    float val_tr = src_data[(y0 * w + x1) * ch + c];
                    float val_bl = src_data[(y1 * w + x0) * ch + c];
                    float val_br = src_data[(y1 * w + x1) * ch + c];

                    float top = val_tl + tx * (val_tr - val_tl);
                    float bottom = val_bl + tx * (val_br - val_bl);
                    float final_val = top + ty * (bottom - top);

                    dst_data[(dy * new_w + dx) * ch + c] = static_cast<uint8_t>(std::clamp(final_val, 0.0f, 255.0f));
                }
            } else {
                // Out of bounds - fill with transparent black
                size_t idx = (static_cast<size_t>(dy) * new_w + dx) * ch;
                std::memset(dst_data.data() + idx, 0, ch);
            }
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

PixelForgeErrorCode Transform::Translate(const Image& src, Image& dst, int dx, int dy) {
    if (!src.isValid()) return PixelForgeErrorCode::ERR_INVALID_PARAMETER;

    uint32_t w = src.getWidth();
    uint32_t h = src.getHeight();
    uint32_t ch = src.getChannels();

    PixelForgeErrorCode err = dst.allocate(w, h, src.getFormat());
    if (err != PixelForgeErrorCode::SUCCESS) return err;

    const auto& src_data = src.getData();
    auto& dst_data = dst.getData();

    std::memset(dst_data.data(), 0, dst_data.size());

    for (uint32_t y = 0; y < h; ++y) {
        int sy = static_cast<int>(y) - dy;
        if (sy < 0 || sy >= static_cast<int>(h)) continue;

        for (uint32_t x = 0; x < w; ++x) {
            int sx = static_cast<int>(x) - dx;
            if (sx < 0 || sx >= static_cast<int>(w)) continue;

            size_t src_idx = (static_cast<size_t>(sy) * w + sx) * ch;
            size_t dst_idx = (static_cast<size_t>(y) * w + x) * ch;
            std::memcpy(dst_data.data() + dst_idx, src_data.data() + src_idx, ch);
        }
    }
    return PixelForgeErrorCode::SUCCESS;
}

} // namespace PixelForge
