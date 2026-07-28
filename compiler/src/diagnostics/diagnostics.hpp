#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace ebasic {

/// fileId indexes into DiagnosticEngine's file registry (registerFile), so a
/// diagnostic anywhere - including inside a #include'd file - can report its
/// true originating file, not just the top-level input file's name.
struct SourceLoc {
    int line = 0;
    int column = 0;
    int fileId = 0;
};

/// Warning never stops compilation (checked via hasErrors(), not a separate
/// warning count) - the pipeline only has these two levels.
enum class Severity {
    Error,
    Warning,
};

/// One reported problem, ready to be formatted by printAll().
struct Diagnostic {
    Severity severity;
    SourceLoc loc;
    std::string message;
};

/// Accumulates diagnostics across an entire compile (lexer through codegen),
/// rather than each stage reporting independently - lets every driver stage
/// keep running after an error (so a single `ebc` invocation can report
/// multiple real problems at once) while still gating on hasErrors() before
/// moving to the next stage.
class DiagnosticEngine {
public:
    void error(SourceLoc loc, std::string message);
    void warning(SourceLoc loc, std::string message);

    /// True once at least one error() has been recorded - callers check this
    /// after each pipeline stage to decide whether to continue.
    bool hasErrors() const { return errorCount_ > 0; }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }

    /// Registers a file path, returning a stable id for use in SourceLoc.
    /// Registering the same path again returns the same id.
    int registerFile(const std::string& path);

    /// Writes every recorded diagnostic to `os`, one per line, in the order
    /// they were reported.
    void printAll(std::ostream& os) const;

private:
    std::vector<Diagnostic> diagnostics_;
    std::vector<std::string> fileNames_;
    std::unordered_map<std::string, int> fileIds_;
    int errorCount_ = 0;
};

} // namespace ebasic
