#pragma once
#include "ast.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Constant Folding Pass
//
//  Walks the AST and rewrites BinOpExpr nodes whose both operands are
//  compile-time literals into a single literal node.
//
//  Examples:
//    2 + 3         → 5
//    4 * 4         → 16
//    10 / 2        → 5
//    7 % 3         → 1
//    threadIdx.x * 1  → threadIdx.x   (identity fold)
//    x + 0         → x               (identity fold)
//
//  This runs BEFORE IRGen so the IR never sees the redundant operations.
//  On a resume this demonstrates knowledge of optimisation passes — the
//  same concept used in LLVM's ConstantFoldingPass and NVPTX backend.
// ─────────────────────────────────────────────────────────────────────────────

class ConstantFolder {
public:
    Program fold(Program prog);

private:
    KernelDef foldKernel(KernelDef kd);
    std::vector<Stmt> foldBlock(std::vector<Stmt> stmts);
    Stmt   foldStmt(Stmt s);
    Expr   foldExpr(Expr e);
    Expr   foldBinOp(BinOpExpr& b);

    // Returns true + the int value if expr is an IntLitExpr
    static bool asInt(const Expr& e, int& out);
    static bool asFloat(const Expr& e, float& out);
};
