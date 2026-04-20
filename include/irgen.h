#pragma once
#include "ast.h"
#include "ir.h"
#include <unordered_map>
#include <stdexcept>

class CodegenError : public std::runtime_error {
public:
    explicit CodegenError(const std::string& m) : std::runtime_error(m) {}
};

// Maps source variable names → IR register names + their types
struct VarInfo {
    std::string reg;
    IRType      type;
    bool        isPtr = false;
};

class IRGen {
public:
    IRProgram generate(const Program& prog);

private:
    IRKernel*                               cur_ = nullptr;
    std::unordered_map<std::string,VarInfo> env_;

    void genKernel(const KernelDef& kd, IRProgram& prog);
    void genStmt(const Stmt& s);
    void genVarDecl(const VarDeclStmt& s);
    void genAssign(const AssignStmt& s);
    void genFor(const ForStmt& s);
    void genIf(const IfStmt& s);
    void genReturn(const ReturnStmt& s);

    // Returns the register that holds the result
    std::string genExpr(const Expr& e, IRType hint = IRType::Int);
    std::string genBinOp(const BinOpExpr& e);

    IRType typeOf(const std::string& reg);

    bool isCmpOp(const std::string& op) const;
    void emit(IRInstr i) { cur_->instrs.push_back(std::move(i)); }
};
