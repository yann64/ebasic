#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ebasic {

// fileId indexes into DiagnosticEngine's file registry (registerFile), so a
// diagnostic anywhere - including inside a #include'd file - can report its
// true originating file, not just the top-level input file's name.
struct SourceLoc {
    int line = 0;
    int column = 0;
    int fileId = 0;
};

enum class Severity {
    Error,
    Warning,
};

struct Diagnostic {
    Severity severity;
    SourceLoc loc;
    std::string message;
};

class DiagnosticEngine {
public:
    void error(SourceLoc loc, std::string message);
    void warning(SourceLoc loc, std::string message);

    bool hasErrors() const { return errorCount_ > 0; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    // Registers a file path, returning a stable id for use in SourceLoc.
    // Registering the same path again returns the same id.
    int registerFile(const std::string& path);

    void printAll(std::ostream& os) const;

private:
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::string> fileNames_;
    std::unordered_map<std::string, int> fileIds_;
    int errorCount_ = 0;
};

} // namespace ebasic
