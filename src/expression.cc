#include "expression.h"
#include <cmath>
#include <cctype>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace PixelForge {

// ═══════════════════════════════════════════════════════════════════════════════
// Token type name lookup
// ═══════════════════════════════════════════════════════════════════════════════

const char* token_type_name(TokenType type) {
    switch (type) {
    case TokenType::NUMBER:      return "NUMBER";
    case TokenType::IDENTIFIER:  return "IDENTIFIER";
    case TokenType::PLUS:        return "PLUS";
    case TokenType::MINUS:       return "MINUS";
    case TokenType::STAR:        return "STAR";
    case TokenType::SLASH:       return "SLASH";
    case TokenType::PERCENT:     return "PERCENT";
    case TokenType::CARET:       return "CARET";
    case TokenType::LESS:        return "LESS";
    case TokenType::GREATER:     return "GREATER";
    case TokenType::LESS_EQ:     return "LESS_EQ";
    case TokenType::GREATER_EQ:  return "GREATER_EQ";
    case TokenType::EQUAL:       return "EQUAL";
    case TokenType::NOT_EQUAL:   return "NOT_EQUAL";
    case TokenType::AND:         return "AND";
    case TokenType::OR:          return "OR";
    case TokenType::NOT:         return "NOT";
    case TokenType::ASSIGN:      return "ASSIGN";
    case TokenType::QUESTION:    return "QUESTION";
    case TokenType::COLON:       return "COLON";
    case TokenType::LPAREN:      return "LPAREN";
    case TokenType::RPAREN:      return "RPAREN";
    case TokenType::COMMA:       return "COMMA";
    case TokenType::SEMICOLON:   return "SEMICOLON";
    case TokenType::EOF_TOKEN:   return "EOF";
    case TokenType::ERROR_TOKEN: return "ERROR";
    default:                     return "UNKNOWN";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Token methods
// ═══════════════════════════════════════════════════════════════════════════════

bool Token::is_operator() const {
    switch (type) {
    case TokenType::PLUS:
    case TokenType::MINUS:
    case TokenType::STAR:
    case TokenType::SLASH:
    case TokenType::PERCENT:
    case TokenType::CARET:
    case TokenType::LESS:
    case TokenType::GREATER:
    case TokenType::LESS_EQ:
    case TokenType::GREATER_EQ:
    case TokenType::EQUAL:
    case TokenType::NOT_EQUAL:
    case TokenType::AND:
    case TokenType::OR:
    case TokenType::NOT:
    case TokenType::ASSIGN:
        return true;
    default:
        return false;
    }
}

std::string Token::to_string() const {
    std::ostringstream oss;
    oss << token_type_name(type) << "('" << text << "'";
    if (type == TokenType::NUMBER) {
        oss << ", val=" << number_value;
    }
    oss << " @" << line << ":" << column << ")";
    return oss.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
// AST type name lookup
// ═══════════════════════════════════════════════════════════════════════════════

const char* ast_type_name(ASTType type) {
    switch (type) {
    case ASTType::Number:        return "Number";
    case ASTType::Variable:      return "Variable";
    case ASTType::UnaryOp:       return "UnaryOp";
    case ASTType::BinaryOp:      return "BinaryOp";
    case ASTType::FunctionCall:  return "FunctionCall";
    case ASTType::Ternary:       return "Ternary";
    case ASTType::Assignment:    return "Assignment";
    case ASTType::StatementList: return "StatementList";
    default:                     return "Unknown";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ASTNode factory methods
// ═══════════════════════════════════════════════════════════════════════════════

std::unique_ptr<ASTNode> ASTNode::make_number(double val) {
    auto node = std::make_unique<ASTNode>(ASTType::Number);
    node->value = val;
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_variable(const std::string& name) {
    auto node = std::make_unique<ASTNode>(ASTType::Variable);
    node->name = name;
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_unary(const std::string& op,
                                              std::unique_ptr<ASTNode> operand) {
    auto node = std::make_unique<ASTNode>(ASTType::UnaryOp);
    node->op = op;
    node->children.push_back(std::move(operand));
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_binary(const std::string& op,
                                               std::unique_ptr<ASTNode> left,
                                               std::unique_ptr<ASTNode> right) {
    auto node = std::make_unique<ASTNode>(ASTType::BinaryOp);
    node->op = op;
    node->children.push_back(std::move(left));
    node->children.push_back(std::move(right));
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_function_call(
    const std::string& name,
    std::vector<std::unique_ptr<ASTNode>> args) {
    auto node = std::make_unique<ASTNode>(ASTType::FunctionCall);
    node->name = name;
    node->children = std::move(args);
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_ternary(
    std::unique_ptr<ASTNode> condition,
    std::unique_ptr<ASTNode> true_branch,
    std::unique_ptr<ASTNode> false_branch) {
    auto node = std::make_unique<ASTNode>(ASTType::Ternary);
    node->children.push_back(std::move(condition));
    node->children.push_back(std::move(true_branch));
    node->children.push_back(std::move(false_branch));
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_assignment(const std::string& var_name,
                                                    std::unique_ptr<ASTNode> value) {
    auto node = std::make_unique<ASTNode>(ASTType::Assignment);
    node->name = var_name;
    node->children.push_back(std::move(value));
    return node;
}

std::unique_ptr<ASTNode> ASTNode::make_statement_list(
    std::vector<std::unique_ptr<ASTNode>> statements) {
    auto node = std::make_unique<ASTNode>(ASTType::StatementList);
    node->children = std::move(statements);
    return node;
}

std::unique_ptr<ASTNode> ASTNode::clone() const {
    auto copy = std::make_unique<ASTNode>(type);
    copy->value = value;
    copy->name = name;
    copy->op = op;
    for (auto& child : children) {
        if (child) {
            copy->children.push_back(child->clone());
        }
    }
    return copy;
}

std::string ASTNode::dump(int indent) const {
    std::string pad(static_cast<size_t>(indent * 2), ' ');
    std::ostringstream oss;
    oss << pad << ast_type_name(type);

    switch (type) {
    case ASTType::Number:
        oss << "(" << value << ")";
        break;
    case ASTType::Variable:
        oss << "(" << name << ")";
        break;
    case ASTType::UnaryOp:
        oss << "(" << op << ")";
        break;
    case ASTType::BinaryOp:
        oss << "(" << op << ")";
        break;
    case ASTType::FunctionCall:
        oss << "(" << name << ", " << children.size() << " args)";
        break;
    case ASTType::Assignment:
        oss << "(" << name << " =)";
        break;
    case ASTType::Ternary:
        oss << "(? :)";
        break;
    case ASTType::StatementList:
        oss << "(" << children.size() << " statements)";
        break;
    }

    oss << "\n";
    for (auto& child : children) {
        if (child) {
            oss << child->dump(indent + 1);
        }
    }
    return oss.str();
}

int ASTNode::node_count() const {
    int count = 1;
    for (auto& child : children) {
        if (child) count += child->node_count();
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ExpressionTokenizer implementation
// ═══════════════════════════════════════════════════════════════════════════════

ExpressionTokenizer::ExpressionTokenizer(const std::string& source)
    : m_source(source), m_pos(0), m_line(1), m_column(1),
      m_has_peek(false) {}

char ExpressionTokenizer::current() const {
    if (m_pos >= m_source.size()) return '\0';
    return m_source[m_pos];
}

char ExpressionTokenizer::look_ahead(int offset) const {
    size_t idx = m_pos + static_cast<size_t>(offset);
    if (idx >= m_source.size()) return '\0';
    return m_source[idx];
}

bool ExpressionTokenizer::at_end() const {
    return m_pos >= m_source.size();
}

void ExpressionTokenizer::advance() {
    if (m_pos < m_source.size()) {
        if (m_source[m_pos] == '\n') {
            m_line++;
            m_column = 1;
        } else {
            m_column++;
        }
        m_pos++;
    }
}

void ExpressionTokenizer::advance_n(int n) {
    for (int i = 0; i < n; i++) advance();
}

void ExpressionTokenizer::skip_whitespace_and_comments() {
    while (!at_end()) {
        char c = current();

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }

        // Skip line comments (//)
        if (c == '/' && look_ahead() == '/') {
            skip_line_comment();
            continue;
        }

        // Skip block comments (/* ... */)
        if (c == '/' && look_ahead() == '*') {
            skip_block_comment();
            continue;
        }

        break;
    }
}

void ExpressionTokenizer::skip_line_comment() {
    // Skip the //
    advance_n(2);
    // Skip to end of line
    while (!at_end() && current() != '\n') {
        advance();
    }
    // Skip the newline itself
    if (!at_end()) advance();
}

void ExpressionTokenizer::skip_block_comment() {
    // Skip the /*
    advance_n(2);
    int depth = 1;
    while (!at_end() && depth > 0) {
        if (current() == '/' && look_ahead() == '*') {
            depth++;
            advance_n(2);
        } else if (current() == '*' && look_ahead() == '/') {
            depth--;
            advance_n(2);
        } else {
            advance();
        }
    }
}

Token ExpressionTokenizer::scan_number() {
    int start_line = m_line;
    int start_col = m_column;
    size_t start_pos = m_pos;

    // Integer part
    while (!at_end() && std::isdigit(static_cast<unsigned char>(current()))) {
        advance();
    }

    // Decimal part
    if (!at_end() && current() == '.' && std::isdigit(static_cast<unsigned char>(look_ahead()))) {
        advance(); // skip '.'
        while (!at_end() && std::isdigit(static_cast<unsigned char>(current()))) {
            advance();
        }
    }

    // Exponent part (e.g., 1e5, 3.14e-2)
    if (!at_end() && (current() == 'e' || current() == 'E')) {
        advance();
        if (!at_end() && (current() == '+' || current() == '-')) {
            advance();
        }
        while (!at_end() && std::isdigit(static_cast<unsigned char>(current()))) {
            advance();
        }
    }

    std::string text = m_source.substr(start_pos, m_pos - start_pos);

    // Parse the numeric value
    double value = 0.0;
    try {
        value = std::stod(text);
    } catch (...) {
        return make_error_token("Invalid number literal: " + text);
    }

    return make_number_token(text, value);
}

Token ExpressionTokenizer::scan_hex_number() {
    int start_line = m_line;
    int start_col = m_column;
    size_t start_pos = m_pos;

    // Skip "0x" or "0X"
    advance_n(2);

    // Read hex digits
    while (!at_end() && std::isxdigit(static_cast<unsigned char>(current()))) {
        advance();
    }

    std::string text = m_source.substr(start_pos, m_pos - start_pos);

    // Parse hex value
    double value = 0.0;
    try {
        unsigned long long ival = std::stoull(text, nullptr, 16);
        value = static_cast<double>(ival);
    } catch (...) {
        return make_error_token("Invalid hex literal: " + text);
    }

    return Token(TokenType::NUMBER, text, value, start_line, start_col);
}

Token ExpressionTokenizer::scan_identifier() {
    int start_line = m_line;
    int start_col = m_column;
    size_t start_pos = m_pos;

    while (!at_end() && (std::isalnum(static_cast<unsigned char>(current())) ||
                          current() == '_')) {
        advance();
    }

    std::string text = m_source.substr(start_pos, m_pos - start_pos);
    return Token(TokenType::IDENTIFIER, text, start_line, start_col);
}

Token ExpressionTokenizer::make_token(TokenType type, const std::string& text) {
    return Token(type, text, m_line, m_column);
}

Token ExpressionTokenizer::make_number_token(const std::string& text, double value) {
    return Token(TokenType::NUMBER, text, value, m_line, m_column);
}

Token ExpressionTokenizer::make_error_token(const std::string& message) {
    m_error = message;
    return Token(TokenType::ERROR_TOKEN, message, m_line, m_column);
}

Token ExpressionTokenizer::next_token() {
    // Return peeked token if available
    if (m_has_peek) {
        m_has_peek = false;
        return m_peek_token;
    }

    skip_whitespace_and_comments();

    if (at_end()) {
        return Token(TokenType::EOF_TOKEN, "", m_line, m_column);
    }

    char c = current();
    int start_line = m_line;
    int start_col = m_column;

    // Number literals
    if (std::isdigit(static_cast<unsigned char>(c))) {
        // Check for hex prefix 0x
        if (c == '0' && (look_ahead() == 'x' || look_ahead() == 'X')) {
            return scan_hex_number();
        }
        return scan_number();
    }

    // Leading decimal point number (.5 etc)
    if (c == '.' && std::isdigit(static_cast<unsigned char>(look_ahead()))) {
        return scan_number();
    }

    // Identifiers
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return scan_identifier();
    }

    // Two-character operators
    char next = look_ahead();

    if (c == '<' && next == '=') {
        advance_n(2);
        return Token(TokenType::LESS_EQ, "<=", start_line, start_col);
    }
    if (c == '>' && next == '=') {
        advance_n(2);
        return Token(TokenType::GREATER_EQ, ">=", start_line, start_col);
    }
    if (c == '=' && next == '=') {
        advance_n(2);
        return Token(TokenType::EQUAL, "==", start_line, start_col);
    }
    if (c == '!' && next == '=') {
        advance_n(2);
        return Token(TokenType::NOT_EQUAL, "!=", start_line, start_col);
    }
    if (c == '&' && next == '&') {
        advance_n(2);
        return Token(TokenType::AND, "&&", start_line, start_col);
    }
    if (c == '|' && next == '|') {
        advance_n(2);
        return Token(TokenType::OR, "||", start_line, start_col);
    }

    // Single-character operators and delimiters
    advance();
    switch (c) {
    case '+':  return Token(TokenType::PLUS,      "+",  start_line, start_col);
    case '-':  return Token(TokenType::MINUS,     "-",  start_line, start_col);
    case '*':  return Token(TokenType::STAR,      "*",  start_line, start_col);
    case '/':  return Token(TokenType::SLASH,     "/",  start_line, start_col);
    case '%':  return Token(TokenType::PERCENT,   "%",  start_line, start_col);
    case '^':  return Token(TokenType::CARET,     "^",  start_line, start_col);
    case '<':  return Token(TokenType::LESS,      "<",  start_line, start_col);
    case '>':  return Token(TokenType::GREATER,   ">",  start_line, start_col);
    case '!':  return Token(TokenType::NOT,       "!",  start_line, start_col);
    case '=':  return Token(TokenType::ASSIGN,    "=",  start_line, start_col);
    case '?':  return Token(TokenType::QUESTION,  "?",  start_line, start_col);
    case ':':  return Token(TokenType::COLON,     ":",  start_line, start_col);
    case '(':  return Token(TokenType::LPAREN,    "(",  start_line, start_col);
    case ')':  return Token(TokenType::RPAREN,    ")",  start_line, start_col);
    case ',':  return Token(TokenType::COMMA,     ",",  start_line, start_col);
    case ';':  return Token(TokenType::SEMICOLON, ";",  start_line, start_col);
    default:
        break;
    }

    // Unrecognized character
    std::string err_msg = "Unexpected character '";
    err_msg += c;
    err_msg += "' at line ";
    err_msg += std::to_string(start_line);
    err_msg += ", column ";
    err_msg += std::to_string(start_col);
    return make_error_token(err_msg);
}

Token ExpressionTokenizer::peek() {
    if (!m_has_peek) {
        m_peek_token = next_token();
        m_has_peek = true;
    }
    return m_peek_token;
}

bool ExpressionTokenizer::has_more() const {
    if (m_has_peek) {
        return m_peek_token.type != TokenType::EOF_TOKEN;
    }
    return m_pos < m_source.size();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ExpressionParser implementation — recursive descent with operator precedence
// ═══════════════════════════════════════════════════════════════════════════════

ExpressionParser::ExpressionParser(const std::string& source)
    : m_tokenizer(source) {
    advance(); // Load the first token
}

void ExpressionParser::advance() {
    m_current = m_tokenizer.next_token();
    if (m_current.type == TokenType::ERROR_TOKEN && m_error.empty()) {
        set_error(m_current.text, m_current);
    }
}

bool ExpressionParser::check(TokenType type) const {
    return m_current.type == type;
}

bool ExpressionParser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token ExpressionParser::expect(TokenType type, const std::string& context) {
    if (check(type)) {
        Token tok = m_current;
        advance();
        return tok;
    }

    std::ostringstream oss;
    oss << "Expected " << token_type_name(type)
        << " " << context
        << ", but got " << token_type_name(m_current.type)
        << " '" << m_current.text << "'";
    set_error(oss.str(), m_current);

    return m_current;
}

void ExpressionParser::set_error(const std::string& msg, const Token& tok) {
    if (m_error.empty()) {
        m_error = msg;
        m_error_line = tok.line;
        m_error_col = tok.column;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level parse: handles multiple statements separated by semicolons
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse() {
    auto result = parse_statement_list();
    if (!check(TokenType::EOF_TOKEN) && m_error.empty()) {
        set_error("Unexpected token after expression: '" + m_current.text + "'",
                  m_current);
    }
    return result;
}

std::unique_ptr<ASTNode> ExpressionParser::parse_expression() {
    return parse_assignment();
}

std::unique_ptr<ASTNode> ExpressionParser::parse_statement_list() {
    std::vector<std::unique_ptr<ASTNode>> statements;
    statements.push_back(parse_expression());

    while (match(TokenType::SEMICOLON)) {
        // Allow trailing semicolons
        if (check(TokenType::EOF_TOKEN)) break;
        statements.push_back(parse_expression());
    }

    if (statements.size() == 1) {
        return std::move(statements[0]);
    }

    return ASTNode::make_statement_list(std::move(statements));
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 1: Assignment (right-associative)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_assignment() {
    // We need to check if this is an assignment. An assignment is:
    // IDENTIFIER '=' expression
    // This requires look-ahead, so we try to parse as a ternary first
    // and then check if it's followed by '='

    // Save the current token — if it's an identifier followed by '=',
    // we have an assignment
    if (check(TokenType::IDENTIFIER)) {
        // Peek ahead to see if next token is '='
        Token id_token = m_current;
        Token peeked = m_tokenizer.peek();

        if (peeked.type == TokenType::ASSIGN) {
            // This is an assignment
            advance(); // consume identifier
            advance(); // consume '='
            auto value = parse_assignment(); // Right-associative
            return ASTNode::make_assignment(id_token.text, std::move(value));
        }
    }

    return parse_ternary();
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 2: Ternary (condition ? true_expr : false_expr)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_ternary() {
    auto condition = parse_logical_or();

    if (match(TokenType::QUESTION)) {
        auto true_branch = parse_expression();
        expect(TokenType::COLON, "in ternary expression");
        auto false_branch = parse_ternary();
        return ASTNode::make_ternary(std::move(condition),
                                      std::move(true_branch),
                                      std::move(false_branch));
    }

    return condition;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 3: Logical OR (||)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_logical_or() {
    auto left = parse_logical_and();

    while (check(TokenType::OR)) {
        advance();
        auto right = parse_logical_and();
        left = ASTNode::make_binary("||", std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 4: Logical AND (&&)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_logical_and() {
    auto left = parse_equality();

    while (check(TokenType::AND)) {
        advance();
        auto right = parse_equality();
        left = ASTNode::make_binary("&&", std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 5: Equality (==, !=)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_equality() {
    auto left = parse_comparison();

    while (check(TokenType::EQUAL) || check(TokenType::NOT_EQUAL)) {
        std::string op = m_current.text;
        advance();
        auto right = parse_comparison();
        left = ASTNode::make_binary(op, std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 6: Comparison (<, >, <=, >=)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_comparison() {
    auto left = parse_addition();

    while (check(TokenType::LESS) || check(TokenType::GREATER) ||
           check(TokenType::LESS_EQ) || check(TokenType::GREATER_EQ)) {
        std::string op = m_current.text;
        advance();
        auto right = parse_addition();
        left = ASTNode::make_binary(op, std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 7: Addition (+, -)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_addition() {
    auto left = parse_multiplication();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        std::string op = m_current.text;
        advance();
        auto right = parse_multiplication();
        left = ASTNode::make_binary(op, std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 8: Multiplication (*, /, %)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_multiplication() {
    auto left = parse_power();

    while (check(TokenType::STAR) || check(TokenType::SLASH) ||
           check(TokenType::PERCENT)) {
        std::string op = m_current.text;
        advance();
        auto right = parse_power();
        left = ASTNode::make_binary(op, std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 9: Power (^) — right-associative
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_power() {
    auto left = parse_unary();

    if (check(TokenType::CARET)) {
        advance();
        auto right = parse_power(); // Right-associative: recurse into same level
        left = ASTNode::make_binary("^", std::move(left), std::move(right));
    }

    return left;
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 10: Unary (-, !)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_unary() {
    if (check(TokenType::MINUS)) {
        advance();
        auto operand = parse_unary(); // Allow chaining: --x
        return ASTNode::make_unary("-", std::move(operand));
    }

    if (check(TokenType::NOT)) {
        advance();
        auto operand = parse_unary();
        return ASTNode::make_unary("!", std::move(operand));
    }

    // Unary plus: just skip it
    if (check(TokenType::PLUS)) {
        advance();
        return parse_unary();
    }

    return parse_primary();
}

// ─────────────────────────────────────────────────────────────────────────────
// Precedence level 11: Primary (numbers, variables, function calls, parens)
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<ASTNode> ExpressionParser::parse_primary() {
    // Number literal
    if (check(TokenType::NUMBER)) {
        double val = m_current.number_value;
        advance();
        return ASTNode::make_number(val);
    }

    // Identifier: could be variable or function call
    if (check(TokenType::IDENTIFIER)) {
        std::string name = m_current.text;
        advance();

        // Check for function call: identifier followed by '('
        if (check(TokenType::LPAREN)) {
            return parse_function_call(name);
        }

        // Otherwise it's a variable reference
        return ASTNode::make_variable(name);
    }

    // Parenthesized expression
    if (check(TokenType::LPAREN)) {
        advance();
        auto expr = parse_expression();
        expect(TokenType::RPAREN, "after parenthesized expression");
        return expr;
    }

    // Error: unexpected token
    std::ostringstream oss;
    oss << "Unexpected token '" << m_current.text
        << "' (" << token_type_name(m_current.type) << ")"
        << " at line " << m_current.line << ", column " << m_current.column;
    set_error(oss.str(), m_current);

    // Return a dummy number node to allow continued parsing
    advance();
    return ASTNode::make_number(0.0);
}

std::unique_ptr<ASTNode> ExpressionParser::parse_function_call(
    const std::string& name) {
    expect(TokenType::LPAREN, "after function name");

    std::vector<std::unique_ptr<ASTNode>> args;

    // Empty argument list
    if (check(TokenType::RPAREN)) {
        advance();
        return ASTNode::make_function_call(name, std::move(args));
    }

    // Parse first argument
    args.push_back(parse_expression());

    // Parse remaining arguments separated by commas
    while (match(TokenType::COMMA)) {
        args.push_back(parse_expression());
    }

    expect(TokenType::RPAREN, "after function arguments");

    return ASTNode::make_function_call(name, std::move(args));
}

} // namespace PixelForge
