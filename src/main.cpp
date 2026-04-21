#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/constfold.h"
#include "../include/irgen.h"
#include "../include/ptx_emitter.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

const char* EXAMPLE_VADD = R"(
kernel vadd(int A[], int B[], int C[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        C[tid] = A[tid] + B[tid];
    }
}
)";

const char* EXAMPLE_SAXPY = R"(
kernel saxpy(float alpha, float X[], float Y[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        Y[tid] = alpha * X[tid] + Y[tid];
    }
}
)";

const char* EXAMPLE_CLAMP = R"(
kernel clamp(int arr[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        if (arr[tid] < 0) {
            arr[tid] = 0;
        } else {
            if (arr[tid] > 255) {
                arr[tid] = 255;
            }
        }
    }
}
)";

const char* EXAMPLE_CONSTFOLD = R"(
kernel constdemo(int arr[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    int stride = 4 * 4;
    int offset = 10 - 2;
    int scale  = n * 1;
    int zero   = n * 0;
    if (tid < n) {
        arr[tid] = arr[tid] + stride + offset;
    }
}
)";

struct CompileResult {
    std::string ptx;
    bool ok = false;
    std::string error;
};

CompileResult compile(const std::string& source, bool verbose = false) {
    CompileResult result;
    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (verbose)
            std::cout << "[1/4] Lexer      : " << tokens.size() - 1 << " tokens\n";

        Parser parser(std::move(tokens));
        Program ast = parser.parse();
        if (verbose) {
            std::cout << "[2/4] Parser     : " << ast.kernels.size() << " kernel(s)";
            for (auto& k : ast.kernels)
                std::cout << " [" << k.name << " / " << k.params.size() << " params]";
            std::cout << "\n";
        }

        ConstantFolder folder;
        ast = folder.fold(std::move(ast));
        if (verbose)
            std::cout << "[3/4] ConstFold  : literals evaluated at compile time\n";

        IRGen irgen;
        IRProgram ir = irgen.generate(ast);
        if (verbose)
            for (auto& k : ir.kernels)
                std::cout << "[4/4] IRGen      : kernel '" << k.name
                          << "' -> " << k.instrs.size() << " IR instructions\n";

        PTXEmitter emitter;
        result.ptx = emitter.emit(ir);
        result.ok = true;

    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

int main(int argc, char* argv[]) {
    std::cout << "================================================\n";
    std::cout << "  MiniCUDA -> PTX Compiler  (sm_75 / PTX ISA 7)\n";
    std::cout << "  Passes: Lex -> Parse -> ConstFold -> IR -> PTX\n";
    std::cout << "================================================\n\n";

    struct Example { const char* name; const char* src; const char* outfile; const char* note; };
    Example examples[] = {
        {"vadd  (vector addition)",    EXAMPLE_VADD,       "output/vadd.ptx",      "baseline kernel"},
        {"saxpy (float SAXPY)",         EXAMPLE_SAXPY,      "output/saxpy.ptx",     "float regs + pointer params"},
        {"clamp (if/else branches)",    EXAMPLE_CLAMP,      "output/clamp.ptx",     "NEW: if/else -> setp + bra + jump"},
        {"constdemo (constant folding)",EXAMPLE_CONSTFOLD,  "output/constfold.ptx", "NEW: 4*4->16, 10-2->8, n*1->n, n*0->0"},
    };

    bool anyFailed = false;
    for (auto& ex : examples) {
        std::cout << "---- " << ex.name << "\n";
        std::cout << "     (" << ex.note << ")\n";
        std::cout << "Source:" << ex.src << "\n";

        auto result = compile(ex.src, true);
        if (!result.ok) {
            std::cerr << "ERROR: " << result.error << "\n\n";
            anyFailed = true;
            continue;
        }

        std::cout << "\nGenerated PTX:\n" << result.ptx;

        std::ofstream f(ex.outfile);
        if (f) { f << result.ptx; std::cout << "[Output] Written to " << ex.outfile << "\n"; }
        std::cout << "\n";
    }

    if (argc == 2) {
        std::ifstream f(argv[1]);
        if (!f) { std::cerr << "Cannot open " << argv[1] << "\n"; return 1; }
        std::ostringstream ss; ss << f.rdbuf();
        std::cout << "---- Compiling: " << argv[1] << " ----\n";
        auto result = compile(ss.str(), true);
        if (!result.ok) { std::cerr << "ERROR: " << result.error << "\n"; return 1; }
        std::cout << result.ptx;
        std::string out = std::string(argv[1]) + ".ptx";
        std::ofstream o(out); o << result.ptx;
        std::cout << "[Output] Written to " << out << "\n";
    }

    return anyFailed ? 1 : 0;
}
