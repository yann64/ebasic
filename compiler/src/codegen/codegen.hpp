#pragma once

#include "ast/ast.hpp"

#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ebasic {

/// Lowers a Sema-checked Module into a single C++ translation unit's worth of
/// source text (generate()), or, for a `--lib` build, an auto-generated
/// `.bas` interface file for dependent packages (generateLibraryInterface).
/// One Codegen instance is meant for one module - the streams below
/// accumulate across a single generate() call.
class Codegen {
public:
    /// `libMode` (M5): when true, the top-level statement stream is never
    /// wrapped in `int main() { ... }` - the caller (ebc's driver) has
    /// already verified the module contains no top-level *executable*
    /// statements (only declarations), since a library's object file must
    /// never define `main` itself (it would collide with the consuming
    /// package's own `main` at final link time).
    std::string generate(const Module& module, bool libMode = false);

    /// M5: renders an auto-generated, real `.bas`-syntax interface file for
    /// a library build - an `Extern "C++" Lib "<libName>" ... End Extern`
    /// block with one `Declare` per public top-level SUB/FUNCTION (each
    /// `Alias`ed to its real, already-`mangleName`d symbol - no change to
    /// ordinary mangling/linkage at all), plus a verbatim copy of any
    /// top-level TYPE/UNION with no methods/ctor/dtor (a plain-data or
    /// opaque record - the only kinds already allowed across an EXTERN
    /// boundary per M4b/M4d's own Sema rules). A dependent package
    /// `#include`s this file and links the archive produced alongside it -
    /// reusing M4's EXTERN/DECLARE/opaque-TYPE machinery entirely on the
    /// consuming side. Only ever meaningful after a `libMode` generate()
    /// call (needs no state from it, but is conceptually paired). Namespaced
    /// members are not walked (deferred - only top-level procs/types are
    /// exported for now).
    std::string generateLibraryInterface(const Module& module, const std::string& libName);

