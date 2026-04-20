#include "../include/ptx_emitter.h"
#include <stdexcept>
#include <set>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string PTXEmitter::ptxType(IRType t, bool isMov) {
    // For arithmetic: .s32 (int), .f32 (float)
    // For mov: .s32 / .f32 same
    return (t == IRType::Float) ? ".f32" : ".s32";
}

std::string PTXEmitter::ptxCmpType(IRType t) {
    return (t == IRType::Float) ? ".f32" : ".s32";
}

std::string PTXEmitter::ptxArithOp(const std::string& op, IRType t) {
    bool fp = (t == IRType::Float);
    if (op == "+") return fp ? "add.f32"  : "add.s32";
    if (op == "-") return fp ? "sub.f32"  : "sub.s32";
    if (op == "*") return fp ? "mul.f32"  : "mul.lo.s32";
    if (op == "/") return fp ? "div.rn.f32":"div.s32";
    if (op == "%") return "rem.s32";
    if (op == "<<") return "shl.b32";
    if (op == ">>") return "shr.s32";
    if (op == "&")  return "and.b32";
    if (op == "|")  return "or.b32";
    if (op == "^")  return "xor.b32";
    throw std::runtime_error("Unknown op: " + op);
}

bool PTXEmitter::isReg(const std::string& s) {
    return !s.empty() && s[0] == '%';
}

