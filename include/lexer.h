#pragma once
#include <string>
#include <vector>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Token kinds
// ─────────────────────────────────────────────────────────────────────────────
enum class TokenKind {
    // Literals
    IntLit, FloatLit,
    // Identifiers
    Ident,
    // Keywords
    KwKernel, KwInt, KwFloat, KwFor, KwIf, KwReturn,
    KwThreadIdx, KwBlockIdx, KwBlockDim,
    // Operators
    Plus, Minus, Star, Slash, Percent,
    Lt, Gt, Le, Ge, EqEq, NotEq,
    Assign,
    Amp, Pipe, Caret, Tilde, LtLt, GtGt,
    // Delimiters
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semi, Dot,
    // End
    Eof
};

struct Token {
    TokenKind kind;
    std::string value;
    int line, col;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Lexer
// ─────────────────────────────────────────────────────────────────────────────
class Lexer {
public:
    explicit Lexer(std::string src);
    std::vector<Token> tokenize();

private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1, col_ = 1;

    char peek(int offset = 0) const;
    char advance();
    void skipWhitespaceAndComments();
    Token readNumber();
    Token readIdent();
    Token makeToken(TokenKind k, std::string v, int line, int col);
};
