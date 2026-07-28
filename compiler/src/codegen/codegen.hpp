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
    // Emits a TYPE method/constructor/destructor's out-of-line C++
    // definition (`RetType eb_owner::eb_method(params) { ... }`, or the
    // constructor/destructor-specific `eb_owner::eb_owner()`/
    // `eb_owner::~eb_owner()` form - neither has a return type). The
    // *declaration* (matching real FreeBASIC's "declared within, defined
    // outside" split) is emitted separately, inside the struct body, by
    // genTypeDecl. Only ever called for top-level SubDecl/FunctionDecl
    // stmts with a non-empty `ownerType`.
    void genMethodDefinition(const Stmt& stmt);
    // Wraps a NAMESPACE's declarative members (CONST/ENUM/DIM/SUB/FUNCTION -
    // Sema already rejects anything else directly inside one) in a literal
    // C++ `namespace eb_x { ... }` block, reopened independently across
    // typesOut_/protoOut_/procOut_/globalsOut_ - matches FB's own namespaces
    // being reopenable, and needs no qualified-name bookkeeping in Codegen
    // beyond the namespaces_ set (real C++ namespaces do the rest).
    void genNamespaceDecl(const Stmt& stmt);
    std::string genExpr(const Expr& expr);
    std::string genCondition(const Expr& expr);
    // Renders the `recv.`/`this->`/`eb_base::` prefix for a Member/Call
    // whose receiver is `lhs` - shared by genExpr's Member/Call cases and
    // Assign's own codegen for a PROPERTY setter call (which can't just
    // call genExpr on the whole target, since that produces the *getter*
    // form).
    std::string memberReceiverPrefix(const Expr& lhs);

    // A unique, per-invocation name for a generated temporary or label, e.g.
    // nextName("eb__forend") -> "eb__forend0", then "eb__forend1", ...
    std::string nextName(const std::string& prefix);

    static std::string ind(int indent);
    static std::string cppType(const Type& type);
    static std::string mangleName(const std::string& name);
    // Renders a C++ parameter list ("T1 a, T2& b, ...") from BASIC params -
    // shared by a free SUB/FUNCTION's prototype+definition and a TYPE
    // method's declaration+out-of-line definition.
    static std::string buildParamList(const std::vector<Param>& params);
    // Same, but for an Operator overload's parameter list specifically: a
    // UserDefined-typed parameter is always rendered `const T&` regardless
    // of the source's BYVAL/BYREF (a plain mutable `T&`, this codebase's
    // normal BYREF rendering, can't bind a temporary - and a chained
    // expression like `a + b + c` passes the `a + b` temporary as an
    // operand, so this would otherwise fail to compile the moment any
    // operator overload is used in a chain). A primitive-typed parameter
    // is rendered by plain value, same as always.
    static std::string buildOperatorParamList(const std::vector<Param>& params);
    // The literal C++ operator token for a BinOp, used both to name an
    // Operator overload's own C++ function and, in genExpr's Binary case,
    // to render a UserDefined operand's expression when the built-in
    // special-cased forms (forced real division, std::pow, ...) must be
    // skipped in favor of plain operator-overload resolution.
    static std::string cppOperatorToken(BinOp op);
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
    // Top-level global DIM/CONST/ENUM declarations, emitted before main().
    // A class member (not a generate()-local) so genNamespaceDecl can wrap
    // the portion it contributes in a namespace block too.
    std::ostringstream globalsOut_;
    // Canonical NAMESPACE names, computed once up front in generate() (a
    // Member/Call whose base name matches this set is namespace-qualified
    // access, rendered as C++ `::`, rather than `.` field/method access).
    std::unordered_set<std::string> namespaces_;
    // Canonical TYPE name -> its EXTENDS base's canonical name (empty if
    // none), computed once up front in generate(). Needed to resolve
    // `Base.Method(args)` to a qualified non-virtual call
    // (`eb_base::eb_method(args)`) - Codegen has no other way to know the
    // current method's owning TYPE's base.
    std::unordered_map<std::string, std::string> baseTypeOf_;
    // Canonical name of the TYPE whose method/constructor/destructor is
    // currently being emitted (by genMethodDefinition), or empty. Mirrors
    // Sema::currentClassName_ - needed to resolve `Base.Method(args)`.
    std::string currentOwnerType_;
};

} // namespace ebasic
