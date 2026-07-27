#include "diagnostics/diagnostics.hpp"

namespace ebasic {

void DiagnosticEngine::error(SourceLoc loc, std::string message) {
    diagnostics_.push_back(Diagnostic{Severity::Error, loc, std::move(message)});
    ++errorCount_;
}

void DiagnosticEngine::warning(SourceLoc loc, std::string message) {
    diagnostics_.push_back(Diagnostic{Severity::Warning, loc, std::move(message)});
}

int DiagnosticEngine::registerFile(const std::string& path) {
    auto it = fileIds_.find(path);
    if (it != fileIds_.end()) return it->second;
    int id = static_cast<int>(fileNames_.size());
    fileNames_.push_back(path);
    fileIds_[path] = id;
    return id;
}

void DiagnosticEngine::printAll(std::ostream& os) const {
    for (const auto& d : diagnostics_) {
        const std::string& filename =
            (d.loc.fileId >= 0 && static_cast<size_t>(d.loc.fileId) < fileNames_.size())
                ? fileNames_[static_cast<size_t>(d.loc.fileId)]
                : std::string("<unknown>");
        os << filename << ":" << d.loc.line << ":" << d.loc.column << ": "
           << (d.severity == Severity::Error ? "error: " : "warning: ") << d.message << "\n";
    }
}

} // namespace ebasic
