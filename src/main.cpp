#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/irgen.h"
#include "../include/ptx_emitter.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
//  Example MiniCUDA programs
// ─────────────────────────────────────────────────────────────────────────────

const char* EXAMPLE_VADD = R"(
// Vector addition:  C[i] = A[i] + B[i]
kernel vadd(int A[], int B[], int C[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        C[tid] = A[tid] + B[tid];
    }
}
)";

const char* EXAMPLE_SAXPY = R"(
// SAXPY:  Y[i] = alpha * X[i] + Y[i]
kernel saxpy(float alpha, float X[], float Y[], int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        Y[tid] = alpha * X[tid] + Y[tid];
    }
}
)";

const char* EXAMPLE_SCALE = R"(
// Scale every element of an array by a constant
kernel scale(int arr[], int factor, int n) {
    int tid = blockIdx.x * blockDim.x + threadIdx.x;
    if (tid < n) {
        arr[tid] = arr[tid] * factor;
    }
}
)";

// ─────────────────────────────────────────────────────────────────────────────
//  Pipeline
// ─────────────────────────────────────────────────────────────────────────────

struct CompileResult {
    std::string ptx;
    bool ok = false;
    std::string error;
};

CompileResult compile(const std::string& source, bool verbose = false) {
    CompileResult result;
    try {
        // Stage 1: Lex
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        if (verbose) {
            std::cout << "[Lexer] " << tokens.size() - 1 << " tokens\n";
        }

        // Stage 2: Parse
        Parser parser(std::move(tokens));
        Program ast = parser.parse();
        if (verbose) {
            std::cout << "[Parser] " << ast.kernels.size() << " kernel(s)\n";
            for (auto& k : ast.kernels)
                std::cout << "         kernel '" << k.name
                          << "' with " << k.params.size() << " params\n";
        }

        // Stage 3: IR lowering
        IRGen irgen;
        IRProgram ir = irgen.generate(ast);
        if (verbose) {
            for (auto& k : ir.kernels)
                std::cout << "[IRGen]  kernel '" << k.name
                          << "' → " << k.instrs.size() << " IR instructions\n";
        }

        // Stage 4: PTX emission
        PTXEmitter emitter;
        result.ptx = emitter.emit(ir);
        result.ok = true;

    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    std::cout << "╔══════════════════════════════════════════════╗\n";
    std::cout << "║   MiniCUDA → PTX Compiler  (sm_75 / PTX 7)  ║\n";
    std::cout << "╚══════════════════════════════════════════════╝\n\n";

    struct Example { const char* name; const char* src; const char* outfile; };
    Example examples[] = {
        {"vadd  (vector addition)",  EXAMPLE_VADD,  "output/vadd.ptx"},
        {"saxpy (float SAXPY)",       EXAMPLE_SAXPY, "output/saxpy.ptx"},
        {"scale (scalar multiply)",   EXAMPLE_SCALE, "output/scale.ptx"},
    };

    bool anyFailed = false;
    for (auto& ex : examples) {
        std::cout << "──── Compiling: " << ex.name << " ────\n";
        std::cout << "Source:\n" << ex.src << "\n";

        auto result = compile(ex.src, /*verbose=*/true);
        if (!result.ok) {
            std::cerr << "ERROR: " << result.error << "\n\n";
            anyFailed = true;
            continue;
        }

        std::cout << "\nGenerated PTX:\n";
        std::cout << result.ptx;

        // Write to file
        std::ofstream f(ex.outfile);
        if (f) {
            f << result.ptx;
            std::cout << "[Output] Written to " << ex.outfile << "\n";
        }
        std::cout << "\n";
    }

    // If user provided a file argument, compile that too
    if (argc == 2) {
        std::ifstream f(argv[1]);
        if (!f) {
            std::cerr << "Cannot open " << argv[1] << "\n";
            return 1;
        }
        std::ostringstream ss; ss << f.rdbuf();
        std::cout << "──── Compiling user file: " << argv[1] << " ────\n";
        auto result = compile(ss.str(), true);
        if (!result.ok) {
            std::cerr << "ERROR: " << result.error << "\n";
            return 1;
        }
        std::cout << result.ptx;

        std::string outpath = std::string(argv[1]) + ".ptx";
        std::ofstream out(outpath);
        out << result.ptx;
        std::cout << "[Output] Written to " << outpath << "\n";
    }

    return anyFailed ? 1 : 0;
}
