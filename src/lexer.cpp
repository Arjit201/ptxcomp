#include "../include/lexer.h"
#include <unordered_map>
#include <cctype>
#include <stdexcept>

static const std::unordered_map<std::string,TokenKind> KEYWORDS = {
    {"kernel",   TokenKind::KwKernel},
    {"int",      TokenKind::KwInt},
    {"float",    TokenKind::KwFloat},
    {"for",      TokenKind::KwFor},
    {"if",       TokenKind::KwIf},
    {"else",     TokenKind::KwElse},
    {"return",   TokenKind::KwReturn},
    {"threadIdx",TokenKind::KwThreadIdx},
    {"blockIdx", TokenKind::KwBlockIdx},
    {"blockDim", TokenKind::KwBlockDim},
};

Lexer::Lexer(std::string src) : src_(std::move(src)) {}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + offset;
    return idx < src_.size() ? src_[idx] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; } else { ++col_; }
    return c;
}

void Lexer::skipWhitespaceAndComments() {
    while (pos_ < src_.size()) {
        char c = peek();
        if (std::isspace(c)) { advance(); continue; }
        // line comment
        if (c == '/' && peek(1) == '/') {
            while (pos_ < src_.size() && peek() != '\n') advance();
            continue;
        }
        break;
    }
}

Token Lexer::makeToken(TokenKind k, std::string v, int line, int col) {
    return Token{k, std::move(v), line, col};
}

Token Lexer::readNumber() {
    int startLine = line_, startCol = col_;
    std::string val;
    bool isFloat = false;
    while (pos_ < src_.size() && (std::isdigit(peek()) || peek() == '.')) {
        if (peek() == '.') isFloat = true;
        val += advance();
    }
    return makeToken(isFloat ? TokenKind::FloatLit : TokenKind::IntLit,
                     val, startLine, startCol);
}

Token Lexer::readIdent() {
    int startLine = line_, startCol = col_;
    std::string val;
    while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_'))
        val += advance();
    auto it = KEYWORDS.find(val);
    TokenKind kind = (it != KEYWORDS.end()) ? it->second : TokenKind::Ident;
    return makeToken(kind, val, startLine, startCol);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();
        if (pos_ >= src_.size()) break;

        int l = line_, c = col_;
        char ch = peek();

        // Numbers
        if (std::isdigit(ch) || (ch == '.' && std::isdigit(peek(1)))) {
            tokens.push_back(readNumber()); continue;
        }
        // Identifiers / keywords
        if (std::isalpha(ch) || ch == '_') {
            tokens.push_back(readIdent()); continue;
        }

        // Two-char operators
        advance(); // consume ch
        auto mk = [&](TokenKind k, const char* v) {
            return Token{k, v, l, c};
        };
        switch (ch) {
            case '+': tokens.push_back(mk(TokenKind::Plus,  "+")); break;
            case '-': tokens.push_back(mk(TokenKind::Minus, "-")); break;
            case '*': tokens.push_back(mk(TokenKind::Star,  "*")); break;
            case '/': tokens.push_back(mk(TokenKind::Slash, "/")); break;
            case '%': tokens.push_back(mk(TokenKind::Percent,"%")); break;
            case '^': tokens.push_back(mk(TokenKind::Caret, "^")); break;
            case '~': tokens.push_back(mk(TokenKind::Tilde, "~")); break;
            case '(': tokens.push_back(mk(TokenKind::LParen,"(")); break;
            case ')': tokens.push_back(mk(TokenKind::RParen,")")); break;
            case '{': tokens.push_back(mk(TokenKind::LBrace,"{")); break;
            case '}': tokens.push_back(mk(TokenKind::RBrace,"}")); break;
            case '[': tokens.push_back(mk(TokenKind::LBracket,"[")); break;
            case ']': tokens.push_back(mk(TokenKind::RBracket,"]")); break;
            case ',': tokens.push_back(mk(TokenKind::Comma, ",")); break;
            case ';': tokens.push_back(mk(TokenKind::Semi,  ";")); break;
            case '.': tokens.push_back(mk(TokenKind::Dot,   ".")); break;
            case '&': tokens.push_back(mk(TokenKind::Amp,   "&")); break;
            case '|': tokens.push_back(mk(TokenKind::Pipe,  "|")); break;
            case '<':
                if (peek() == '=') { advance(); tokens.push_back(mk(TokenKind::Le,"<=")); }
                else if (peek() == '<') { advance(); tokens.push_back(mk(TokenKind::LtLt,"<<")); }
                else tokens.push_back(mk(TokenKind::Lt,"<"));
                break;
            case '>':
                if (peek() == '=') { advance(); tokens.push_back(mk(TokenKind::Ge,">=")); }
                else if (peek() == '>') { advance(); tokens.push_back(mk(TokenKind::GtGt,">>")); }
                else tokens.push_back(mk(TokenKind::Gt,">"));
                break;
            case '=':
                if (peek() == '=') { advance(); tokens.push_back(mk(TokenKind::EqEq,"==")); }
                else tokens.push_back(mk(TokenKind::Assign,"="));
                break;
            case '!':
                if (peek() == '=') { advance(); tokens.push_back(mk(TokenKind::NotEq,"!=")); }
                else throw std::runtime_error("Unexpected '!' at " + std::to_string(l));
                break;
            default:
                throw std::runtime_error(
                    std::string("Unknown char '") + ch + "' at line " + std::to_string(l));
        }
    }

    tokens.push_back(Token{TokenKind::Eof, "", line_, col_});
    return tokens;
}
