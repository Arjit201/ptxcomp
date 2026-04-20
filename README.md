# MiniCUDA → PTX Compiler

A from-scratch compiler in C++17 that accepts a tiny C-like GPU kernel language and emits real PTX assembly targeting NVIDIA sm_75 (Turing architecture).

Built as a resume project for NVIDIA's PTX compiler team — covering the exact pipeline PTX compiler engineers work on: **lexing → parsing → IR lowering → PTX instruction emission**.

---

## Pipeline

```
MiniCUDA source code
        │
        ▼
   ┌─────────┐
   │  Lexer  │   Character stream → Token stream
   └─────────┘   Recognizes: keywords, identifiers, literals,
        │        operators, threadIdx/blockIdx/blockDim
        ▼
   ┌─────────┐
   │  Parser │   Token stream → AST
   └─────────┘   Recursive descent. Produces typed node tree:
        │        KernelDef, ForStmt, BinOpExpr, IndexExpr, ...
        ▼
   ┌─────────┐
   │  IRGen  │   AST → Three-Address IR
   └─────────┘   Flat register-based IR (like LLVM IR, simplified).
        │        Allocates virtual registers: %r0, %f0, ...
        │        Emits: IR_BinOp, IR_LoadIdx, IR_Cmp, IR_Label, ...
        ▼
   ┌─────────────┐
   │ PTX Emitter │   IR → PTX assembly text
   └─────────────┘   Produces valid .ptx files per NVIDIA PTX ISA 7.0
        │            add.s32, ld.global.f32, setp.lt.s32, bra, ret, ...
        ▼
      .ptx file  (can be validated with ptxas or run via CUDA Driver API)
```

---

## Language Features

```c
// Arrays (passed as 64-bit device pointers)
kernel vadd(int A[], int B[], int C[], int n) {
    // Built-in thread/block variables
    int tid = blockIdx.x * blockDim.x + threadIdx.x;

    // Bounds check
    if (tid < n) {
        // Array reads and writes
        C[tid] = A[tid] + B[tid];
    }
}
```

Supported constructs:
- **Types**: `int`, `float`
- **Parameters**: scalars and arrays (pointer params)
- **Built-ins**: `threadIdx.x/y/z`, `blockIdx.x/y/z`, `blockDim.x/y/z`
- **Statements**: variable declaration, assignment, `if`, `for`, `return`
- **Expressions**: `+`, `-`, `*`, `/`, `%`, `<`, `>`, `<=`, `>=`, `==`, `!=`, array indexing
- **Comments**: `// line comments`

---

## Sample Output

Input:
```c
kernel saxpy(float alpha, float X[], float Y[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        Y[tid] = alpha * X[tid] + Y[tid];
    }
}
```

Output (`saxpy.ptx`):
```ptx
.version 7.0
.target sm_75
.address_size 64

.visible .entry saxpy(
    .param .f32 saxpy_param_0,
    .param .u64 saxpy_param_1,
    .param .u64 saxpy_param_2,
    .param .s32 saxpy_param_3
)
{
    .reg .s32 %r<8>;
    .reg .f32 %f<5>;
    .reg .u64 %rd_X;
    .reg .u64 %rd_Y;
    .reg .pred %p<1>;

    ld.param.f32  %f0, [saxpy_param_0];
    ld.param.u64  %rd_X, [saxpy_param_1];
    ld.param.u64  %rd_Y, [saxpy_param_2];
    ld.param.s32  %r3, [saxpy_param_3];

    mov.u32 %r2, %ctaid.x;
    mov.u32 %r3, %ntid.x;
    mul.lo.s32 %r4, %r2, %r3;
    mov.u32 %r5, %tid.x;
    add.s32 %r6, %r4, %r5;
    mov.s32 %r1, %r6;
    setp.lt.s32 %r7, %r1, %r0;
    @!%r7 bra .L0;
    mul.wide.s32 %rd_tmp_load, %r1, 4;
    add.u64 %rd_tmp_load, %rd_X, %rd_tmp_load;
    ld.global.f32 %f1, [%rd_tmp_load];
    mul.f32 %f2, %f0, %f1;
    mul.wide.s32 %rd_tmp_load, %r1, 4;
    add.u64 %rd_tmp_load, %rd_Y, %rd_tmp_load;
    ld.global.f32 %f3, [%rd_tmp_load];
    add.f32 %f4, %f2, %f3;
    mul.wide.s32 %rd_tmp_store, %r1, 4;
    add.u64 %rd_tmp_store, %rd_Y, %rd_tmp_store;
    st.global.f32 [%rd_tmp_store], %f4;
.L0:
    ret;
}
```

---

## Build

```bash
# Requires g++ with C++17 support
g++ -std=c++17 -O2 -Iinclude \
    src/lexer.cpp src/parser.cpp src/irgen.cpp \
    src/ptx_emitter.cpp src/main.cpp \
    -o ptxc

# Run built-in examples
./ptxc

# Compile your own .mcuda file
./ptxc my_kernel.mcuda
```

Or with CMake:
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

---

## Validate Output with ptxas

If you have the CUDA Toolkit installed:
```bash
ptxas --gpu-name sm_75 output/vadd.ptx -o vadd.cubin
ptxas --gpu-name sm_75 output/saxpy.ptx -o saxpy.cubin
```

---

## Project Structure

```
ptx_compiler/
├── include/
│   ├── lexer.h          # Token types, Lexer class
│   ├── ast.h            # AST node definitions (std::variant tree)
│   ├── parser.h         # Recursive-descent parser
│   ├── ir.h             # Three-Address IR instruction set
│   ├── irgen.h          # AST → IR code generator
│   └── ptx_emitter.h    # IR → PTX text emitter
├── src/
│   ├── lexer.cpp
│   ├── parser.cpp
│   ├── irgen.cpp
│   ├── ptx_emitter.cpp
│   └── main.cpp         # Driver + example kernels
├── output/              # Generated .ptx files
├── CMakeLists.txt
└── README.md
```

---

## Possible Extensions (for interviews)

- **SSA form** — Convert IR to Static Single Assignment for optimisation passes
- **Dead code elimination** — Remove unused register assignments
- **Constant folding** — Evaluate `2 * 4` at compile time → `8`
- **Register allocation** — Map unlimited virtual registers to a limited physical set
- **`else` branches** — Currently `if` with no else; add full if/else
- **Type checking pass** — Catch type mismatches before codegen
- **CUDA Driver API runner** — Load the `.ptx` at runtime and execute it on a real GPU

---

## Key Concepts for NVIDIA Interviews

| Topic | Where it appears |
|---|---|
| PTX ISA | ptx_emitter.cpp — every instruction |
| Three-Address Code / IR | ir.h, irgen.cpp |
| Recursive descent parsing | parser.cpp |
| `std::variant` / visitor pattern | ast.h, irgen.cpp — all AST traversal |
| Virtual register allocation | IRKernel::newIntReg(), newFloatReg() |
| Predicate registers | IR_Cmp → `setp` + `@!pred bra` |
| Global memory addressing | IR_LoadIdx/StoreIdx → `ld.global`, `st.global` |
| Thread indexing | IR_ThreadIdx → `mov.u32 %r, %tid.x` |
