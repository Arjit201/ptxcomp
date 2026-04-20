#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>

// ─────────────────────────────────────────────────────────────────────────────
//  Three-Address Code IR
//
//  Each instruction is a simple struct.  The code generator walks the AST and
//  emits a flat list of IRInstr per kernel.  The PTX emitter then walks the IR.
//
//  Registers are named:  %r0, %r1, ... (int)
//                        %f0, %f1, ... (float)
//  Labels are named:     .L0, .L1, ...
// ─────────────────────────────────────────────────────────────────────────────

enum class IRType { Int, Float };

struct IRParam {
    std::string name;   // original param name
    IRType      type;
    bool        isPtr;  // true for arrays (passed as .param .u64)
};

// ── Instruction variants ──────────────────────────────────────────────────────

struct IR_Assign    { std::string dst, src; };                   // dst = src
struct IR_BinOp     { std::string dst, lhs, op, rhs; IRType type; }; // dst = lhs op rhs
struct IR_LoadIdx   { std::string dst, base, idx; IRType type; }; // dst = base[idx]
struct IR_StoreIdx  { std::string base, idx, src; IRType type; }; // base[idx] = src
struct IR_Label     { std::string name; };                       // label:
struct IR_Jump      { std::string target; };                     // goto target
struct IR_BranchIf  { std::string cond, target; };              // if cond goto target
struct IR_BranchNot { std::string cond, target; };              // if !cond goto target
struct IR_Cmp       { std::string dst, lhs, op, rhs; IRType type; }; // dst = lhs cmp rhs
struct IR_ThreadIdx { std::string dst; char dim; };              // dst = threadIdx.x
struct IR_BlockIdx  { std::string dst; char dim; };
struct IR_BlockDim  { std::string dst; char dim; };
struct IR_Ret       {};

using IRInstr = std::variant<
    IR_Assign, IR_BinOp, IR_LoadIdx, IR_StoreIdx,
    IR_Label, IR_Jump, IR_BranchIf, IR_BranchNot,
    IR_Cmp, IR_ThreadIdx, IR_BlockIdx, IR_BlockDim,
    IR_Ret
>;

struct IRKernel {
    std::string          name;
    std::vector<IRParam> params;
    std::vector<IRInstr> instrs;

    // register / label counters (used during codegen)
    int intRegs   = 0;
    int floatRegs = 0;
    int labels    = 0;

    std::string newIntReg()   { return "%r" + std::to_string(intRegs++); }
    std::string newFloatReg() { return "%f" + std::to_string(floatRegs++); }
    std::string newLabel()    { return ".L" + std::to_string(labels++); }
};

struct IRProgram {
    std::vector<IRKernel> kernels;
};
