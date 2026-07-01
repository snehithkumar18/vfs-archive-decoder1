#pragma once

#include <cstdint>
#include <string>
#include "bitmap_font.h"
#include "image.h"

namespace PixelForge {

// ─────────────────────────────────────────────────────────────────────────────
// High-level text rendering convenience API
//
// These functions wrap the TextLayout engine to provide simple one-call
// text rendering for common use cases.
// ─────────────────────────────────────────────────────────────────────────────

// Draw single-line text at position (x, y) on the target image.
// Text is drawn left-aligned from the given position.
// (x, y) is the top-left corner of the text baseline.
void draw_text(Image& img, const std::string& text, int x, int y,
               BitmapFont* font,
               uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
               uint8_t a = 255);

// Draw text with word wrapping within max_width pixels.
// Lines that exceed max_width are broken at word boundaries.
void draw_text_wrapped(Image& img, const std::string& text,
                       int x, int y, int max_width,
                       BitmapFont* font,
                       uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                       uint8_t a = 255);

// Draw text centered at (center_x, center_y).
// The text bounding box is centered both horizontally and vertically.
void draw_text_centered(Image& img, const std::string& text,
                        int center_x, int center_y,
                        BitmapFont* font,
                        uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                        uint8_t a = 255);

// Draw right-aligned text ending at x_right.
void draw_text_right(Image& img, const std::string& text,
                     int x_right, int y,
                     BitmapFont* font,
                     uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                     uint8_t a = 255);

// Draw text with a background rectangle behind it.
void draw_text_with_background(Image& img, const std::string& text,
                               int x, int y,
                               BitmapFont* font,
                               uint8_t fg_r, uint8_t fg_g, uint8_t fg_b,
                               uint8_t bg_r, uint8_t bg_g, uint8_t bg_b,
                               int padding = 2);

// Render text into a newly allocated Image sized to fit the text exactly.
// Background is filled with (bg_r, bg_g, bg_b). Returns nullptr on error.
Image* render_text_to_image(const std::string& text,
                            BitmapFont* font,
                            uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                            uint8_t bg_r = 0, uint8_t bg_g = 0, uint8_t bg_b = 0,
                            uint8_t bg_a = 255);

// Render wrapped text into a newly allocated Image with the given max width.
Image* render_text_wrapped_to_image(const std::string& text,
                                    int max_width,
                                    BitmapFont* font,
                                    uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                                    uint8_t bg_r = 0, uint8_t bg_g = 0, uint8_t bg_b = 0,
                                    uint8_t bg_a = 255);

// Measure text dimensions without rendering.
// Returns the bounding box that the text would occupy.
void measure_text(const std::string& text, BitmapFont* font,
                  int* out_width, int* out_height);

// Measure wrapped text dimensions.
void measure_text_wrapped(const std::string& text, int max_width,
                          BitmapFont* font,
                          int* out_width, int* out_height);

} // namespace PixelForge
