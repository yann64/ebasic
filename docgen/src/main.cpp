#include "render.hpp"

#include "diagnostics/diagnostics.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "preprocessor/preprocessor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

void printUsage(std::ostream& os) {
    os << "usage: docgen <input.bas> -o <output-dir>\n";
    os << "  produces <output-dir>/index.md and <output-dir>/index.html\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4 || std::string(argv[2]) != "-o") {
        printUsage(std::cerr);
        return 1;
    }
    std::string inputPath = argv[1];
    std::string outputDir = argv[3];

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
