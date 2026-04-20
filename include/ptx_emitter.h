#pragma once
#include "ir.h"
#include <string>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
//  PTX Emitter
//
//  Walks the IRProgram and produces a valid .ptx text file.
//
//  PTX version target: sm_75 (Turing), PTX ISA 7.0
//  (easy to change via the constants below)
// ─────────────────────────────────────────────────────────────────────────────

class PTXEmitter {
public:
    std::string emit(const IRProgram& prog);

private:
    std::ostringstream out_;

    void emitKernel(const IRKernel& k);
    void emitInstr(const IRInstr& instr, const IRKernel& k);

    // helpers
    void emitParamDecls(const IRKernel& k);
    void emitRegDecls(const IRKernel& k);
    void loadParams(const IRKernel& k);

    // PTX type strings
    static std::string ptxType(IRType t, bool isMov = false);
    static std::string ptxCmpType(IRType t);
    static std::string ptxArithOp(const std::string& op, IRType t);
    static bool isReg(const std::string& s);
};
