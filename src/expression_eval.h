#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include "expression.h"

namespace PixelForge {

// ─────────────────────────────────────────────────────────────────────────────
// Registered function descriptor
// ─────────────────────────────────────────────────────────────────────────────
struct RegisteredFunction {
    std::string name;
    std::function<double(const std::vector<double>&)> func;
    int min_args = 0;       // Minimum number of arguments (-1 = variadic)
    int max_args = 0;       // Maximum number of arguments (-1 = variadic)
    std::string description;
};

// ─────────────────────────────────────────────────────────────────────────────
// Expression evaluation context — holds variables and function registry
// ─────────────────────────────────────────────────────────────────────────────
class ExpressionContext {
public:
    ExpressionContext();
    ~ExpressionContext() = default;

    // ─── Variable management ────────────────────────────────────────────

    // Set a variable's value
    void set_variable(const std::string& name, double value);

    // Get a variable's value; returns 0.0 if not found
    double get_variable(const std::string& name) const;

    // Check if a variable exists
    bool has_variable(const std::string& name) const;

    // Remove a variable
    void remove_variable(const std::string& name);

    // Clear all variables
    void clear_variables();

    // Get all variable names
    std::vector<std::string> variable_names() const;

    // Direct access to variable map
    const std::map<std::string, double>& variables() const { return m_variables; }

    // ─── Function management ────────────────────────────────────────────

    // Register a function with fixed argument count
    void register_function(const std::string& name,
                           std::function<double(const std::vector<double>&)> func,
                           int arg_count,
                           const std::string& description = "");

    // Register a function with variable argument count
    void register_function(const std::string& name,
                           std::function<double(const std::vector<double>&)> func,
                           int min_args, int max_args,
                           const std::string& description = "");

    // Check if a function exists
    bool has_function(const std::string& name) const;

    // Call a registered function
    double call_function(const std::string& name,
                         const std::vector<double>& args) const;

    // Get function info
    const RegisteredFunction* get_function(const std::string& name) const;

    // Get all function names
    std::vector<std::string> function_names() const;

    // ─── Builtin registration ───────────────────────────────────────────

    // Register all standard math/utility functions
    void register_builtin_functions();

    // ─── Constants ──────────────────────────────────────────────────────

    // Register standard math constants (pi, e, etc.)
    void register_builtin_constants();

private:
    std::map<std::string, double> m_variables;
    std::map<std::string, RegisteredFunction> m_functions;

    // Random state for deterministic rand()
    uint64_t m_rand_state = 12345678901234ULL;
    double next_rand();

    // Registration helpers
    void register_trig_functions();
    void register_exp_log_functions();
    void register_rounding_functions();
    void register_clamping_functions();
    void register_conversion_functions();
    void register_misc_functions();
};

// ─────────────────────────────────────────────────────────────────────────────
// Expression evaluator — walks AST and computes results
// ─────────────────────────────────────────────────────────────────────────────
class ExpressionEvaluator {
public:
    explicit ExpressionEvaluator(ExpressionContext& ctx);
    ~ExpressionEvaluator() = default;

    // Evaluate a pre-parsed AST tree
    double evaluate(const ASTNode* root);

    // Parse and evaluate an expression string
    double evaluate(const std::string& expression);

    // Check for evaluation errors
    bool has_error() const { return !m_error.empty(); }
    const std::string& get_error() const { return m_error; }
    void clear_error() { m_error.clear(); }

    // Get the context
    ExpressionContext& context() { return m_ctx; }
    const ExpressionContext& context() const { return m_ctx; }

private:
    ExpressionContext& m_ctx;
    std::string m_error;

    // Internal evaluation dispatch
    double eval_node(const ASTNode* node);
    double eval_number(const ASTNode* node);
    double eval_variable(const ASTNode* node);
    double eval_unary(const ASTNode* node);
    double eval_binary(const ASTNode* node);
    double eval_function_call(const ASTNode* node);
    double eval_ternary(const ASTNode* node);
    double eval_assignment(const ASTNode* node);
    double eval_statement_list(const ASTNode* node);

    // Error reporting
    void set_error(const std::string& msg);
};

} // namespace PixelForge
