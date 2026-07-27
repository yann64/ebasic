#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace ebasic {

struct SourceLoc {
    int line = 0;
    int column = 0;
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

    void printAll(std::ostream& os, const std::string& filename) const;

private:
    std::vector<Diagnostic> diagnostics_;
    int errorCount_ = 0;
};

} // namespace ebasic