    /// Library names to link (`-l<name>`), collected from the module's
    /// `Lib "name"` clauses (M4) - valid after generate() returns.
    const std::vector<std::string>& externLibs() const { return externLibs_; }

private:
    /// Renders `type` back into BASIC source syntax (e.g. "INTEGER", "STRING",
    /// "Point PTR") - the inverse of parsing a type, needed only by
    /// generateLibraryInterface to re-emit real `.bas` text. Never used by
    /// ordinary C++ codegen (which goes through cppType instead).
    static std::string basicTypeName(const Type& type);
    void genStmt(const Stmt& stmt, std::ostringstream& out, int indent);
    void genBlock(const std::vector<StmtPtr>& stmts, std::ostringstream& out, int indent);
    /// Emits a SUB/FUNCTION's prototype (into protoOut_) and body (into
    /// procOut_). Only ever called for top-level SubDecl/FunctionDecl stmts.
    void genProcedure(const Stmt& stmt);
    /// Emits a TYPE's C++ struct into typesOut_, recursively emitting any
    /// other TYPE it embeds as a field first - struct-valued fields need the
    /// embedded type's *definition* (not just a forward declaration) above
    /// their own, regardless of the TYPEs' declaration order in the source.
    /// Idempotent (a dependency may pull a type in before its own turn in the
    /// top-level walk reaches it) and cycle-safe (a self-referential TYPE is
    /// invalid C++ regardless - left for the backend to reject, not a crash
    /// here).
    void genTypeDecl(const Stmt& stmt);
    /// Emits a TYPE method/constructor/destructor's out-of-line C++
    /// definition (`RetType eb_owner::eb_method(params) { ... }`, or the
    /// constructor/destructor-specific `eb_owner::eb_owner()`/
    /// `eb_owner::~eb_owner()` form - neither has a return type). The
    /// *declaration* (matching real FreeBASIC's "declared within, defined
    /// outside" split) is emitted separately, inside the struct body, by
    /// genTypeDecl. Only ever called for top-level SubDecl/FunctionDecl
    /// stmts with a non-empty `ownerType`.
    void genMethodDefinition(const Stmt& stmt);
    /// Wraps a NAMESPACE's declarative members (CONST/ENUM/DIM/SUB/FUNCTION -
    /// Sema already rejects anything else directly inside one) in a literal
    /// C++ `namespace eb_x { ... }` block, reopened independently across
    /// typesOut_/protoOut_/procOut_/globalsOut_ - matches FB's own namespaces
    /// being reopenable, and needs no qualified-name bookkeeping in Codegen
    /// beyond the namespaces_ set (real C++ namespaces do the rest).
    void genNamespaceDecl(const Stmt& stmt);
    /// Renders `expr` into C++, then wraps the result in an explicit
    /// `static_cast<T*>(...)` (or `static_cast<const char*>(...)` for the
    /// ANY-PTR-as-ZSTRING bridge) when Sema set `expr.pointerCastTo` (an ANY
    /// PTR value being bridged into a typed pointer - or ZSTRING - context;
    /// C++, unlike this language, has no implicit void* -> T*/const char*
    /// conversion) - or, when Sema set `expr.suppressStringWrap`, renders
    /// the bare string literal instead of genExprBase's usual
    /// `BString("...")` wrap (see that field's own doc comment: the wrap
    /// would otherwise construct a dangling-once-destroyed temporary
    /// wherever a ZSTRING target outlives the current statement). The
    /// actual per-ExprKind rendering lives in genExprBase; every recursive
    /// call for a sub-expression goes through this wrapper too, so neither
    /// the cast nor the unwrap is ever missed regardless of where in a
    /// larger expression the annotated node sits.
    std::string genExpr(const Expr& expr);
    /// The real per-ExprKind rendering that genExpr wraps - never call this
    /// directly except from within genExpr itself or genExprBase's own
    /// recursive sub-expression calls (which intentionally go through
    /// genExpr, not this).
    std::string genExprBase(const Expr& expr);
    std::string genCondition(const Expr& expr);
    /// Renders the `recv.`/`this->`/`eb_base::` prefix for a Member/Call
    /// whose receiver is `lhs` - shared by genExpr's Member/Call cases and
    /// Assign's own codegen for a PROPERTY setter call (which can't just
    /// call genExpr on the whole target, since that produces the *getter*
    /// form).
    std::string memberReceiverPrefix(const Expr& lhs);
    /// Renders the full callee expression (name, plus prefix) for a Call
    /// expression/CallStmt naming `name`, qualified via receiver `lhs`
    /// (nullptr = an unqualified, free-standing call). Consults
    /// externProcNames_ first - a plain free call or a Namespace.Name(args)
    /// call may be bound to a real EXTERN/DECLARE procedure (M4), which
    /// must be called by its real external name verbatim, never
    /// mangleName. Falls back to memberReceiverPrefix + mangleName(name)
    /// for everything else (This/Base/a general receiver, or an ordinary
    /// non-extern procedure/namespace member).
    std::string resolveCalleeName(const Expr* lhs, const std::string& name);
    /// Recursively collects every isExtern SubDecl/FunctionDecl's real
    /// external name into externProcNames_, keyed by its (possibly
    /// namespace-qualified) canonical BASIC name - recurses into
    /// NamespaceDecl bodies so an Extern "C++" NAMESPACE binding (M4c) is
    /// found too, not just top-level EXTERN declarations (M4b).
    void collectExternProcNames(const std::vector<StmtPtr>& stmts, const std::string& keyPrefix,
                                const std::string& realPrefix);

    /// A unique, per-invocation name for a generated temporary or label, e.g.
    /// nextName("eb__forend") -> "eb__forend0", then "eb__forend1", ...
    std::string nextName(const std::string& prefix);

