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
    // Extra library search paths (`-L <dir>`, repeatable), forwarded as-is
    // to the backend compiler. A `Lib "name"` clause (M4) only names a
    // library, not a path to it - the path (e.g. a test's build directory)
    // has to come from outside the .bas source, exactly like g++'s own
    // -L/-l split.
    std::vector<std::string> libDirs;
    // M5c: extra libraries to link (`-l <name>`, repeatable), forwarded as
    // `-l<name>` after the auto-derived ones from the module's own `Lib
    // "name"` clauses. Needed for a *transitive* dependency: if this
    // module `#include`s only its direct dependency's interface file, its
    // own `Module::externLibs` has no idea a transitively-needed library
    // even exists (that library's `Lib` clause lives in a different,
    // never-`#include`d interface file) - ebpm passes it explicitly here
    // instead, exactly like it already must pass `-L` for the same reason.
    std::vector<std::string> extraLibNames;
    // M5: build a library (static archive + auto-generated interface file)
    // instead of an executable - see printUsage for the exact output shape.
    bool libMode = false;
    // M5: extra #include search paths (`-I <dir>`, repeatable) - a fallback
    // only, consulted after the includer-relative lookup fails (see
    // preprocess()'s doc comment). Lets a package's source #include a
    // dependency's auto-generated interface file without knowing its exact
    // relative filesystem path.
    std::vector<std::string> includeDirs;
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
        } else if (a == "-L") {
            if (i + 1 >= args.size()) {
                err = "-L requires an argument";
                return false;
            }
            opts.libDirs.push_back(args[++i]);
        } else if (a == "-I") {
            if (i + 1 >= args.size()) {
                err = "-I requires an argument";
                return false;
            }
            opts.includeDirs.push_back(args[++i]);
        } else if (a == "-l") {
            if (i + 1 >= args.size()) {
                err = "-l requires an argument";
                return false;
            }
            opts.extraLibNames.push_back(args[++i]);
        } else if (a == "--keep-cpp") {
            opts.keepCpp = true;
        } else if (a == "--lib") {
            opts.libMode = true;
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
    os << "usage: ebc <input.bas> [-o <output>] [-cxx <compiler>] [-L <dir>]... [-I <dir>]...\n";
    os << "           [-l <name>]... [--keep-cpp] [--lib]\n";
    os << "  --lib: build a library instead of an executable - <output> is a bare\n";
    os << "  name; produces lib<output>.a (a static archive) and <output>.iface.bas\n";
    os << "  (an auto-generated interface for dependent packages to #include).\n";
    os << "  -I <dir>: extra #include search path, consulted only after the\n";
    os << "  includer-relative lookup fails.\n";
    os << "  -l <name>: extra library to link, alongside any already named by the\n";
    os << "  module's own Lib \"name\" clauses.\n";
}

// M6: the runtime's PCH shadow directory (built by runtime/CMakeLists.txt,
// GCC only), added as an extra -I *before* the real runtime include dir so
// a plain `#include "ebasic/runtime/runtime.hpp"` automatically prefers the
// precompiled .gch sitting there - no other change to the compile
// invocation is needed (verified empirically: GCC's own automatic PCH
// lookup, and its own graceful fallback when the .gch doesn't match, both
// require no special flags at all). Empty when no system g++ was found to
// build one (CMake defines the macro either way) - appending nothing in
// that case, rather than passing a bogus `-I ""`, which could otherwise
// resolve to searching the current directory.
std::vector<std::string> runtimeIncludeArgs() {
    std::vector<std::string> args;
    if (std::string pchDir = EBASIC_RUNTIME_PCH_DIR; !pchDir.empty()) {
        args.push_back("-I");
        args.push_back(pchDir);
    }
    args.push_back("-I");
    args.push_back(EBASIC_RUNTIME_INCLUDE_DIR);
    return args;
}

