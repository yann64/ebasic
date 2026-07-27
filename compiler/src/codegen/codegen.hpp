#pragma once

#include "ast/ast.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ebasic {

class Codegen {
public:
    std::string generate(const Module& module);

private:
    void genStmt(const Stmt& stmt, std::ostringstream& out, int indent);
    void genBlock(const std::vector<StmtPtr>& stmts, std::ostringstream& out, int indent);
    std::string genExpr(const Expr& expr);
    std::string genCondition(const Expr& expr);

    // A unique, per-invocation name for a generated temporary or label, e.g.
    // nextName("eb__forend") -> "eb__forend0", then "eb__forend1", ...
    std::string nextName(const std::string& prefix);

    static std::string ind(int indent);
    static std::string cppType(TypeKind type);
    static std::string mangleName(const std::string& name);
    static std::string escapeStringLiteral(const std::string& s);

    int tempCounter_ = 0;
    // Currently-open loops, innermost last, paired with the label placed
    // right after the loop so EXIT FOR/DO/WHILE can `goto` out of however
    // many nested loops separate it from the loop kind it targets.
    std::vector<std::pair<LoopKind, std::string>> loopStack_;
};

} // namespace ebasic
