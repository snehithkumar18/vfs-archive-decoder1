#include "animation.h"

#include <algorithm>
#include <cstring>

namespace PixelForge {

namespace {

int normalized_delay(int delay_ms) {
    return delay_ms <= 0 ? 10 : delay_ms;
}

void fill_image(Image& image, Pixel color) {
    for (uint32_t y = 0; y < image.getHeight(); ++y) {
        for (uint32_t x = 0; x < image.getWidth(); ++x) {
            image.set_pixel(x, y, color);
        }
    }
    image.sync();
}

void clear_rect(Image& image, int x, int y, int w, int h, Pixel color) {
    int left = std::max(0, x);
    int top = std::max(0, y);
    int right = std::min<int>(static_cast<int>(image.getWidth()), x + w);
    int bottom = std::min<int>(static_cast<int>(image.getHeight()), y + h);
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) {
            image.set_pixel(static_cast<uint32_t>(px), static_cast<uint32_t>(py), color);
        }
    }
    image.sync();
}

Pixel alpha_over(Pixel dst, Pixel src) {
    const uint32_t sa = src.a;
    const uint32_t inv = 255 - sa;
    Pixel out;
    out.r = static_cast<uint8_t>((src.r * sa + dst.r * inv + 127) / 255);
    out.g = static_cast<uint8_t>((src.g * sa + dst.g * inv + 127) / 255);
    out.b = static_cast<uint8_t>((src.b * sa + dst.b * inv + 127) / 255);
    out.a = static_cast<uint8_t>(std::min<uint32_t>(255, sa + (dst.a * inv + 127) / 255));
    return out;
}

void draw_image(Image& dst, const Image& src, int x, int y) {
    if (!dst.isValid() || !src.isValid()) {
        return;
    }
    for (uint32_t sy = 0; sy < src.getHeight(); ++sy) {
        int dy = y + static_cast<int>(sy);
        if (dy < 0 || dy >= static_cast<int>(dst.getHeight())) {
            continue;
        }
        for (uint32_t sx = 0; sx < src.getWidth(); ++sx) {
            int dx = x + static_cast<int>(sx);
            if (dx < 0 || dx >= static_cast<int>(dst.getWidth())) {
                continue;
            }
            Pixel src_pixel = src.get_pixel(sx, sy);
            Pixel dst_pixel = dst.get_pixel(static_cast<uint32_t>(dx), static_cast<uint32_t>(dy));
            dst.set_pixel(static_cast<uint32_t>(dx), static_cast<uint32_t>(dy), alpha_over(dst_pixel, src_pixel));
        }
    }
    dst.sync();
}

} // namespace

std::unique_ptr<AnimationFrame> AnimationFrame::clone() const {
    auto frame = std::make_unique<AnimationFrame>();
    if (pixels) {
        frame->pixels = std::make_unique<Image>(*pixels);
    }
    frame->delay_ms = delay_ms;
    frame->disposal = disposal;
    frame->x = x;
    frame->y = y;
    frame->user_input = user_input;
    return frame;
}

size_t AnimationFrame::memory_usage() const {
    if (!pixels || !pixels->isValid()) {
        return 0;
    }
    return pixels->getData().size();
}

Animation::Animation() = default;

Animation::Animation(uint32_t width_, uint32_t height_)
    : width(width_), height(height_) {
}