// M5 (--lib mode): a library's object file must never define `main` itself
// (it would collide with the consuming package's own `main` at final link
// time), so its module may only contain declarations - no top-level
// executable statement (PRINT, assignment, IF, a loop, ...). Checked
// structurally here, directly against the parsed module, rather than
// threading a new mode flag through Sema.
bool hasOnlyLibDeclarations(const ebasic::Module& module, std::string& err) {
    for (const auto& stmtPtr : module.stmts) {
        switch (stmtPtr->kind) {
            case ebasic::StmtKind::Dim:
            case ebasic::StmtKind::Const:
            case ebasic::StmtKind::Enum:
            case ebasic::StmtKind::SubDecl:
            case ebasic::StmtKind::FunctionDecl:
            case ebasic::StmtKind::TypeDecl:
            case ebasic::StmtKind::UnionDecl:
            case ebasic::StmtKind::NamespaceDecl:
                continue;
            default:
                err = "line " + std::to_string(stmtPtr->loc.line) +
                      ": a --lib build may only contain declarations (DIM/CONST/ENUM/SUB/"
                      "FUNCTION/TYPE/UNION/NAMESPACE) at the top level, not executable code";
                return false;
        }
    }
    return true;
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
    diags.registerFile(opts.inputPath); // fileId 0

    ebasic::PreprocessResult preprocessed =
        ebasic::preprocess(rawSource, opts.inputPath, diags, opts.includeDirs);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    ebasic::Lexer lexer(preprocessed.source, preprocessed.lineMap, diags);
    auto tokens = lexer.tokenize();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    ebasic::Parser parser(std::move(tokens), diags);
    ebasic::Module module = parser.parseModule();
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    ebasic::Sema sema(diags);
    sema.check(module);
    if (diags.hasErrors()) {
        diags.printAll(std::cerr);
        return 1;
    }

    if (opts.libMode) {
        std::string err;
        if (!hasOnlyLibDeclarations(module, err)) {
            std::cerr << "ebc: error: " << err << "\n";
            return 1;
        }
    }

    ebasic::Codegen codegen;
    std::string cpp = codegen.generate(module, opts.libMode);

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

    int rc = 0;
    if (opts.libMode) {
        // M5: compile to an object file only (no main() to link into an
        // executable - genuinely absent, see Codegen::generate's libMode),
        // then archive it into a static lib alongside an auto-generated
        // interface .bas file, exactly mirroring the M4 fixture-library
        // pattern (transpile -> compile -> ar), just driven by ebc itself.
        fs::path outDir = fs::path(opts.outputPath).parent_path();
        std::string libName = fs::path(opts.outputPath).filename().string();
        fs::path objPath = opts.outputPath + ".o";
        fs::path archivePath = outDir / ("lib" + libName + ".a");
        fs::path ifacePath = outDir / (libName + ".iface.bas");

        std::vector<std::string> compileArgs = {cxx, "-std=c++17"};
        for (const std::string& a : runtimeIncludeArgs()) compileArgs.push_back(a);
        compileArgs.push_back("-c");
        compileArgs.push_back(cppPath.string());
        compileArgs.push_back("-o");
        compileArgs.push_back(objPath.string());
        rc = ebasic::runProcess(compileArgs);
        if (rc == 0) {
            rc = ebasic::runProcess({"ar", "rcs", archivePath.string(), objPath.string()});
        }
        if (rc == 0) {
            std::ofstream ifaceOut(ifacePath);
            ifaceOut << codegen.generateLibraryInterface(module, libName);
        }
        std::error_code ec;
        fs::remove(objPath, ec);
    } else {
        std::vector<std::string> compileArgs = {cxx, "-std=c++17"};
        for (const std::string& a : runtimeIncludeArgs()) compileArgs.push_back(a);
        compileArgs.push_back(cppPath.string());
        compileArgs.push_back("-o");
        compileArgs.push_back(opts.outputPath);
        for (const std::string& dir : opts.libDirs) {
            compileArgs.push_back("-L");
            compileArgs.push_back(dir);
        }
        // Library names from `Lib "name"` clauses (M4) - -l flags must come
        // after the object/source files on the command line for a
        // traditional (non-`--start-group`) linker to resolve symbols from
        // them correctly.
        for (const std::string& lib : codegen.externLibs()) {
            compileArgs.push_back("-l" + lib);
        }
        // M5c: explicit -l names (a transitive dependency's library, whose
        // own Lib clause never appears in *this* module at all - see
        // Options::extraLibNames's doc comment).
        for (const std::string& lib : opts.extraLibNames) {
            compileArgs.push_back("-l" + lib);
        }
        rc = ebasic::runProcess(compileArgs);
    }

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
