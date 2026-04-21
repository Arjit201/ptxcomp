#include "../include/parser.h"
#include <sstream>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// ── Helpers ───────────────────────────────────────────────────────────────────

Token& Parser::peek(int offset) {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back(); // EOF
    return tokens_[idx];
}

Token Parser::advance() {
    Token t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) ++pos_;
    return t;
}

bool Parser::check(TokenKind k) { return peek().kind == k; }
bool Parser::check2(TokenKind k) { return peek(1).kind == k; }

Token Parser::expect(TokenKind k, const std::string& msg) {
    if (!check(k)) {
        std::ostringstream ss;
        ss << "Line " << peek().line << ": expected "
           << static_cast<int>(k) << " (" << msg << "), got '"
           << peek().value << "'";
        throw ParseError(ss.str());
    }
    return advance();
}

bool Parser::match(TokenKind k) {
    if (check(k)) { advance(); return true; }
    return false;
}

// ── Top-level ─────────────────────────────────────────────────────────────────

Program Parser::parse() {
    Program prog;
    while (!check(TokenKind::Eof))
        prog.kernels.push_back(parseKernel());
    return prog;
}

// ── Kernel ────────────────────────────────────────────────────────────────────

KernelDef Parser::parseKernel() {
    int line = peek().line;
    expect(TokenKind::KwKernel, "kernel");
    std::string name = expect(TokenKind::Ident, "kernel name").value;
    expect(TokenKind::LParen, "(");
    auto params = parseParamList();
    expect(TokenKind::RParen, ")");
    auto body = parseBlock();
    return KernelDef{name, std::move(params), std::move(body), line};
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;
    if (check(TokenKind::KwInt) || check(TokenKind::KwFloat)) {
        params.push_back(parseParam());
        while (match(TokenKind::Comma))
            params.push_back(parseParam());
    }
    return params;
}

Param Parser::parseParam() {
    int line = peek().line;
    std::string dtype = advance().value;  // int or float
    std::string name  = expect(TokenKind::Ident, "param name").value;
    bool isArray = false;
    if (match(TokenKind::LBracket)) {
        expect(TokenKind::RBracket, "]");
        isArray = true;
    }
    return Param{dtype, name, isArray, line};
}

// ── Block & Statements ────────────────────────────────────────────────────────

std::vector<Stmt> Parser::parseBlock() {
    expect(TokenKind::LBrace, "{");
    std::vector<Stmt> stmts;
    while (!check(TokenKind::RBrace) && !check(TokenKind::Eof))
        stmts.push_back(parseStmt());
    expect(TokenKind::RBrace, "}");
    return stmts;
}

Stmt Parser::parseStmt() {
    if (check(TokenKind::KwInt) || check(TokenKind::KwFloat))
        return parseVarDecl();
    if (check(TokenKind::KwFor))    return parseFor();
    if (check(TokenKind::KwIf))     return parseIf();
    if (check(TokenKind::KwReturn)) return parseReturn();
    return parseAssignOrExprStmt();
}

std::unique_ptr<VarDeclStmt> Parser::parseVarDecl() {
    int line = peek().line;
    std::string dtype = advance().value;
    std::string name  = expect(TokenKind::Ident, "variable name").value;
    std::optional<Expr> init;
    if (match(TokenKind::Assign))
        init = parseExpr();
    expect(TokenKind::Semi, ";");
    auto s = std::make_unique<VarDeclStmt>();
    s->dtype = dtype; s->name = name;
    s->init = std::move(init); s->line = line;
    return s;
}

std::unique_ptr<ForStmt> Parser::parseFor() {
    int line = peek().line;
    expect(TokenKind::KwFor, "for");
    expect(TokenKind::LParen, "(");

    // init — always a var decl (with trailing ;)
    auto init = parseVarDecl();

    auto cond = parseExpr();
    expect(TokenKind::Semi, ";");

    // step: expr = expr
    Expr stepTarget = parseExpr();
    expect(TokenKind::Assign, "=");
    Expr stepVal = parseExpr();
    auto step = std::make_unique<AssignStmt>();
    step->target = std::move(stepTarget);
    step->value  = std::move(stepVal);
    step->line   = line;

    expect(TokenKind::RParen, ")");
    auto body = parseBlock();

    auto s = std::make_unique<ForStmt>();
    s->init = std::move(init);
    s->cond = std::move(cond);
    s->step = std::move(step);
    s->body = std::move(body);
    s->line = line;
    return s;
}

std::unique_ptr<IfStmt> Parser::parseIf() {
    int line = peek().line;
    expect(TokenKind::KwIf, "if");
    expect(TokenKind::LParen, "(");
    auto cond = parseExpr();
    expect(TokenKind::RParen, ")");
    auto thenBody = parseBlock();
    std::vector<Stmt> elseBody;
    if (match(TokenKind::KwElse))
        elseBody = parseBlock();
    auto s = std::make_unique<IfStmt>();
    s->cond = std::move(cond);
    s->thenBody = std::move(thenBody);
    s->elseBody = std::move(elseBody);
    s->line = line;
    return s;
}

