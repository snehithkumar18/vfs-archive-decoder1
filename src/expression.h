#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

namespace PixelForge {

// ─────────────────────────────────────────────────────────────────────────────
// Token types for the expression language
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenType {
    // Literals
    NUMBER,           // 42, 3.14, 1e5, 0xFF
    IDENTIFIER,       // variable names, function names

    // Arithmetic operators
    PLUS,             // +
    MINUS,            // -
    STAR,             // *
    SLASH,            // /
    PERCENT,          // %
    CARET,            // ^  (power)

    // Comparison operators
    LESS,             // <
    GREATER,          // >
    LESS_EQ,          // <=
    GREATER_EQ,       // >=
    EQUAL,            // ==
    NOT_EQUAL,        // !=

    // Logical operators
    AND,              // &&
    OR,               // ||
    NOT,              // !

    // Assignment
    ASSIGN,           // =

    // Ternary
    QUESTION,         // ?
    COLON,            // :

    // Delimiters
    LPAREN,           // (
    RPAREN,           // )
    COMMA,            // ,
    SEMICOLON,        // ;

    // Special
    EOF_TOKEN,        // End of input
    ERROR_TOKEN       // Lexical error
};

// Returns a human-readable name for a token type
const char* token_type_name(TokenType type);

// ─────────────────────────────────────────────────────────────────────────────
// Token: a single lexical unit
// ─────────────────────────────────────────────────────────────────────────────
struct Token {
    TokenType type = TokenType::EOF_TOKEN;
    std::string text;
    double number_value = 0.0;
    int line = 1;
    int column = 1;

    Token() = default;
    Token(TokenType t, const std::string& txt, int ln, int col)
        : type(t), text(txt), number_value(0.0), line(ln), column(col) {}
    Token(TokenType t, const std::string& txt, double val, int ln, int col)
        : type(t), text(txt), number_value(val), line(ln), column(col) {}

    bool is(TokenType t) const { return type == t; }
    bool is_operator() const;
    bool is_literal() const { return type == TokenType::NUMBER; }
    bool is_end() const { return type == TokenType::EOF_TOKEN; }

    std::string to_string() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// AST node types
// ─────────────────────────────────────────────────────────────────────────────
enum class ASTType {
    Number,          // Literal number
    Variable,        // Variable reference
    UnaryOp,         // Unary operation (-, !)
    BinaryOp,        // Binary operation (+, -, *, /, %, ^, comparisons, logical)
    FunctionCall,    // Function invocation with arguments
    Ternary,         // condition ? true_expr : false_expr
    Assignment,      // variable = expression
    StatementList    // Multiple statements separated by ';'
};

// Returns a human-readable name for an AST node type
const char* ast_type_name(ASTType type);

// ─────────────────────────────────────────────────────────────────────────────
// AST Node: a node in the abstract syntax tree
// ─────────────────────────────────────────────────────────────────────────────
struct ASTNode {
    ASTType type;
    double value = 0.0;          // For Number nodes
    std::string name;            // For Variable, FunctionCall, operators
    std::string op;              // For UnaryOp, BinaryOp
    std::vector<std::unique_ptr<ASTNode>> children;

    ASTNode() : type(ASTType::Number) {}
    explicit ASTNode(ASTType t) : type(t) {}

    // Factory helpers for readability
    static std::unique_ptr<ASTNode> make_number(double val);
    static std::unique_ptr<ASTNode> make_variable(const std::string& name);
    static std::unique_ptr<ASTNode> make_unary(const std::string& op,
                                                std::unique_ptr<ASTNode> operand);
    static std::unique_ptr<ASTNode> make_binary(const std::string& op,
                                                 std::unique_ptr<ASTNode> left,
                                                 std::unique_ptr<ASTNode> right);
    static std::unique_ptr<ASTNode> make_function_call(const std::string& name,
                                                        std::vector<std::unique_ptr<ASTNode>> args);
    static std::unique_ptr<ASTNode> make_ternary(std::unique_ptr<ASTNode> condition,
                                                  std::unique_ptr<ASTNode> true_branch,
                                                  std::unique_ptr<ASTNode> false_branch);
    static std::unique_ptr<ASTNode> make_assignment(const std::string& var_name,
                                                     std::unique_ptr<ASTNode> value);
    static std::unique_ptr<ASTNode> make_statement_list(
        std::vector<std::unique_ptr<ASTNode>> statements);

