#pragma once
#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
struct IntLitExpr;
struct FloatLitExpr;
struct NameExpr;
struct ThreadIdxExpr;
struct BlockIdxExpr;
struct BlockDimExpr;
struct BinOpExpr;
struct IndexExpr;

// Expr is a pointer to any of the above
using Expr = std::variant<
    std::unique_ptr<IntLitExpr>,
    std::unique_ptr<FloatLitExpr>,
    std::unique_ptr<NameExpr>,
    std::unique_ptr<ThreadIdxExpr>,
    std::unique_ptr<BlockIdxExpr>,
    std::unique_ptr<BlockDimExpr>,
    std::unique_ptr<BinOpExpr>,
    std::unique_ptr<IndexExpr>
>;

// ─────────────────────────────────────────────────────────────────────────────
//  Expression nodes
// ─────────────────────────────────────────────────────────────────────────────
struct IntLitExpr   { int value; int line; };
struct FloatLitExpr { float value; int line; };
struct NameExpr     { std::string name; int line; };

struct ThreadIdxExpr { char dim; int line; }; // dim: 'x','y','z'
struct BlockIdxExpr  { char dim; int line; };
struct BlockDimExpr  { char dim; int line; };

struct BinOpExpr {
    std::string op;
    Expr left, right;
    int line;
};

struct IndexExpr {
    Expr array;
    Expr index;
    int line;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Statement nodes
// ─────────────────────────────────────────────────────────────────────────────
struct VarDeclStmt;
struct AssignStmt;
struct ForStmt;
struct IfStmt;
struct ReturnStmt;
struct ExprStmt;

using Stmt = std::variant<
    std::unique_ptr<VarDeclStmt>,
    std::unique_ptr<AssignStmt>,
    std::unique_ptr<ForStmt>,
    std::unique_ptr<IfStmt>,
    std::unique_ptr<ReturnStmt>,
    std::unique_ptr<ExprStmt>
>;

struct VarDeclStmt {
    std::string dtype;          // "int" | "float"
    std::string name;
    std::optional<Expr> init;
    int line;
};

struct AssignStmt {
    Expr target;
    Expr value;
    int line;
};

struct ForStmt {
    std::unique_ptr<VarDeclStmt> init;
    Expr cond;
    std::unique_ptr<AssignStmt> step;
    std::vector<Stmt> body;
    int line;
};

struct IfStmt {
    Expr cond;
    std::vector<Stmt> thenBody;
    std::vector<Stmt> elseBody;
    int line;
};

struct ReturnStmt {
    std::optional<Expr> value;
    int line;
};

struct ExprStmt {
    Expr expr;
    int line;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Top-level
// ─────────────────────────────────────────────────────────────────────────────
struct Param {
    std::string dtype;
    std::string name;
    bool isArray;
    int line;
};

struct KernelDef {
    std::string name;
    std::vector<Param> params;
    std::vector<Stmt> body;
    int line;
};

struct Program {
    std::vector<KernelDef> kernels;
};
