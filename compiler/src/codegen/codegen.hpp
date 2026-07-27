#pragma once

#include "ast/ast.hpp"

#include <sstream>
#include <string>

namespace ebasic {

class Codegen {
public:
    std::string generate(const Module& module);

private:
    void genStmt(const Stmt& stmt, std::ostringstream& out);
    std::string genExpr(const Expr& expr);

    static std::string cppType(TypeKind type);
    static std::string mangleName(const std::string& name);
    static std::string escapeStringLiteral(const std::string& s);
};

} // namespace ebasic