// PTX comparison operator names
static std::string cmpOp(const std::string& op, IRType t) {
    bool fp = (t == IRType::Float);
    if (op == "<")  return fp ? "lt" : "lt";
    if (op == ">")  return fp ? "gt" : "gt";
    if (op == "<=") return fp ? "le" : "le";
    if (op == ">=") return fp ? "ge" : "ge";
    if (op == "==") return fp ? "eq" : "eq";
    if (op == "!=") return fp ? "ne" : "ne";
    throw std::runtime_error("Unknown cmp op: " + op);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────────────────────

std::string PTXEmitter::emit(const IRProgram& prog) {
    out_.str(""); out_.clear();

    out_ << ".version 7.0\n";
    out_ << ".target sm_75\n";
    out_ << ".address_size 64\n\n";

    for (auto& k : prog.kernels)
        emitKernel(k);

    return out_.str();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Kernel prologue
// ─────────────────────────────────────────────────────────────────────────────

void PTXEmitter::emitKernel(const IRKernel& k) {
    out_ << ".visible .entry " << k.name << "(\n";

    // Parameters
    for (size_t i = 0; i < k.params.size(); ++i) {
        auto& p = k.params[i];
        if (p.isPtr)
            out_ << "    .param .u64 " << k.name << "_param_" << i;
        else
            out_ << "    .param " << ptxType(p.type) << " " << k.name << "_param_" << i;
        if (i + 1 < k.params.size()) out_ << ",";
        out_ << "\n";
    }
    out_ << ")\n{\n";

    // Declare registers
    emitRegDecls(k);

    // Load parameters into registers
    loadParams(k);

    // Body
    for (auto& instr : k.instrs)
        emitInstr(instr, k);

    out_ << "}\n\n";
}

void PTXEmitter::emitRegDecls(const IRKernel& k) {
    if (k.intRegs > 0)
        out_ << "    .reg .s32 %r<" << k.intRegs << ">;\n";
    if (k.floatRegs > 0)
        out_ << "    .reg .f32 %f<" << k.floatRegs << ">;\n";

    // Pointer registers (64-bit) for array params
    std::set<std::string> seen;
    for (auto& p : k.params) {
        if (p.isPtr) {
            std::string rname = "%rd_" + p.name;
            if (!seen.count(rname)) {
                out_ << "    .reg .u64 " << rname << ";\n";
                seen.insert(rname);
            }
        }
    }

    // Predicate registers — count how many IR_Cmp we have
    int preds = 0;
    for (auto& instr : k.instrs)
        if (std::holds_alternative<IR_Cmp>(instr)) ++preds;
    if (preds > 0)
        out_ << "    .reg .pred %p<" << preds << ">;\n";

    out_ << "\n";
}

void PTXEmitter::loadParams(const IRKernel& k) {
    for (size_t i = 0; i < k.params.size(); ++i) {
        auto& p = k.params[i];
        std::string paramName = k.name + "_param_" + std::to_string(i);

        if (p.isPtr) {
            out_ << "    ld.param.u64 %rd_" << p.name
                 << ", [" << paramName << "];\n";
        } else {
            // Find the register allocated during IRGen
            // For non-ptr params they are allocated as first regs in order
            // We just emit a placeholder load and rely on IRGen register names
            std::string regPrefix = (p.type == IRType::Float) ? "%f" : "%r";
            // params are assigned %r0, %r1... or %f0, %f1... in order
            // We need to find the index — simplest: track a counter per type
            // (For correctness we store the reg name in IRParam — add it here)
            out_ << "    ld.param" << ptxType(p.type) << " "
                 << regPrefix << i   // simplification: works for pure-int or pure-float
                 << ", [" << paramName << "];\n";
        }
    }
    out_ << "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Per-instruction emission
// ─────────────────────────────────────────────────────────────────────────────

void PTXEmitter::emitInstr(const IRInstr& instr, const IRKernel& k) {
    std::visit([&](auto& i) {
        using T = std::decay_t<decltype(i)>;

        if constexpr (std::is_same_v<T, IR_Assign>) {
            // Determine type from register name
            bool fp = (!i.dst.empty() && i.dst.size() > 1 && i.dst[1] == 'f');
            std::string ty = fp ? ".f32" : ".s32";
            if (!isReg(i.src)) {
                // Move immediate
                out_ << "    mov" << ty << " " << i.dst << ", " << i.src << ";\n";
            } else {
                out_ << "    mov" << ty << " " << i.dst << ", " << i.src << ";\n";
            }
        }
        else if constexpr (std::is_same_v<T, IR_BinOp>) {
            out_ << "    " << ptxArithOp(i.op, i.type)
                 << " " << i.dst << ", " << i.lhs << ", " << i.rhs << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_Cmp>) {
            // setp instruction — produces a predicate register
            std::string ty  = ptxCmpType(i.type);
            std::string cop = cmpOp(i.op, i.type);
            out_ << "    setp." << cop << ty
                 << " " << i.dst << ", " << i.lhs << ", " << i.rhs << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_BranchIf>) {
            out_ << "    @" << i.cond << " bra " << i.target << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_BranchNot>) {
            out_ << "    @!" << i.cond << " bra " << i.target << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_Jump>) {
            out_ << "    bra " << i.target << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_Label>) {
            out_ << i.name << ":\n";
        }
        else if constexpr (std::is_same_v<T, IR_LoadIdx>) {
            // Address = base + idx * sizeof(type)
            std::string addrReg = "%rd_tmp_load";
            int elemSize = (i.type == IRType::Float) ? 4 : 4;
            // mul idx * elemSize
            out_ << "    mul.wide.s32 " << addrReg << ", " << i.idx
                 << ", " << elemSize << ";\n";
            out_ << "    add.u64 " << addrReg << ", " << i.base
                 << ", " << addrReg << ";\n";
            out_ << "    ld.global" << ptxType(i.type)
                 << " " << i.dst << ", [" << addrReg << "];\n";
        }
        else if constexpr (std::is_same_v<T, IR_StoreIdx>) {
            std::string addrReg = "%rd_tmp_store";
            int elemSize = (i.type == IRType::Float) ? 4 : 4;
            out_ << "    mul.wide.s32 " << addrReg << ", " << i.idx
                 << ", " << elemSize << ";\n";
            out_ << "    add.u64 " << addrReg << ", " << i.base
                 << ", " << addrReg << ";\n";
            out_ << "    st.global" << ptxType(i.type)
                 << " [" << addrReg << "], " << i.src << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_ThreadIdx>) {
            char d = i.dim;
            out_ << "    mov.u32 " << i.dst << ", %tid." << d << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_BlockIdx>) {
            char d = i.dim;
            out_ << "    mov.u32 " << i.dst << ", %ctaid." << d << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_BlockDim>) {
            char d = i.dim;
            out_ << "    mov.u32 " << i.dst << ", %ntid." << d << ";\n";
        }
        else if constexpr (std::is_same_v<T, IR_Ret>) {
            out_ << "    ret;\n";
        }
    }, instr);
}
