#include "diagnostics/diagnostics.hpp"

namespace ebasic {

void DiagnosticEngine::error(SourceLoc loc, std::string message) {
    diagnostics_.push_back(Diagnostic{Severity::Error, loc, std::move(message)});
    ++errorCount_;
}

void DiagnosticEngine::warning(SourceLoc loc, std::string message) {
    diagnostics_.push_back(Diagnostic{Severity::Warning, loc, std::move(message)});
}

void DiagnosticEngine::printAll(std::ostream& os, const std::string& filename) const {
    for (const auto& d : diagnostics_) {
        os << filename << ":" << d.loc.line << ":" << d.loc.column << ": "
           << (d.severity == Severity::Error ? "error: " : "warning: ") << d.message << "\n";
    }
}

} // namespace ebasic
