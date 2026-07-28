#include "render.hpp"

#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"

#include "ebasic/version.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// Parsed command-line options - see parseArgs.
struct Options {
    std::string inputPath;
    std::string outputDir;
    /// -v/--version and -h/--help short-circuit everything else in main() -
    /// parseArgs still returns true with these set even if inputPath/
    /// outputDir were never given, matching ebc's own convention.
    bool showVersion = false;
    bool showHelp = false; ///< see showVersion
};

void printUsage(std::ostream& os) {
    os << "usage: docgen <input.bas> -o <output-dir>\n";
    os << "       docgen [-v | --version] [-h | --help]\n";
    os << "  produces <output-dir>/index.md and <output-dir>/index.html\n";
}

/// Fills `opts` from argv, or returns false with a message in `err`.
/// Recognizes -o <dir>, -v/--version, -h/--help, and one positional input
/// path - anything else (an unknown flag, more than one positional
/// argument, or a missing required flag/positional when neither -v nor -h
/// was given) is an error.
bool parseArgs(int argc, char** argv, Options& opts, std::string& err) {
    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "-o") {
            if (i + 1 >= args.size()) {
                err = "-o requires an argument";
                return false;
            }
            opts.outputDir = args[++i];
        } else if (a == "-v" || a == "--version") {
            opts.showVersion = true;
        } else if (a == "-h" || a == "--help") {
            opts.showHelp = true;
        } else if (!a.empty() && a[0] == '-') {
            err = "unknown option: " + a;
            return false;
        } else {
            if (!opts.inputPath.empty()) {
                err = "multiple input files are not supported";
                return false;
            }
            opts.inputPath = a;
        }
    }
    if (!opts.showVersion && !opts.showHelp && (opts.inputPath.empty() || opts.outputDir.empty())) {
        err = "expected <input.bas> -o <output-dir>";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Options opts;
    std::string err;
    if (!parseArgs(argc, argv, opts, err)) {
        std::cerr << "docgen: error: " << err << "\n";
        printUsage(std::cerr);
        return 1;
    }
    if (opts.showVersion) {
        std::cout << "docgen " << ebasic::versionString() << "\n";
        return 0;
    }
    if (opts.showHelp) {
        printUsage(std::cout);
        return 0;
    }

    std::string inputPath = opts.inputPath;
    std::string outputDir = opts.outputDir;

    std::ifstream in(inputPath);
    if (!in) {
        std::cerr << "docgen: error: cannot open input file: " << inputPath << "\n";
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string rawSource = buf.str();

    ebasic::DiagnosticEngine diags;
    diags.registerFile(inputPath);

    ebasic::PreprocessResult preprocessed = ebasic::preprocess(rawSource, inputPath, diags);
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

    std::error_code ec;
    fs::create_directories(outputDir, ec);
    if (ec) {
        std::cerr << "docgen: error: could not create '" << outputDir << "': " << ec.message() << "\n";
        return 1;
    }

    std::string title = fs::path(inputPath).stem().string();

    {
        std::ofstream md(fs::path(outputDir) / "index.md");
        md << docgen::renderMarkdown(module, title);
    }
    {
        std::ofstream html(fs::path(outputDir) / "index.html");
        html << docgen::renderHtml(module, title);
    }

    return 0;
}
