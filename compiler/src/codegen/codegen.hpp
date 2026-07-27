#pragma once

#include "ast/ast.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ebasic {

class Codegen {
public:
    std::string generate(const Module& module);

private:
    void genStmt(const Stmt& stmt, std::ostringstream& out, int indent);
    void genBlock(const std::vector<StmtPtr>& stmts, std::ostringstream& out, int indent);
    // Emits a SUB/FUNCTION's prototype (into protoOut_) and body (into
    // procOut_). Only ever called for top-level SubDecl/FunctionDecl stmts.
    void genProcedure(const Stmt& stmt);
    // Emits a TYPE's C++ struct into typesOut_, recursively emitting any
    // other TYPE it embeds as a field first - struct-valued fields need the
    // embedded type's *definition* (not just a forward declaration) above
    // their own, regardless of the TYPEs' declaration order in the source.
    // Idempotent (a dependency may pull a type in before its own turn in the
    // top-level walk reaches it) and cycle-safe (a self-referential TYPE is
    // invalid C++ regardless - left for the backend to reject, not a crash
    // here).
    void genTypeDecl(const Stmt& stmt);
    std::string genExpr(const Expr& expr);
    std::string genCondition(const Expr& expr);

    // A unique, per-invocation name for a generated temporary or label, e.g.
    // nextName("eb__forend") -> "eb__forend0", then "eb__forend1", ...
    std::string nextName(const std::string& prefix);

    static std::string ind(int indent);
    static std::string cppType(const Type& type);
    static std::string mangleName(const std::string& name);
    static std::string escapeStringLiteral(const std::string& s);
    // Name of the synthesized parameterless void function a GOSUB-target
    // label's code is hoisted into. Distinct prefix from mangleName's "eb_"
    // so it can't collide with an ordinary variable of the same source name.
    static std::string gosubFunctionName(const std::string& label);

    int tempCounter_ = 0;
    // Currently-open loops, innermost last, paired with the label placed
    // right after the loop so EXIT FOR/DO/WHILE can `goto` out of however
    // many nested loops separate it from the loop kind it targets.
    std::vector<std::pair<LoopKind, std::string>> loopStack_;
    // Canonical array name -> the mangled name of the C++ variable holding
    // its lower bound, cached once at DIM time so later Call(as array-read)/
    // element-assign codegen can subtract it (arrays are runtime-sized
    // std::vectors, so their bound must be captured once at declaration, not
    // re-evaluated). Also doubles as the array-vs-function disambiguator for
    // a Call expression: present here => array, else => function call.
    std::unordered_map<std::string, std::string> arrayLowerBoundVar_;
    // Forward declarations and bodies for SUB/FUNCTION, accumulated across
    // the whole generate() call and emitted before main(). GOSUB-target
    // spans are hoisted into synthesized parameterless functions and share
    // these same two streams.
    std::ostringstream protoOut_;
    std::ostringstream procOut_;
    // Canonical labels that are the target of at least one GOSUB, computed
    // once up front in generate(). A Label matching this set is hoisted into
    // its own function instead of emitted inline.
    std::unordered_set<std::string> gosubTargets_;
    // TYPE struct definitions, emitted before globals/prototypes. See
    // genTypeDecl.
    std::ostringstream typesOut_;
    std::unordered_map<std::string, const Stmt*> typeDeclsByName_;
    std::unordered_set<std::string> typesEmitted_;
    std::unordered_set<std::string> typesBeingEmitted_; // cycle guard
};

} // namespace ebasic
