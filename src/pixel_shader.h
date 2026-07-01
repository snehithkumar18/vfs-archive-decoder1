#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "image.h"
#include "expression.h"
#include "expression_eval.h"

namespace PixelForge {

// ─────────────────────────────────────────────────────────────────────────────
// PixelShader: applies a mathematical expression to every pixel of an image.
//
// Built-in variables available in expressions:
//   r, g, b, a  — current pixel channel values, normalized [0.0, 1.0]
//   x, y        — pixel coordinates (integer)
//   w, h        — image width and height
//   u, v        — normalized coordinates [0.0, 1.0]
//
// Multiple expressions can be separated by ';' for multi-channel output:
//   "r = r * 0.5; g = g * 0.3; b = b * 0.2"
//
// If the expression doesn't use coordinate variables (x, y, u, v),
// an optimization path evaluates it once and applies the result uniformly.
// ─────────────────────────────────────────────────────────────────────────────
class PixelShader {
public:
    // Construct a shader from an expression string.
    // The expression is parsed immediately; check has_error() after construction.
    explicit PixelShader(const std::string& expression);
    ~PixelShader() = default;

    // Apply the shader to every pixel of the image
    void apply(Image& img);

    // Apply the shader only within a rectangular region
    void apply_region(Image& img, int rx, int ry, int rw, int rh);

    // Check for parse or evaluation errors
    bool has_error() const { return !m_error.empty(); }
    const std::string& get_error() const { return m_error; }

    // Get the original expression string
    const std::string& expression() const { return m_expression; }

    // Check whether the expression references coordinate variables
    bool uses_coordinates() const { return m_uses_coordinates; }

    // Set a custom variable before applying (persists across apply calls)
    void set_variable(const std::string& name, double value);

    // Register a custom function
    void register_function(const std::string& name,
                           std::function<double(const std::vector<double>&)> func,
                           int arg_count);

    // Predefined shader presets
    static PixelShader grayscale();
    static PixelShader sepia();
    static PixelShader invert();
    static PixelShader brightness(double factor);
    static PixelShader contrast(double factor);
    static PixelShader gamma_correction(double gamma);
    static PixelShader threshold(double level);
    static PixelShader vignette();

private:
    std::string m_expression;
    std::string m_error;
    std::unique_ptr<ASTNode> m_ast;
    ExpressionContext m_context;
    bool m_uses_coordinates = false;
    bool m_parsed = false;

    // Multi-statement support: each statement is a separate AST
    std::vector<std::unique_ptr<ASTNode>> m_statements;

    // Parse the expression and populate m_statements
    void parse_expression();

    // Check if the AST references any coordinate variables
    bool check_coordinate_usage(const ASTNode* node) const;

    // Apply shader to a single pixel
    void apply_pixel(ExpressionEvaluator& eval, Image& img,
                     uint32_t px, uint32_t py);
};

} // namespace PixelForge
