#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ebasic {

class Sema {
public:
    explicit Sema(DiagnosticEngine& diags) : diags_(diags) {}

    void check(Module& module);

private:
    void collectLabels(std::vector<StmtPtr>& stmts);
    void checkBlock(std::vector<StmtPtr>& stmts, bool atTopLevel);
    void checkStmt(Stmt& stmt, bool atTopLevel);
    void checkCondition(Expr& expr, const char* what);
    TypeKind checkExpr(Expr& expr);
    static std::string canonicalName(const std::string& name);

    std::unordered_map<std::string, TypeKind> symbols_;
    std::unordered_set<std::string> labels_;
    std::vector<LoopKind> loopStack_;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
