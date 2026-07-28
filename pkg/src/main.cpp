#include "manifest.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void printUsage(std::ostream& os) {
    os << "usage: ebpm <command> [args]\n";
    os << "commands:\n";
    os << "  new <name> [--lib]   scaffold a new package directory <name>/\n";
    os << "  init [--lib]         scaffold a package in the current directory\n";
}

// Bare package-name text for a manifest's [package] name = "..." line - not
// a general TOML string escaper (a package name is always a plain
// identifier-ish string in practice), just enough to avoid producing
// malformed TOML from a name containing a literal quote.
std::string escapeTomlString(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '"' || c == '\\') r.push_back('\\');
        r.push_back(c);
    }
    return r;
}

std::string manifestText(const std::string& name, bool isLib) {
    std::ostringstream out;
    out << "[package]\n";
    out << "name = \"" << escapeTomlString(name) << "\"\n";
    out << "version = \"0.1.0\"\n\n";
    if (isLib) {
        out << "[lib]\n";
        out << "path = \"src/lib.bas\"\n";
    } else {
        out << "[bin]\n";
        out << "name = \"" << escapeTomlString(name) << "\"\n";
        out << "path = \"src/main.bas\"\n";
    }
    return out.str();
}

std::string stubBinSource(const std::string& name) {
    return "PRINT \"Hello from " + name + "!\"\n";
}

std::string stubLibSource() {
    return "FUNCTION Placeholder() AS INTEGER\n    Placeholder = 0\nEND FUNCTION\n";
}

// Writes a fresh package (manifest + one stub source file) into `dir`,
// which must already exist and be empty of any prior `ebasic.toml`. Shared
// by `new` (dir is a just-created subdirectory) and `init` (dir is `.`).
bool scaffoldInto(const fs::path& dir, const std::string& name, bool isLib, std::string& err) {
    fs::path manifestPath = dir / "ebasic.toml";
    if (fs::exists(manifestPath)) {
        err = manifestPath.string() + " already exists";
        return false;
    }
    std::error_code ec;
    fs::create_directories(dir / "src", ec);
    if (ec) {
        err = "could not create '" + (dir / "src").string() + "': " + ec.message();
        return false;
    }

    {
        std::ofstream out(manifestPath);
        out << manifestText(name, isLib);
    }
    fs::path srcPath = dir / (isLib ? "src/lib.bas" : "src/main.bas");
    if (!fs::exists(srcPath)) {
        std::ofstream out(srcPath);
        out << (isLib ? stubLibSource() : stubBinSource(name));
    }
    return true;
}

int cmdNew(const std::vector<std::string>& args) {
    bool isLib = false;
    std::string name;
    for (const std::string& a : args) {
        if (a == "--lib") isLib = true;
        else if (!name.empty()) {
            std::cerr << "ebpm: error: multiple package names given\n";
            return 1;
        } else {
            name = a;
        }
    }
    if (name.empty()) {
        std::cerr << "ebpm: error: 'new' requires a package name\n";
        return 1;
    }
    fs::path dir = name;
    if (fs::exists(dir)) {
        std::cerr << "ebpm: error: '" << dir.string() << "' already exists\n";
        return 1;
    }
    std::string err;
    if (!scaffoldInto(dir, name, isLib, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::cout << "Created " << (isLib ? "library" : "binary") << " package '" << name << "' at "
              << dir.string() << "\n";
    return 0;
}

int cmdInit(const std::vector<std::string>& args) {
    bool isLib = false;
    for (const std::string& a : args) {
        if (a == "--lib") isLib = true;
        else {
            std::cerr << "ebpm: error: unknown argument to 'init': " << a << "\n";
            return 1;
        }
    }
    fs::path dir = fs::current_path();
    std::string name = dir.filename().string();
    std::string err;
    if (!scaffoldInto(dir, name, isLib, err)) {
        std::cerr << "ebpm: error: " << err << "\n";
        return 1;
    }
    std::cout << "Created " << (isLib ? "library" : "binary") << " package '" << name
              << "' in the current directory\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) {
        printUsage(std::cerr);
        return 1;
    }
    std::string command = args[0];
    std::vector<std::string> rest(args.begin() + 1, args.end());

    if (command == "new") return cmdNew(rest);
    if (command == "init") return cmdInit(rest);

    std::cerr << "ebpm: error: unknown command '" << command << "'\n";
    printUsage(std::cerr);
    return 1;
}