    static std::string ind(int indent);
    static std::string cppType(const Type& type);
    static std::string mangleName(const std::string& name);
    /// Renders a C++ parameter list ("T1 a, T2& b, ...") from BASIC params -
    /// shared by a free SUB/FUNCTION's prototype+definition and a TYPE
    /// method's declaration+out-of-line definition.
    static std::string buildParamList(const std::vector<Param>& params);
    /// Same, but for an Operator overload's parameter list specifically: a
    /// UserDefined-typed parameter is always rendered `const T&` regardless
    /// of the source's BYVAL/BYREF (a plain mutable `T&`, this codebase's
    /// normal BYREF rendering, can't bind a temporary - and a chained
    /// expression like `a + b + c` passes the `a + b` temporary as an
    /// operand, so this would otherwise fail to compile the moment any
    /// operator overload is used in a chain). A primitive-typed parameter
    /// is rendered by plain value, same as always.
    static std::string buildOperatorParamList(const std::vector<Param>& params);
    /// The literal C++ operator token for a BinOp, used both to name an
    /// Operator overload's own C++ function and, in genExpr's Binary case,
    /// to render a UserDefined operand's expression when the built-in
    /// special-cased forms (forced real division, std::pow, ...) must be
    /// skipped in favor of plain operator-overload resolution.
    static std::string cppOperatorToken(BinOp op);
    static std::string escapeStringLiteral(const std::string& s);
    /// Name of the synthesized parameterless void function a GOSUB-target
    /// label's code is hoisted into. Distinct prefix from mangleName's "eb_"
    /// so it can't collide with an ordinary variable of the same source name.
    static std::string gosubFunctionName(const std::string& label);

    int tempCounter_ = 0;
    /// Currently-open loops, innermost last, paired with the label placed
    /// right after the loop so EXIT FOR/DO/WHILE can `goto` out of however
    /// many nested loops separate it from the loop kind it targets.
    std::vector<std::pair<LoopKind, std::string>> loopStack_;
    /// Canonical array name -> the mangled name of the C++ variable holding
    /// its lower bound, cached once at DIM time so later Call(as array-read)/
    /// element-assign codegen can subtract it (arrays are runtime-sized
    /// std::vectors, so their bound must be captured once at declaration, not
    /// re-evaluated). Also doubles as the array-vs-function disambiguator for
    /// a Call expression: present here => array, else => function call.
    std::unordered_map<std::string, std::string> arrayLowerBoundVar_;
    /// Forward declarations and bodies for SUB/FUNCTION, accumulated across
    /// the whole generate() call and emitted before main(). GOSUB-target
    /// spans are hoisted into synthesized parameterless functions and share
    /// these same two streams.
    std::ostringstream protoOut_;
    std::ostringstream procOut_;
    /// Canonical labels that are the target of at least one GOSUB, computed
    /// once up front in generate(). A Label matching this set is hoisted into
    /// its own function instead of emitted inline.
    std::unordered_set<std::string> gosubTargets_;
    /// TYPE struct definitions, emitted before globals/prototypes. See
    /// genTypeDecl.
    std::ostringstream typesOut_;
    std::unordered_map<std::string, const Stmt*> typeDeclsByName_;
    std::unordered_set<std::string> typesEmitted_;
    std::unordered_set<std::string> typesBeingEmitted_; // cycle guard
    /// Top-level global DIM/CONST/ENUM declarations, emitted before main().
    /// A class member (not a generate()-local) so genNamespaceDecl can wrap
    /// the portion it contributes in a namespace block too.
    std::ostringstream globalsOut_;
    /// Canonical NAMESPACE names, computed once up front in generate() (a
    /// Member/Call whose base name matches this set is namespace-qualified
    /// access, rendered as C++ `::`, rather than `.` field/method access).
    std::unordered_set<std::string> namespaces_;
    /// See the public externLibs() accessor.
    std::vector<std::string> externLibs_;
    /// Canonical BASIC name -> real external symbol name (the `Alias`, or
    /// the declared name as-is), computed once up front in generate() for
    /// every top-level `isExtern` SubDecl/FunctionDecl (M4). A call site
    /// referencing one of these must emit the real name verbatim - never
    /// through mangleName, which would rename it to something the linker
    /// can't find in the external library at all.
    std::unordered_map<std::string, std::string> externProcNames_;
    /// Canonical TYPE name -> its EXTENDS base's canonical name (empty if
    /// none), computed once up front in generate(). Needed to resolve
    /// `Base.Method(args)` to a qualified non-virtual call
    /// (`eb_base::eb_method(args)`) - Codegen has no other way to know the
    /// current method's owning TYPE's base.
    std::unordered_map<std::string, std::string> baseTypeOf_;
    /// Canonical name of the TYPE whose method/constructor/destructor is
    /// currently being emitted (by genMethodDefinition), or empty. Mirrors
    /// Sema::currentClassName_ - needed to resolve `Base.Method(args)`.
    std::string currentOwnerType_;
};

} // namespace ebasic
