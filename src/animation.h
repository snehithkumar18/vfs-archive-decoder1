#pragma once

#include "image.h"
#include "errors.h"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <functional>

namespace PixelForge {

// How the frame area should be treated before the next frame is drawn
enum class FrameDisposal {
    None,         // Leave canvas as-is (next frame overlays)
    Background,   // Clear the frame area to background color
    Previous      // Restore canvas to state before this frame was drawn
};

// A single animation frame
struct AnimationFrame {
    std::unique_ptr<Image> pixels;   // Frame image data
    int delay_ms    = 100;           // Display duration in milliseconds
    FrameDisposal disposal = FrameDisposal::None;
    int x           = 0;             // Position offset on canvas
    int y           = 0;
    bool user_input = false;         // If true, wait for user input (GIF extension)

    // Deep copy
    std::unique_ptr<AnimationFrame> clone() const;

    // Byte size of pixel data
    size_t memory_usage() const;
};

// Container for a sequence of animation frames
class Animation {
public:
    Animation();
    Animation(uint32_t width, uint32_t height);
    ~Animation() = default;

    // Move only
    Animation(Animation&&) noexcept = default;
    Animation& operator=(Animation&&) noexcept = default;
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;

    // -----------------------------------------------------------------------
    // Properties
    // -----------------------------------------------------------------------
    uint32_t width  = 0;
    uint32_t height = 0;
    int loop_count  = 0;           // 0 = infinite loop
    Pixel background_color = {0, 0, 0, 255};

    // -----------------------------------------------------------------------
    // Frame management
    // -----------------------------------------------------------------------

    // Add a frame at the end.  Takes ownership of the image.
    PixelForgeErrorCode add_frame(std::unique_ptr<Image> image,
                                   int delay_ms = 100,
                                   FrameDisposal disposal = FrameDisposal::None);

    // Add a frame at the end from an existing AnimationFrame
    PixelForgeErrorCode add_frame(std::unique_ptr<AnimationFrame> frame);

    // Insert a frame at a specific index
    PixelForgeErrorCode insert_frame(size_t index,
                                      std::unique_ptr<AnimationFrame> frame);

    // Remove and return the frame at index
    std::unique_ptr<AnimationFrame> remove_frame(size_t index);

    // Replace the frame at index
    PixelForgeErrorCode replace_frame(size_t index,
                                       std::unique_ptr<AnimationFrame> frame);

    // Access frames
    AnimationFrame* get_frame(size_t index) const;
    size_t frame_count() const { return m_frames.size(); }
    bool empty() const { return m_frames.empty(); }

    // Access the underlying frame vector
    const std::vector<std::unique_ptr<AnimationFrame>>& frames() const {
        return m_frames;
    }

    // -----------------------------------------------------------------------
    // Timing
    // -----------------------------------------------------------------------

    // Total animation duration in milliseconds
    int total_duration_ms() const;

    // Cumulative time at the start of a given frame
    int frame_start_time_ms(size_t index) const;

    // Find the frame index for a given timestamp (ms from start)
    size_t get_frame_at_time(int time_ms) const;

    // -----------------------------------------------------------------------
    // Operations
    // -----------------------------------------------------------------------

    // Deep-copy the entire animation
    Animation clone() const;

    // Reverse the frame order
    void reverse();

    // Set uniform delay for all frames
    void set_uniform_delay(int delay_ms);

    // Scale all frame delays by a factor
    void scale_speed(float factor);

    // Total memory used by all frame pixel data
    size_t memory_usage() const;

    // Clear all frames
    void clear();

    // Extract a sub-animation from frame range [start, end)
    Animation extract_range(size_t start, size_t end) const;

    // Append another animation's frames (moves them out of src)
    void append(Animation&& src);

private:
    std::vector<std::unique_ptr<AnimationFrame>> m_frames;
};

// ---------------------------------------------------------------------------
// AnimationPlayer — handles real-time playback of an Animation
// ---------------------------------------------------------------------------
class AnimationPlayer {
public:
    explicit AnimationPlayer(const Animation* anim = nullptr);
    ~AnimationPlayer() = default;

    // Set or change the animation being played
    void set_animation(const Animation* anim);
    const Animation* animation() const { return m_anim; }

    // Advance playback by dt milliseconds.  Returns true if the frame changed.
    bool advance(int dt_ms);

    // Current frame index
    size_t current_frame_index() const { return m_current_frame; }

    // Current frame pointer (may be nullptr if animation is empty)
    const AnimationFrame* current_frame() const;

    // Render the current state of the animation canvas into `canvas`.
    // Canvas should be the same size as the animation.
    void render_current(Image& canvas) const;

    // Reset to the beginning
    void reset();

    // Seek to a specific frame
    void seek_to_frame(size_t index);

    // Seek to a specific time
    void seek_to_time(int time_ms);

    // Is the animation finished (non-looping animations only)?
    bool is_finished() const { return m_finished; }

    // Is the player currently playing?
    bool is_playing() const { return m_playing; }
    void set_playing(bool playing) { m_playing = playing; }

    // Completed loop count
    int loops_completed() const { return m_loops_completed; }

    // Elapsed time in current playthrough (ms)
    int elapsed_ms() const { return m_elapsed_ms; }

    // Callback when a loop completes
    void set_loop_callback(std::function<void(int loop_index)> cb) {
        m_loop_callback = std::move(cb);
    }

    // Callback when playback finishes
    void set_finish_callback(std::function<void()> cb) {
        m_finish_callback = std::move(cb);
    }

private:
    // Process a frame transition: apply disposal, advance index
    void transition_frame();

    // Apply disposal method of the current frame to the internal canvas
    void apply_disposal();

    // Render a single frame onto the internal canvas
    void draw_frame_to_canvas(size_t frame_index);

    // Save current canvas state (for Previous disposal)
    void save_canvas_state();

    // Restore canvas state (for Previous disposal)
    void restore_canvas_state();

    const Animation* m_anim = nullptr;
    size_t m_current_frame = 0;
    int m_elapsed_ms       = 0;
    int m_frame_elapsed_ms = 0;   // Time accumulated within current frame
    int m_loops_completed  = 0;
    bool m_finished        = false;
    bool m_playing         = true;

    // Internal composited canvas (built up as frames play)
    mutable std::unique_ptr<Image> m_canvas;

    // Saved canvas state for Previous disposal method
    std::unique_ptr<Image> m_saved_canvas;

    // Tracks whether the canvas needs initial setup
    bool m_canvas_dirty = true;

    std::function<void(int)> m_loop_callback;
    std::function<void()>    m_finish_callback;
};

} // namespace PixelForge