PixelForgeErrorCode Animation::add_frame(std::unique_ptr<Image> image,
                                         int delay_ms,
                                         FrameDisposal disposal) {
    if (!image || !image->isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (width == 0 || height == 0) {
        width = image->getWidth();
        height = image->getHeight();
    }

    auto frame = std::make_unique<AnimationFrame>();
    frame->pixels = std::move(image);
    frame->delay_ms = normalized_delay(delay_ms);
    frame->disposal = disposal;
    return add_frame(std::move(frame));
}

PixelForgeErrorCode Animation::add_frame(std::unique_ptr<AnimationFrame> frame) {
    return insert_frame(m_frames.size(), std::move(frame));
}

PixelForgeErrorCode Animation::insert_frame(size_t index, std::unique_ptr<AnimationFrame> frame) {
    if (!frame || !frame->pixels || !frame->pixels->isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    if (width == 0 || height == 0) {
        width = frame->pixels->getWidth();
        height = frame->pixels->getHeight();
    }
    if (index > m_frames.size()) {
        index = m_frames.size();
    }
    frame->delay_ms = normalized_delay(frame->delay_ms);
    m_frames.insert(m_frames.begin() + static_cast<ptrdiff_t>(index), std::move(frame));
    return PixelForgeErrorCode::SUCCESS;
}

std::unique_ptr<AnimationFrame> Animation::remove_frame(size_t index) {
    if (index >= m_frames.size()) {
        return nullptr;
    }
    auto frame = std::move(m_frames[index]);
    m_frames.erase(m_frames.begin() + static_cast<ptrdiff_t>(index));
    return frame;
}

PixelForgeErrorCode Animation::replace_frame(size_t index, std::unique_ptr<AnimationFrame> frame) {
    if (index >= m_frames.size() || !frame || !frame->pixels || !frame->pixels->isValid()) {
        return PixelForgeErrorCode::ERR_INVALID_PARAMETER;
    }
    frame->delay_ms = normalized_delay(frame->delay_ms);
    m_frames[index] = std::move(frame);
    return PixelForgeErrorCode::SUCCESS;
}

AnimationFrame* Animation::get_frame(size_t index) const {
    if (index >= m_frames.size()) {
        return nullptr;
    }
    return m_frames[index].get();
}

int Animation::total_duration_ms() const {
    int total = 0;
    for (const auto& frame : m_frames) {
        total += normalized_delay(frame ? frame->delay_ms : 0);
    }
    return total;
}

int Animation::frame_start_time_ms(size_t index) const {
    int total = 0;
    size_t count = std::min(index, m_frames.size());
    for (size_t i = 0; i < count; ++i) {
        total += normalized_delay(m_frames[i] ? m_frames[i]->delay_ms : 0);
    }
    return total;
}

size_t Animation::get_frame_at_time(int time_ms) const {
    if (m_frames.empty()) {
        return 0;
    }
    int duration = total_duration_ms();
    if (duration <= 0) {
        return 0;
    }
    if (loop_count == 0) {
        time_ms %= duration;
        if (time_ms < 0) {
            time_ms += duration;
        }
    } else {
        time_ms = std::max(0, std::min(time_ms, duration - 1));
    }

    int cursor = 0;
    for (size_t i = 0; i < m_frames.size(); ++i) {
        cursor += normalized_delay(m_frames[i] ? m_frames[i]->delay_ms : 0);
        if (time_ms < cursor) {
            return i;
        }
    }
    return m_frames.size() - 1;
}

Animation Animation::clone() const {
    Animation copy(width, height);
    copy.loop_count = loop_count;
    copy.background_color = background_color;
    for (const auto& frame : m_frames) {
        copy.add_frame(frame ? frame->clone() : nullptr);
    }
    return copy;
}

void Animation::reverse() {
    std::reverse(m_frames.begin(), m_frames.end());
}

void Animation::set_uniform_delay(int delay_ms) {
    int value = normalized_delay(delay_ms);
    for (auto& frame : m_frames) {
        if (frame) {
            frame->delay_ms = value;
        }
    }
}

void Animation::scale_speed(float factor) {
    if (factor <= 0.0f) {
        return;
    }
    for (auto& frame : m_frames) {
        if (frame) {
            frame->delay_ms = normalized_delay(static_cast<int>(frame->delay_ms * factor));
        }
    }
}

size_t Animation::memory_usage() const {
    size_t total = 0;
    for (const auto& frame : m_frames) {
        total += frame ? frame->memory_usage() : 0;
    }
    return total;
}

void Animation::clear() {
    m_frames.clear();
}

Animation Animation::extract_range(size_t start, size_t end) const {
    Animation out(width, height);
    out.loop_count = loop_count;
    out.background_color = background_color;
    if (start >= m_frames.size() || start >= end) {
        return out;
    }
    end = std::min(end, m_frames.size());
    for (size_t i = start; i < end; ++i) {
        out.add_frame(m_frames[i] ? m_frames[i]->clone() : nullptr);
    }
    return out;
}

void Animation::append(Animation&& src) {
    if (width == 0 || height == 0) {
        width = src.width;
        height = src.height;
        background_color = src.background_color;
    }
    for (auto& frame : src.m_frames) {
        if (frame) {
            m_frames.push_back(std::move(frame));
        }
    }
    src.m_frames.clear();
}

AnimationPlayer::AnimationPlayer(const Animation* anim) {
    set_animation(anim);
}

void AnimationPlayer::set_animation(const Animation* anim) {
    m_anim = anim;
    reset();
}

bool AnimationPlayer::advance(int dt_ms) {
    if (!m_anim || m_anim->empty() || !m_playing || m_finished || dt_ms <= 0) {
        return false;
    }

    bool changed = false;
    m_elapsed_ms += dt_ms;
    m_frame_elapsed_ms += dt_ms;

    while (!m_finished && m_frame_elapsed_ms >= current_frame()->delay_ms) {
        m_frame_elapsed_ms -= current_frame()->delay_ms;
        transition_frame();
        changed = true;
    }
    return changed;
}

const AnimationFrame* AnimationPlayer::current_frame() const {
    if (!m_anim || m_anim->empty()) {
        return nullptr;
    }
    size_t index = std::min(m_current_frame, m_anim->frame_count() - 1);
    return m_anim->get_frame(index);
}

void AnimationPlayer::render_current(Image& canvas) const {
    if (!m_anim || m_anim->width == 0 || m_anim->height == 0) {
        return;
    }
    if (m_canvas_dirty || !m_canvas || !m_canvas->isValid()) {
        const_cast<AnimationPlayer*>(this)->m_canvas =
            std::make_unique<Image>(m_anim->width, m_anim->height, PixelFormat::RGBA8888);
        fill_image(*m_canvas, m_anim->background_color);
        const_cast<AnimationPlayer*>(this)->draw_frame_to_canvas(m_current_frame);
        const_cast<AnimationPlayer*>(this)->m_canvas_dirty = false;
    }
    canvas = *m_canvas;
}

void AnimationPlayer::reset() {
    m_current_frame = 0;
    m_elapsed_ms = 0;
    m_frame_elapsed_ms = 0;
    m_loops_completed = 0;
    m_finished = false;
    m_playing = true;
    m_canvas_dirty = true;
    m_canvas.reset();
    m_saved_canvas.reset();
}

void AnimationPlayer::seek_to_frame(size_t index) {
    if (!m_anim || m_anim->empty()) {
        reset();
        return;
    }
    m_current_frame = std::min(index, m_anim->frame_count() - 1);
    m_elapsed_ms = m_anim->frame_start_time_ms(m_current_frame);
    m_frame_elapsed_ms = 0;
    m_finished = false;
    m_canvas_dirty = true;
}

void AnimationPlayer::seek_to_time(int time_ms) {
    if (!m_anim || m_anim->empty()) {
        reset();
        return;
    }
    time_ms = std::max(0, time_ms);
    m_current_frame = m_anim->get_frame_at_time(time_ms);
    m_elapsed_ms = time_ms;
    m_frame_elapsed_ms = time_ms - m_anim->frame_start_time_ms(m_current_frame);
    m_finished = false;
    m_canvas_dirty = true;
}

void AnimationPlayer::transition_frame() {
    if (!m_anim || m_anim->empty()) {
        return;
    }

    apply_disposal();
    ++m_current_frame;
    if (m_current_frame >= m_anim->frame_count()) {
        ++m_loops_completed;
        if (m_loop_callback) {
            m_loop_callback(m_loops_completed);
        }
        if (m_anim->loop_count > 0 && m_loops_completed >= m_anim->loop_count) {
            m_current_frame = m_anim->frame_count() - 1;
            m_finished = true;
            m_playing = false;
            if (m_finish_callback) {
                m_finish_callback();
            }
            return;
        }
        m_current_frame = 0;
    }

    draw_frame_to_canvas(m_current_frame);
}

void AnimationPlayer::apply_disposal() {
    const AnimationFrame* frame = current_frame();
    if (!frame || !m_canvas) {
        return;
    }
    if (frame->disposal == FrameDisposal::Background && frame->pixels) {
        clear_rect(*m_canvas,
                   frame->x,
                   frame->y,
                   static_cast<int>(frame->pixels->getWidth()),
                   static_cast<int>(frame->pixels->getHeight()),
                   m_anim ? m_anim->background_color : Pixel{0, 0, 0, 0});
    } else if (frame->disposal == FrameDisposal::Previous) {
        restore_canvas_state();
    }
}

void AnimationPlayer::draw_frame_to_canvas(size_t frame_index) {
    if (!m_anim || frame_index >= m_anim->frame_count()) {
        return;
    }
    if (!m_canvas) {
        m_canvas = std::make_unique<Image>(m_anim->width, m_anim->height, PixelFormat::RGBA8888);
        fill_image(*m_canvas, m_anim->background_color);
    }

    const AnimationFrame* frame = m_anim->get_frame(frame_index);
    if (!frame || !frame->pixels) {
        return;
    }
    if (frame->disposal == FrameDisposal::Previous) {
        save_canvas_state();
    }
    draw_image(*m_canvas, *frame->pixels, frame->x, frame->y);
    m_canvas_dirty = false;
}

void AnimationPlayer::save_canvas_state() {
    if (m_canvas) {
        m_saved_canvas = std::make_unique<Image>(*m_canvas);
    }
}

void AnimationPlayer::restore_canvas_state() {
    if (m_saved_canvas) {
        m_canvas = std::make_unique<Image>(*m_saved_canvas);
        m_saved_canvas.reset();
    }
}

} // namespace PixelForge