    // Deep clone
    std::unique_ptr<ASTNode> clone() const;

    // Debug string representation
    std::string dump(int indent = 0) const;

    // Count nodes in subtree
    int node_count() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// Tokenizer: converts source text into a stream of tokens
// ─────────────────────────────────────────────────────────────────────────────
class ExpressionTokenizer {
public:
    explicit ExpressionTokenizer(const std::string& source);

    // Get the next token and advance
    Token next_token();

    // Peek at the next token without consuming
    Token peek();

    // Check if there are more tokens
    bool has_more() const;

    // Get current line and column
    int current_line() const { return m_line; }
    int current_column() const { return m_column; }

    // Get any accumulated error message
    const std::string& error_message() const { return m_error; }

private:
    std::string m_source;
    size_t m_pos;
    int m_line;
    int m_column;
    bool m_has_peek;
    Token m_peek_token;
    std::string m_error;

    // Character inspection helpers
    char current() const;
    char look_ahead(int offset = 1) const;
    bool at_end() const;
    void advance();
    void advance_n(int n);

    // Whitespace and comment skipping
    void skip_whitespace_and_comments();
    void skip_line_comment();
    void skip_block_comment();

    // Token scanning
    Token scan_number();
    Token scan_hex_number();
    Token scan_identifier();
    Token make_token(TokenType type, const std::string& text);
    Token make_number_token(const std::string& text, double value);
    Token make_error_token(const std::string& message);
};

// ─────────────────────────────────────────────────────────────────────────────
// Parser: recursive descent parser with operator precedence
// ─────────────────────────────────────────────────────────────────────────────
class ExpressionParser {
public:
    explicit ExpressionParser(const std::string& source);

    // Parse the entire input into an AST
    std::unique_ptr<ASTNode> parse();

    // Parse a single expression
    std::unique_ptr<ASTNode> parse_expression();

    // Check for errors
    bool has_error() const { return !m_error.empty(); }
    const std::string& error_message() const { return m_error; }
    int error_line() const { return m_error_line; }
    int error_column() const { return m_error_col; }

private:
    ExpressionTokenizer m_tokenizer;
    Token m_current;
    std::string m_error;
    int m_error_line = 0;
    int m_error_col = 0;

    // Token consumption
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& context);
    void set_error(const std::string& msg, const Token& tok);

    // Precedence levels (low to high):
    // 1. Assignment (=)
    // 2. Ternary (?:)
    // 3. Logical OR (||)
    // 4. Logical AND (&&)
    // 5. Equality (==, !=)
    // 6. Comparison (<, >, <=, >=)
    // 7. Addition (+, -)
    // 8. Multiplication (*, /, %)
    // 9. Power (^) — right-associative
    // 10. Unary (-, !)
    // 11. Primary (numbers, variables, function calls, parens)

    std::unique_ptr<ASTNode> parse_statement_list();
    std::unique_ptr<ASTNode> parse_assignment();
    std::unique_ptr<ASTNode> parse_ternary();
    std::unique_ptr<ASTNode> parse_logical_or();
    std::unique_ptr<ASTNode> parse_logical_and();
    std::unique_ptr<ASTNode> parse_equality();
    std::unique_ptr<ASTNode> parse_comparison();
    std::unique_ptr<ASTNode> parse_addition();
    std::unique_ptr<ASTNode> parse_multiplication();
    std::unique_ptr<ASTNode> parse_power();
    std::unique_ptr<ASTNode> parse_unary();
    std::unique_ptr<ASTNode> parse_primary();
    std::unique_ptr<ASTNode> parse_function_call(const std::string& name);
};

} // namespace PixelForge
