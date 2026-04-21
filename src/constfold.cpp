#include "../include/constfold.h"
#include <cmath>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers — extract literal values
// ─────────────────────────────────────────────────────────────────────────────

bool ConstantFolder::asInt(const Expr& e, int& out) {
    if (auto* p = std::get_if<std::unique_ptr<IntLitExpr>>(&e)) {
        out = (*p)->value; return true;
    }
    return false;
}

bool ConstantFolder::asFloat(const Expr& e, float& out) {
    if (auto* p = std::get_if<std::unique_ptr<FloatLitExpr>>(&e)) {
        out = (*p)->value; return true;
    }
    // Int literals can widen to float
    if (auto* p = std::get_if<std::unique_ptr<IntLitExpr>>(&e)) {
        out = static_cast<float>((*p)->value); return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Core fold: BinOpExpr
// ─────────────────────────────────────────────────────────────────────────────

Expr ConstantFolder::foldBinOp(BinOpExpr& b) {
    // Recursively fold children first
    b.left  = foldExpr(std::move(b.left));
    b.right = foldExpr(std::move(b.right));

    const std::string& op = b.op;
    int line = b.line;

    // ── Integer constant folding ─────────────────────────────────────────────
    int lI, rI;
    if (asInt(b.left, lI) && asInt(b.right, rI)) {
        int result = 0;
        bool folded = true;
        if      (op == "+")  result = lI + rI;
        else if (op == "-")  result = lI - rI;
        else if (op == "*")  result = lI * rI;
        else if (op == "/")  { if (rI == 0) folded = false; else result = lI / rI; }
        else if (op == "%")  { if (rI == 0) folded = false; else result = lI % rI; }
        else if (op == "<")  result = lI <  rI ? 1 : 0;
        else if (op == ">")  result = lI >  rI ? 1 : 0;
        else if (op == "<=") result = lI <= rI ? 1 : 0;
        else if (op == ">=") result = lI >= rI ? 1 : 0;
        else if (op == "==") result = lI == rI ? 1 : 0;
        else if (op == "!=") result = lI != rI ? 1 : 0;
        else if (op == "<<") result = lI << rI;
        else if (op == ">>") result = lI >> rI;
        else if (op == "&")  result = lI &  rI;
        else if (op == "|")  result = lI |  rI;
        else if (op == "^")  result = lI ^  rI;
        else folded = false;

        if (folded) {
            auto n = std::make_unique<IntLitExpr>();
            n->value = result; n->line = line;
            return n;
        }
    }

    // ── Float constant folding ───────────────────────────────────────────────
    float lF, rF;
    if (asFloat(b.left, lF) && asFloat(b.right, rF)) {
        bool folded = true;
        float result = 0.0f;
        if      (op == "+") result = lF + rF;
        else if (op == "-") result = lF - rF;
        else if (op == "*") result = lF * rF;
        else if (op == "/") { if (rF == 0.0f) folded = false; else result = lF / rF; }
        else folded = false;

        if (folded) {
            auto n = std::make_unique<FloatLitExpr>();
            n->value = result; n->line = line;
            return n;
        }
    }

    // ── Identity folding: x + 0, x * 1, x - 0, x * 0 ───────────────────────
    int rVal;
    if (asInt(b.right, rVal)) {
        if ((op == "+" || op == "-") && rVal == 0)
            return std::move(b.left);
        if (op == "*" && rVal == 1)
            return std::move(b.left);
        if (op == "*" && rVal == 0) {
            auto n = std::make_unique<IntLitExpr>();
            n->value = 0; n->line = line; return n;
        }
        if (op == "/" && rVal == 1)
            return std::move(b.left);
    }
    int lVal;
    if (asInt(b.left, lVal)) {
        if (op == "+" && lVal == 0)
            return std::move(b.right);
        if (op == "*" && lVal == 1)
            return std::move(b.right);
        if (op == "*" && lVal == 0) {
            auto n = std::make_unique<IntLitExpr>();
            n->value = 0; n->line = line; return n;
        }
    }

    // Not foldable — rebuild as BinOp
    auto node = std::make_unique<BinOpExpr>();
    node->op    = b.op;
    node->left  = std::move(b.left);
    node->right = std::move(b.right);
    node->line  = b.line;
    return node;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Expr / Stmt / Block traversal
// ─────────────────────────────────────────────────────────────────────────────

Expr ConstantFolder::foldExpr(Expr e) {
    return std::visit([&](auto& node) -> Expr {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<BinOpExpr>>) {
            return foldBinOp(*node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpr>>) {
            node->array = foldExpr(std::move(node->array));
            node->index = foldExpr(std::move(node->index));
            return std::move(node);
        }
        // Literals, Name, ThreadIdx, BlockIdx, BlockDim — already atomic
        return std::move(node);
    }, e);
}

Stmt ConstantFolder::foldStmt(Stmt s) {
    return std::visit([&](auto& node) -> Stmt {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, std::unique_ptr<VarDeclStmt>>) {
            if (node->init) node->init = foldExpr(std::move(*node->init));
            return std::move(node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<AssignStmt>>) {
            node->target = foldExpr(std::move(node->target));
            node->value  = foldExpr(std::move(node->value));
            return std::move(node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<ForStmt>>) {
            if (node->init->init) node->init->init = foldExpr(std::move(*node->init->init));
            node->cond      = foldExpr(std::move(node->cond));
            node->step->value = foldExpr(std::move(node->step->value));
            node->body      = foldBlock(std::move(node->body));
            return std::move(node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<IfStmt>>) {
            node->cond     = foldExpr(std::move(node->cond));
            node->thenBody = foldBlock(std::move(node->thenBody));
            node->elseBody = foldBlock(std::move(node->elseBody));
            return std::move(node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnStmt>>) {
            if (node->value) node->value = foldExpr(std::move(*node->value));
            return std::move(node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<ExprStmt>>) {
            node->expr = foldExpr(std::move(node->expr));
            return std::move(node);
        }
        return std::move(node);
    }, s);
}

std::vector<Stmt> ConstantFolder::foldBlock(std::vector<Stmt> stmts) {
    for (auto& s : stmts) s = foldStmt(std::move(s));
    return stmts;
}

KernelDef ConstantFolder::foldKernel(KernelDef kd) {
    kd.body = foldBlock(std::move(kd.body));
    return kd;
}

Program ConstantFolder::fold(Program prog) {
    for (auto& k : prog.kernels)
        k = foldKernel(std::move(k));
    return prog;
}
