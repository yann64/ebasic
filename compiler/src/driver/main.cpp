#include "codegen/codegen.hpp"
#include "diagnostics/diagnostics.hpp"
#include "driver/process.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"
#include "sema/sema.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::string cxx;
    bool keepCpp = false;
};

bool parseArgs(int argc, char** argv, Options& opts, std::string& err) {
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-o") {
            if (i + 1 >= args.size()) {
                err = "-o requires an argument";
                return false;
            }
            opts.outputPath = args[++i];
        } else if (a == "-cxx") {
            if (i + 1 >= args.size()) {
                err = "-cxx requires an argument";
                return false;
            }
            opts.cxx = args[++i];
        } else if (a == "--keep-cpp") {
            opts.keepCpp = true;
        } else if (!a.empty() && a[0] == '-') {
            err = "unknown option: " + a;
            return false;
        } else {
            if (!opts.inputPath.empty()) {
                err = "multiple input files are not supported yet";
                return false;
            }
            opts.inputPath = a;
        }
    }
    if (opts.inputPath.empty()) {
        err = "no input file";
        return false;
    }
    return true;
}

void printUsage(std::ostream& os) {
    os << "usage: ebc <input.bas> [-o <output>] [-cxx <compiler>] [--keep-cpp]\n";
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    std::string err;
    if (!parseArgs(argc, argv, opts, err)) {
        std::cerr << "ebc: error: " << err << "\n";
        printUsage(std::cerr);
        return 1;
    }

    std::ifstream in(opts.inputPath);
    if (!in) {
        std::cerr << "ebc: error: cannot open input file: " << opts.inputPath << "\n";
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string rawSource = buf.str();

    ebasic::DiagnosticEngine diags;

    std::string source = ebasic::preprocess(rawSource, diags);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr, opts.inputPath);
        return 1;
    }

    ebasic::Lexer lexer(source, diags);
    auto tokens = lexer.tokenize();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr, opts.inputPath);
        return 1;
    }

    ebasic::Parser parser(std::move(tokens), diags);
    ebasic::Module module = parser.parseModule();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr, opts.inputPath);
        return 1;
    }

    ebasic::Sema sema(diags);
    sema.check(module);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr, opts.inputPath);
        return 1;
    }

    ebasic::Codegen codegen;
    std::string cpp = codegen.generate(module);

    if (opts.outputPath.empty()) {
        opts.outputPath = fs::path(opts.inputPath).stem().string();
    }

    fs::path cppPath = opts.outputPath + ".gen.cpp";
    {
        std::ofstream out(cppPath);
        out << cpp;
    }

    std::string cxx = opts.cxx;
    if (cxx.empty()) {
        const char* envCxx = std::getenv("CXX");
        cxx = envCxx ? envCxx : "g++";
    }

    std::vector<std::string> compileArgs = {
        cxx, "-std=c++17", "-I", EBASIC_RUNTIME_INCLUDE_DIR, cppPath.string(), "-o", opts.outputPath,
    };

    int rc = ebasic::runProcess(compileArgs);

    if (!opts.keepCpp) {
        std::error_code ec;
        fs::remove(cppPath, ec);
    }

    if (rc != 0) {
        std::cerr << "ebc: error: backend compilation failed (exit code " << rc << ")\n";
        return 1;
    }

    return 0;
}
