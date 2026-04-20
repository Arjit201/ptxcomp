#include "../include/irgen.h"
#include <stdexcept>
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
//  Top level
// ─────────────────────────────────────────────────────────────────────────────

IRProgram IRGen::generate(const Program& prog) {
    IRProgram irprog;
    for (auto& k : prog.kernels)
        genKernel(k, irprog);
    return irprog;
}

void IRGen::genKernel(const KernelDef& kd, IRProgram& prog) {
    IRKernel ik;
    ik.name = kd.name;
    cur_ = &ik;
    env_.clear();

    // Register parameters
    for (auto& p : kd.params) {
        IRType t = (p.dtype == "float") ? IRType::Float : IRType::Int;
        std::string reg = p.isArray
            ? ("%rd_" + p.name)   // 64-bit pointer register
            : (t == IRType::Float ? ik.newFloatReg() : ik.newIntReg());
        ik.params.push_back(IRParam{p.name, t, p.isArray});
        env_[p.name] = VarInfo{reg, t, p.isArray};
    }

    for (auto& s : kd.body)
        genStmt(s);

    emit(IR_Ret{});
    prog.kernels.push_back(std::move(ik));
    cur_ = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Statements
// ─────────────────────────────────────────────────────────────────────────────

void IRGen::genStmt(const Stmt& s) {
    std::visit([&](auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<VarDeclStmt>>)
            genVarDecl(*node);
        else if constexpr (std::is_same_v<T, std::unique_ptr<AssignStmt>>)
            genAssign(*node);
        else if constexpr (std::is_same_v<T, std::unique_ptr<ForStmt>>)
            genFor(*node);
        else if constexpr (std::is_same_v<T, std::unique_ptr<IfStmt>>)
            genIf(*node);
        else if constexpr (std::is_same_v<T, std::unique_ptr<ReturnStmt>>)
            genReturn(*node);
        else if constexpr (std::is_same_v<T, std::unique_ptr<ExprStmt>>)
            genExpr(node->expr);
    }, s);
}

void IRGen::genVarDecl(const VarDeclStmt& s) {
    IRType t = (s.dtype == "float") ? IRType::Float : IRType::Int;
    std::string reg = (t == IRType::Float) ? cur_->newFloatReg() : cur_->newIntReg();
    env_[s.name] = VarInfo{reg, t};

    if (s.init) {
        std::string src = genExpr(*s.init, t);
        emit(IR_Assign{reg, src});
    } else {
        // Default-initialise to 0
        emit(IR_Assign{reg, "0"});
    }
}

void IRGen::genAssign(const AssignStmt& s) {
    // LHS can be a Name or an Index
    if (auto* idx = std::get_if<std::unique_ptr<IndexExpr>>(&s.target)) {
        // array[i] = val
        std::string baseReg = genExpr((*idx)->array);
        std::string idxReg  = genExpr((*idx)->index);
        std::string valReg  = genExpr(s.value);
        // Determine type from value register
        IRType t = valReg.find('%') != std::string::npos && valReg[1] == 'f'
                 ? IRType::Float : IRType::Int;
        emit(IR_StoreIdx{baseReg, idxReg, valReg, t});
        return;
    }
    // Simple name
    std::string dst = genExpr(s.target);
    std::string src = genExpr(s.value);
    emit(IR_Assign{dst, src});
}

void IRGen::genFor(const ForStmt& s) {
    genVarDecl(*s.init);

    std::string loopLabel = cur_->newLabel();
    std::string exitLabel = cur_->newLabel();

    emit(IR_Label{loopLabel});

    // Condition
    std::string condReg = genExpr(s.cond);
    emit(IR_BranchNot{condReg, exitLabel});

    for (auto& stmt : s.body)
        genStmt(stmt);

    genAssign(*s.step);
    emit(IR_Jump{loopLabel});
    emit(IR_Label{exitLabel});
}

void IRGen::genIf(const IfStmt& s) {
    std::string condReg  = genExpr(s.cond);
    std::string exitLabel = cur_->newLabel();

    emit(IR_BranchNot{condReg, exitLabel});
    for (auto& stmt : s.thenBody)
        genStmt(stmt);
    emit(IR_Label{exitLabel});
}

void IRGen::genReturn(const ReturnStmt& s) {
    // Nothing to store for void kernels; value is unused
    emit(IR_Ret{});
}

// ─────────────────────────────────────────────────────────────────────────────
//  Expressions — return register name holding the result
// ─────────────────────────────────────────────────────────────────────────────

std::string IRGen::genExpr(const Expr& e, IRType hint) {
    return std::visit([&](auto& node) -> std::string {
        using T = std::decay_t<decltype(node)>;

        if constexpr (std::is_same_v<T, std::unique_ptr<IntLitExpr>>) {
            return std::to_string(node->value);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<FloatLitExpr>>) {
            return std::to_string(node->value);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<NameExpr>>) {
            auto it = env_.find(node->name);
            if (it == env_.end())
                throw CodegenError("Undefined variable: " + node->name);
            return it->second.reg;
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<ThreadIdxExpr>>) {
            std::string dst = cur_->newIntReg();
            emit(IR_ThreadIdx{dst, node->dim});
            return dst;
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<BlockIdxExpr>>) {
            std::string dst = cur_->newIntReg();
            emit(IR_BlockIdx{dst, node->dim});
            return dst;
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<BlockDimExpr>>) {
            std::string dst = cur_->newIntReg();
            emit(IR_BlockDim{dst, node->dim});
            return dst;
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<BinOpExpr>>) {
            return genBinOp(*node);
        }
        if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpr>>) {
            std::string baseReg = genExpr(node->array);
            std::string idxReg  = genExpr(node->index);
            // Look up type from environment
            IRType elemType = IRType::Int;
            if (auto* nm = std::get_if<std::unique_ptr<NameExpr>>(&node->array)) {
                auto it = env_.find((*nm)->name);
                if (it != env_.end()) elemType = it->second.type;
            }
            std::string dst = (elemType == IRType::Float)
                ? cur_->newFloatReg() : cur_->newIntReg();
            emit(IR_LoadIdx{dst, baseReg, idxReg, elemType});
            return dst;
        }
        return "0"; // unreachable
    }, e);
}

std::string IRGen::genBinOp(const BinOpExpr& e) {
    bool isCmp = isCmpOp(e.op);

    std::string lhs = genExpr(e.left);
    std::string rhs = genExpr(e.right);

    // Infer type from operands
    IRType t = IRType::Int;
    if (lhs.find('.') != std::string::npos || rhs.find('.') != std::string::npos)
        t = IRType::Float;
    if (!lhs.empty() && lhs[0] == '%' && lhs.size() > 1 && lhs[1] == 'f')
        t = IRType::Float;

    if (isCmp) {
        std::string pred = cur_->newIntReg(); // predicate register
        emit(IR_Cmp{pred, lhs, e.op, rhs, t});
        return pred;
    }

    std::string dst = (t == IRType::Float) ? cur_->newFloatReg() : cur_->newIntReg();
    emit(IR_BinOp{dst, lhs, e.op, rhs, t});
    return dst;
}

bool IRGen::isCmpOp(const std::string& op) const {
    return op == "<" || op == ">" || op == "<=" ||
           op == ">=" || op == "==" || op == "!=";
}
