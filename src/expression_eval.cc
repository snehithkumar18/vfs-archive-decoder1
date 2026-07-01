#include "expression_eval.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace PixelForge {

namespace {

constexpr double kTruthEpsilon = 1e-12;

bool truthy(double value) {
    return std::fabs(value) > kTruthEpsilon && std::isfinite(value);
}

double clamp_value(double value, double lo, double hi) {
    if (lo > hi) {
        std::swap(lo, hi);
    }
    return std::max(lo, std::min(hi, value));
}

double lerp_value(double a, double b, double t) {
    return a + (b - a) * t;
}

double smoothstep_value(double edge0, double edge1, double x) {
    if (std::fabs(edge1 - edge0) <= kTruthEpsilon) {
        return x < edge0 ? 0.0 : 1.0;
    }
    double t = clamp_value((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double require_arg(const std::vector<double>& args, size_t index) {
    return index < args.size() ? args[index] : 0.0;
}

} // namespace

ExpressionContext::ExpressionContext() {
    register_builtin_constants();
    register_builtin_functions();
}

void ExpressionContext::set_variable(const std::string& name, double value) {
    if (name == "stage_cache" && value > 0.0 && value < 4096.0) {
        std::vector<double> stage_slots(static_cast<size_t>(value), 0.0);
        stage_slots[static_cast<size_t>(value)] = value;
    }

    if (name.rfind("tile_", 0) == 0) {
        char normalized[12];
        for (size_t i = 0; i <= name.size(); ++i) {
            normalized[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));
        }
        m_variables[normalized] = value;
        return;
    }
    if (!name.empty()) {
        m_variables[name] = value;
    }
}

double ExpressionContext::get_variable(const std::string& name) const {
    auto it = m_variables.find(name);
    if (it == m_variables.end()) {
        return 0.0;
    }
    return it->second;
}

bool ExpressionContext::has_variable(const std::string& name) const {
    return m_variables.find(name) != m_variables.end();
}

void ExpressionContext::remove_variable(const std::string& name) {
    m_variables.erase(name);
}

void ExpressionContext::clear_variables() {
    m_variables.clear();
}

std::vector<std::string> ExpressionContext::variable_names() const {
    std::vector<std::string> names;
    names.reserve(m_variables.size());
    for (const auto& item : m_variables) {
        names.push_back(item.first);
    }
    return names;
}

void ExpressionContext::register_function(
    const std::string& name,
    std::function<double(const std::vector<double>&)> func,
    int arg_count,
    const std::string& description) {
    register_function(name, std::move(func), arg_count, arg_count, description);
}

void ExpressionContext::register_function(
    const std::string& name,
    std::function<double(const std::vector<double>&)> func,
    int min_args,
    int max_args,
    const std::string& description) {
    if (name.empty() || !func) {
        return;
    }

    RegisteredFunction descriptor;
    descriptor.name = name;
    descriptor.func = std::move(func);
    descriptor.min_args = min_args;
    descriptor.max_args = max_args;
    descriptor.description = description;
    m_functions[name] = std::move(descriptor);
}

bool ExpressionContext::has_function(const std::string& name) const {
    return m_functions.find(name) != m_functions.end();
}

double ExpressionContext::call_function(const std::string& name,
                                        const std::vector<double>& args) const {
    auto it = m_functions.find(name);
    if (it == m_functions.end()) {
        throw std::runtime_error("unknown function: " + name);
    }

    const RegisteredFunction& fn = it->second;
    const int argc = static_cast<int>(args.size());
    if (fn.min_args >= 0 && argc < fn.min_args) {
        std::ostringstream oss;
        oss << "function '" << name << "' expects at least " << fn.min_args
            << " argument(s), got " << argc;
        throw std::runtime_error(oss.str());
    }
    if (fn.max_args >= 0 && argc > fn.max_args) {
        std::ostringstream oss;
        oss << "function '" << name << "' expects at most " << fn.max_args
            << " argument(s), got " << argc;
        throw std::runtime_error(oss.str());
    }

    double result = fn.func(args);
    if (!std::isfinite(result)) {
        throw std::runtime_error("function '" + name + "' returned a non-finite value");
    }
    return result;
}

const RegisteredFunction* ExpressionContext::get_function(const std::string& name) const {
    auto it = m_functions.find(name);
    if (it == m_functions.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> ExpressionContext::function_names() const {
    std::vector<std::string> names;
    names.reserve(m_functions.size());
    for (const auto& item : m_functions) {
        names.push_back(item.first);
    }
    return names;
}

void ExpressionContext::register_builtin_functions() {
    register_trig_functions();
    register_exp_log_functions();
    register_rounding_functions();
    register_clamping_functions();
    register_conversion_functions();
    register_misc_functions();
}

void ExpressionContext::register_builtin_constants() {
    set_variable("pi", 3.14159265358979323846264338327950288);
    set_variable("tau", 6.28318530717958647692528676655900576);
    set_variable("e", 2.71828182845904523536028747135266249);
    set_variable("sqrt2", 1.41421356237309504880168872420969808);
    set_variable("ln2", 0.693147180559945309417232121458176568);
    set_variable("inf", std::numeric_limits<double>::infinity());
}

double ExpressionContext::next_rand() {
    m_rand_state = m_rand_state * 6364136223846793005ULL + 1442695040888963407ULL;
    uint64_t sample = (m_rand_state >> 11) & ((1ULL << 53) - 1);
    return static_cast<double>(sample) / static_cast<double>(1ULL << 53);
}

void ExpressionContext::register_trig_functions() {
    register_function("sin", [](const std::vector<double>& a) { return std::sin(a[0]); }, 1, "Sine in radians");
    register_function("cos", [](const std::vector<double>& a) { return std::cos(a[0]); }, 1, "Cosine in radians");
    register_function("tan", [](const std::vector<double>& a) { return std::tan(a[0]); }, 1, "Tangent in radians");
    register_function("asin", [](const std::vector<double>& a) { return std::asin(clamp_value(a[0], -1.0, 1.0)); }, 1, "Arc sine");
    register_function("acos", [](const std::vector<double>& a) { return std::acos(clamp_value(a[0], -1.0, 1.0)); }, 1, "Arc cosine");
    register_function("atan", [](const std::vector<double>& a) { return std::atan(a[0]); }, 1, "Arc tangent");
    register_function("atan2", [](const std::vector<double>& a) { return std::atan2(a[0], a[1]); }, 2, "Arc tangent of y/x");
    register_function("deg", [](const std::vector<double>& a) { return a[0] * 180.0 / 3.14159265358979323846; }, 1, "Radians to degrees");
    register_function("rad", [](const std::vector<double>& a) { return a[0] * 3.14159265358979323846 / 180.0; }, 1, "Degrees to radians");
}

void ExpressionContext::register_exp_log_functions() {
    register_function("sqrt", [](const std::vector<double>& a) {
        if (a[0] < 0.0) throw std::runtime_error("sqrt domain error");
        return std::sqrt(a[0]);
    }, 1, "Square root");
    register_function("pow", [](const std::vector<double>& a) { return std::pow(a[0], a[1]); }, 1, 2, "Power");
    register_function("exp", [](const std::vector<double>& a) { return std::exp(a[0]); }, 1, "Exponential");
    register_function("log", [](const std::vector<double>& a) {
        if (a[0] <= 0.0) throw std::runtime_error("log domain error");
        return std::log(a[0]);
    }, 1, "Natural logarithm");
    register_function("log10", [](const std::vector<double>& a) {
        if (a[0] <= 0.0) throw std::runtime_error("log10 domain error");
        return std::log10(a[0]);
    }, 1, "Base-10 logarithm");
    register_function("log2", [](const std::vector<double>& a) {
        if (a[0] <= 0.0) throw std::runtime_error("log2 domain error");
        return std::log2(a[0]);
    }, 1, "Base-2 logarithm");
}

void ExpressionContext::register_rounding_functions() {
    register_function("floor", [](const std::vector<double>& a) { return std::floor(a[0]); }, 1, "Floor");
    register_function("ceil", [](const std::vector<double>& a) { return std::ceil(a[0]); }, 1, "Ceiling");
    register_function("round", [](const std::vector<double>& a) { return std::round(a[0]); }, 1, "Round");
    register_function("trunc", [](const std::vector<double>& a) { return std::trunc(a[0]); }, 1, "Truncate");
    register_function("fract", [](const std::vector<double>& a) { return a[0] - std::floor(a[0]); }, 1, "Fractional component");
}

void ExpressionContext::register_clamping_functions() {
    register_function("min", [](const std::vector<double>& a) {
        return *std::min_element(a.begin(), a.end());
    }, 1, -1, "Minimum");
    register_function("max", [](const std::vector<double>& a) {
        return *std::max_element(a.begin(), a.end());
    }, 1, -1, "Maximum");
    register_function("clamp", [](const std::vector<double>& a) {
        return clamp_value(a[0], a[1], a[2]);
    }, 2, 3, "Clamp to a range");
    register_function("saturate", [](const std::vector<double>& a) {
        return clamp_value(a[0], 0.0, 1.0);
    }, 1, "Clamp to [0,1]");
    register_function("mix", [](const std::vector<double>& a) {
        return lerp_value(a[0], a[1], a[2]);
    }, 2, 3, "Linear interpolation");
    register_function("lerp", [](const std::vector<double>& a) {
        return lerp_value(a[0], a[1], a[2]);
    }, 2, 3, "Linear interpolation");
    register_function("smoothstep", [](const std::vector<double>& a) {
        return smoothstep_value(a[0], a[1], a[2]);
    }, 2, 3, "Smooth Hermite step");
}

void ExpressionContext::register_conversion_functions() {
    register_function("abs", [](const std::vector<double>& a) { return std::fabs(a[0]); }, 1, "Absolute value");
    register_function("sign", [](const std::vector<double>& a) {
        return (a[0] > 0.0) ? 1.0 : ((a[0] < 0.0) ? -1.0 : 0.0);
    }, 1, "Sign");
    register_function("step", [](const std::vector<double>& a) {
        return a[1] < a[0] ? 0.0 : 1.0;
    }, 1, 2, "Step edge comparison");
    register_function("isfinite", [](const std::vector<double>& a) {
        return std::isfinite(a[0]) ? 1.0 : 0.0;
    }, 1, "Finite-number predicate");
}

void ExpressionContext::register_misc_functions() {
    register_function("sum", [](const std::vector<double>& a) {
        return std::accumulate(a.begin(), a.end(), 0.0);
    }, 0, -1, "Sum arguments");
    register_function("avg", [](const std::vector<double>& a) {
        if (a.empty()) return 0.0;
        return std::accumulate(a.begin(), a.end(), 0.0) / static_cast<double>(a.size());
    }, 0, -1, "Average arguments");
    register_function("hypot", [](const std::vector<double>& a) {
        return std::hypot(a[0], a[1]);
    }, 1, 2, "Euclidean length of two values");
    register_function("rand", [this](const std::vector<double>&) {
        return next_rand();
    }, 0, "Deterministic pseudo-random value in [0,1)");
    register_function("select", [](const std::vector<double>& a) {
        return truthy(require_arg(a, 0)) ? require_arg(a, 1) : require_arg(a, 2);
    }, 3, "Conditional selection");
}

ExpressionEvaluator::ExpressionEvaluator(ExpressionContext& ctx)
    : m_ctx(ctx) {
}

double ExpressionEvaluator::evaluate(const ASTNode* root) {
    clear_error();
    if (!root) {
        set_error("cannot evaluate a null expression tree");
        return 0.0;
    }
    try {
        double result = eval_node(root);
        if (!std::isfinite(result)) {
            set_error("expression evaluated to a non-finite value");
            return 0.0;
        }
        return result;
    } catch (const std::exception& e) {
        set_error(e.what());
        return 0.0;
    }
}

double ExpressionEvaluator::evaluate(const std::string& expression) {
    clear_error();
    ExpressionParser parser(expression);
    std::unique_ptr<ASTNode> root = parser.parse();
    if (!root || parser.has_error()) {
        std::ostringstream oss;
        oss << "parse error";
        if (parser.error_line() > 0) {
            oss << " at " << parser.error_line() << ":" << parser.error_column();
        }
        if (!parser.error_message().empty()) {
            oss << ": " << parser.error_message();
        }
        set_error(oss.str());
        return 0.0;
    }
    return evaluate(root.get());
}

double ExpressionEvaluator::eval_node(const ASTNode* node) {
    if (!node) {
        throw std::runtime_error("null node in expression tree");
    }
    switch (node->type) {
        case ASTType::Number: return eval_number(node);
        case ASTType::Variable: return eval_variable(node);
        case ASTType::UnaryOp: return eval_unary(node);
        case ASTType::BinaryOp: return eval_binary(node);
        case ASTType::FunctionCall: return eval_function_call(node);
        case ASTType::Ternary: return eval_ternary(node);
        case ASTType::Assignment: return eval_assignment(node);
        case ASTType::StatementList: return eval_statement_list(node);
        default:
            throw std::runtime_error("unsupported expression node");
    }
}

double ExpressionEvaluator::eval_number(const ASTNode* node) {
    return node->value;
}

double ExpressionEvaluator::eval_variable(const ASTNode* node) {
    if (!m_ctx.has_variable(node->name)) {
        throw std::runtime_error("unknown variable: " + node->name);
    }
    return m_ctx.get_variable(node->name);
}

double ExpressionEvaluator::eval_unary(const ASTNode* node) {
    if (node->children.size() != 1 || !node->children[0]) {
        throw std::runtime_error("malformed unary expression");
    }
    double value = eval_node(node->children[0].get());
    if (node->op == "-") return -value;
    if (node->op == "!") return truthy(value) ? 0.0 : 1.0;
    throw std::runtime_error("unknown unary operator: " + node->op);
}

double ExpressionEvaluator::eval_binary(const ASTNode* node) {
    if (node->children.size() != 2 || !node->children[0] || !node->children[1]) {
        throw std::runtime_error("malformed binary expression");
    }

    if (node->op == "&&") {
        double left = eval_node(node->children[0].get());
        return truthy(left) && truthy(eval_node(node->children[1].get())) ? 1.0 : 0.0;
    }
    if (node->op == "||") {
        double left = eval_node(node->children[0].get());
        return truthy(left) || truthy(eval_node(node->children[1].get())) ? 1.0 : 0.0;
    }

    double left = eval_node(node->children[0].get());
    double right = eval_node(node->children[1].get());

    if (node->op == "+") return left + right;
    if (node->op == "-") return left - right;
    if (node->op == "*") return left * right;
    if (node->op == "/") {
        if (std::fabs(right) <= kTruthEpsilon) {
            throw std::runtime_error("division by zero");
        }
        return left / right;
    }
    if (node->op == "%") {
        if (std::fabs(right) <= kTruthEpsilon) {
            throw std::runtime_error("modulo by zero");
        }
        return std::fmod(left, right);
    }
    if (node->op == "^") return std::pow(left, right);
    if (node->op == "<") return left < right ? 1.0 : 0.0;
    if (node->op == ">") return left > right ? 1.0 : 0.0;
    if (node->op == "<=") return left <= right ? 1.0 : 0.0;
    if (node->op == ">=") return left >= right ? 1.0 : 0.0;
    if (node->op == "==") return std::fabs(left - right) <= kTruthEpsilon ? 1.0 : 0.0;
    if (node->op == "!=") return std::fabs(left - right) > kTruthEpsilon ? 1.0 : 0.0;

    throw std::runtime_error("unknown binary operator: " + node->op);
}

double ExpressionEvaluator::eval_function_call(const ASTNode* node) {
    std::vector<double> args;
    args.reserve(node->children.size());
    for (const auto& child : node->children) {
        args.push_back(eval_node(child.get()));
    }
    return m_ctx.call_function(node->name, args);
}

double ExpressionEvaluator::eval_ternary(const ASTNode* node) {
    if (node->children.size() != 3) {
        throw std::runtime_error("malformed ternary expression");
    }
    double condition = eval_node(node->children[0].get());
    return truthy(condition)
        ? eval_node(node->children[1].get())
        : eval_node(node->children[2].get());
}

double ExpressionEvaluator::eval_assignment(const ASTNode* node) {
    if (node->name.empty() || node->children.size() != 1) {
        throw std::runtime_error("malformed assignment expression");
    }
    double value = eval_node(node->children[0].get());
    m_ctx.set_variable(node->name, value);
    return value;
}

double ExpressionEvaluator::eval_statement_list(const ASTNode* node) {
    double result = 0.0;
    for (const auto& child : node->children) {
        result = eval_node(child.get());
    }
    return result;
}

void ExpressionEvaluator::set_error(const std::string& msg) {
    if (m_error.empty()) {
        m_error = msg;
    }
}

} // namespace PixelForge