std::unique_ptr<ReturnStmt> Parser::parseReturn() {
    int line = peek().line;
    expect(TokenKind::KwReturn, "return");
    std::optional<Expr> val;
    if (!check(TokenKind::Semi))
        val = parseExpr();
    expect(TokenKind::Semi, ";");
    auto s = std::make_unique<ReturnStmt>();
    s->value = std::move(val); s->line = line;
    return s;
}

Stmt Parser::parseAssignOrExprStmt() {
    int line = peek().line;
    Expr lhs = parseExpr();
    if (match(TokenKind::Assign)) {
        Expr rhs = parseExpr();
        expect(TokenKind::Semi, ";");
        auto s = std::make_unique<AssignStmt>();
        s->target = std::move(lhs); s->value = std::move(rhs); s->line = line;
        return s;
    }
    expect(TokenKind::Semi, ";");
    auto s = std::make_unique<ExprStmt>();
    s->expr = std::move(lhs); s->line = line;
    return s;
}

// ── Expressions ───────────────────────────────────────────────────────────────

Expr Parser::parseExpr()           { return parseAdditive(); }

Expr Parser::parseAdditive() {
    Expr left = parseMultiplicative();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        int l = peek().line;
        std::string op = advance().value;
        Expr right = parseMultiplicative();
        auto b = std::make_unique<BinOpExpr>();
        b->op = op; b->left = std::move(left); b->right = std::move(right); b->line = l;
        left = std::move(b);
    }
    return left;
}

Expr Parser::parseMultiplicative() {
    Expr left = parseComparison();
    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
        int l = peek().line;
        std::string op = advance().value;
        Expr right = parseComparison();
        auto b = std::make_unique<BinOpExpr>();
        b->op = op; b->left = std::move(left); b->right = std::move(right); b->line = l;
        left = std::move(b);
    }
    return left;
}

Expr Parser::parseComparison() {
    Expr left = parsePrimary();
    while (check(TokenKind::Lt)  || check(TokenKind::Gt)  ||
           check(TokenKind::Le)  || check(TokenKind::Ge)  ||
           check(TokenKind::EqEq)|| check(TokenKind::NotEq)) {
        int l = peek().line;
        std::string op = advance().value;
        Expr right = parsePrimary();
        auto b = std::make_unique<BinOpExpr>();
        b->op = op; b->left = std::move(left); b->right = std::move(right); b->line = l;
        left = std::move(b);
    }
    return left;
}

Expr Parser::parsePrimary() {
    Token& t = peek();

    if (t.kind == TokenKind::IntLit) {
        advance();
        auto n = std::make_unique<IntLitExpr>();
        n->value = std::stoi(t.value); n->line = t.line;
        return n;
    }
    if (t.kind == TokenKind::FloatLit) {
        advance();
        auto n = std::make_unique<FloatLitExpr>();
        n->value = std::stof(t.value); n->line = t.line;
        return n;
    }
    if (t.kind == TokenKind::KwThreadIdx ||
        t.kind == TokenKind::KwBlockIdx  ||
        t.kind == TokenKind::KwBlockDim) {
        return parseThreadVar();
    }
    if (t.kind == TokenKind::Ident) {
        Token nameTok = advance();
        Expr node = std::make_unique<NameExpr>(NameExpr{nameTok.value, nameTok.line});
        while (check(TokenKind::LBracket)) {
            int l = peek().line;
            advance(); // [
            Expr idx = parseExpr();
            expect(TokenKind::RBracket, "]");
            auto ie = std::make_unique<IndexExpr>();
            ie->array = std::move(node); ie->index = std::move(idx); ie->line = l;
            node = std::move(ie);
        }
        return node;
    }
    if (t.kind == TokenKind::LParen) {
        advance();
        Expr inner = parseExpr();
        expect(TokenKind::RParen, ")");
        return inner;
    }
    std::ostringstream ss;
    ss << "Line " << t.line << ": unexpected '" << t.value << "' in expression";
    throw ParseError(ss.str());
}

Expr Parser::parseThreadVar() {
    Token src = advance();
    expect(TokenKind::Dot, ".");
    Token dim = expect(TokenKind::Ident, "x/y/z");
    if (dim.value != "x" && dim.value != "y" && dim.value != "z")
        throw ParseError("Expected x/y/z dimension at line " + std::to_string(dim.line));
    char d = dim.value[0];
    if (src.kind == TokenKind::KwThreadIdx) {
        auto n = std::make_unique<ThreadIdxExpr>(); n->dim = d; n->line = src.line; return n;
    }
    if (src.kind == TokenKind::KwBlockIdx) {
        auto n = std::make_unique<BlockIdxExpr>();  n->dim = d; n->line = src.line; return n;
    }
    auto n = std::make_unique<BlockDimExpr>(); n->dim = d; n->line = src.line; return n;
}
