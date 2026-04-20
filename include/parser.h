#pragma once
#include "lexer.h"
#include "ast.h"
#include <stdexcept>

class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& msg) : std::runtime_error(msg) {}
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    // Helpers
    Token& peek(int offset = 0);
    Token  advance();
    bool   check(TokenKind k);
    bool   check2(TokenKind k);  // peek ahead 1
    Token  expect(TokenKind k, const std::string& msg = "");
    bool   match(TokenKind k);

    // Grammar rules
    KernelDef              parseKernel();
    std::vector<Param>     parseParamList();
    Param                  parseParam();
    std::vector<Stmt>      parseBlock();
    Stmt                   parseStmt();
    std::unique_ptr<VarDeclStmt> parseVarDecl();
    std::unique_ptr<ForStmt>     parseFor();
    std::unique_ptr<IfStmt>      parseIf();
    std::unique_ptr<ReturnStmt>  parseReturn();
    Stmt                         parseAssignOrExprStmt();

    Expr parseExpr();
    Expr parseAdditive();
    Expr parseMultiplicative();
    Expr parseComparison();
    Expr parsePrimary();
    Expr parseThreadVar();
};
