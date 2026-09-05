#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ebasic {

/// A declared name: its element type (for arrays, the element's type), and
/// whether it's an array and/or immutable (CONST/ENUM member). isDynamicArray
/// only matters when isArray is true: only an array declared with empty
/// parens (DIM arr() AS Type) can later be REDIM'd, matching real FB
/// ("REDIM cannot be used on fixed-size arrays").
struct SymbolInfo {
    Type type;
    bool isConst = false;
    bool isArray = false;
    bool isDynamicArray = false;
    /// Where this DIM/CONST/ENUM member was declared - populated purely for
    /// tooling (the LSP's hover/go-to-definition, via Sema::index()), never
    /// read by any check in this file itself.
    SourceLoc declLoc;
};

/// A SUB/FUNCTION's signature, registered up front (before any body is
/// checked) so calls - including recursive and mutually-recursive ones -
/// resolve regardless of source order.
struct ProcedureInfo {
    bool isFunction = false;
    Type returnType; // unused for SUB
    std::vector<Param> params;
    /// True for an `Extern`-declared (bodyless) SUB/FUNCTION - `@ProcName`
    /// (AddressOf) rejects these, since there's no eBasic-compiled body to
    /// take the address of (the real implementation lives in an external
    /// library, reached by name at link time, not by a pointer eBasic
    /// itself can produce).
    bool isExtern = false;
    /// Methods only (meaningless for free/namespace procedures): true if
    /// declared Virtual, or Override (an override necessarily participates
    /// in the vtable too). Codegen emits literal `virtual`/`override`.
    bool isVirtual = false;
    /// "" (cdecl, the default) or "stdcall" - mirrors Stmt::callConv (a
    /// free/namespace procedure's own Cdecl/Stdcall clause, meaningless
    /// for methods/operators since neither can carry one). Consumed by
    /// `@ProcName` (AddressOf) so the FunctionPointer type it produces
    /// reflects the addressed proc's real calling convention instead of
    /// always defaulting to cdecl.
    std::string callConv;
    /// Where this SUB/FUNCTION (or method) was declared - see
    /// SymbolInfo::declLoc's comment; same tooling-only purpose.
    SourceLoc declLoc;
};

/// A declared PROPERTY's value type. This version requires every property
/// to have both a getter and a setter (of the same type) - see
/// Stmt::isProperty's comment for why.
struct PropertyInfo {
    Type type;
    /// Where this PROPERTY's getter was declared (the setter may be
    /// declared elsewhere, but only one location is worth tracking for
    /// tooling) - see SymbolInfo::declLoc's comment.
    SourceLoc declLoc;
};

/// A declared TYPE's fields and member procedures, registered up front
/// (collectTypes) so a field/method can be referenced regardless of
/// declaration order. Methods follow FreeBASIC's own "declared within,
/// defined outside" split: `methods` holds the *declared* signatures (from
/// `Declare Sub/Function/Constructor/Destructor` inside the TYPE body);
/// `definedMethods`/`ctorDefined`/`dtorDefined` are filled in once the
/// matching out-of-line definitions (top-level Stmts with `ownerType` set)
/// are found, so a declared-but-never-defined method can be reported.
/// UNION never populates any of this (Sema-rejected).
struct RecordInfo {
    std::vector<FieldDecl> fields;
    std::unordered_map<std::string, ProcedureInfo> methods; // canonical method name -> signature
    std::unordered_set<std::string> definedMethods;
    bool hasCtor = false; // a Declare Constructor() was found
    bool hasDtor = false; // a Declare Destructor() was found
    bool ctorDefined = false;
    bool dtorDefined = false;
    /// Canonical name of the immediate EXTENDS base TYPE, empty if none.
    /// FreeBASIC is single-inheritance only - one base, not a list. UNION
    /// never populates this (Sema-rejected).
    std::string baseName;
    std::unordered_map<std::string, PropertyInfo> properties; // canonical property name -> value type
    std::unordered_set<std::string> definedGetters;
    std::unordered_set<std::string> definedSetters;
    /// A TYPE with zero fields, zero methods/properties, no ctor/dtor, and no
    /// EXTENDS (M4d) - an opaque, unknown-layout external handle (e.g. a C
    /// `struct sqlite3;`-style forward-only type). Only ever legal behind a
    /// PTR: no by-value DIM, no by-value embedding as another TYPE/UNION's
    /// field, and no EXTENDS in either direction. Never true for UNION (kept
    /// narrow to TYPE - an empty UNION is a degenerate case, not this
    /// feature's target).
    bool isOpaque = false;
    /// Where this TYPE/UNION was declared - see SymbolInfo::declLoc's
    /// comment; same tooling-only purpose.
    SourceLoc declLoc;
};

/// A read-only snapshot of every top-level name Sema resolved, for tooling
/// (the LSP) that needs real, resolved symbol/type information after
/// check() runs - a copy, not a reference into Sema's own (still-private)
/// maps, so those maps stay free to change shape without disturbing this
/// contract. See Sema::index().
struct SemaIndex {
    std::unordered_map<std::string, SymbolInfo> symbols;
    std::unordered_map<std::string, ProcedureInfo> procedures;
    std::unordered_map<std::string, RecordInfo> structs;
    std::unordered_set<std::string> namespaces;
};

/// Type-checks and resolves a parsed Module in place - name resolution,
/// type checking, and the structural validation described by the private
/// members below (recorded here, not repeated per-declaration, since Sema
/// has no other public surface than check()).
class Sema {
public:
    explicit Sema(DiagnosticEngine& diags) : diags_(diags) {}

    /// Runs every check pass over `module`; diags (passed to the
    /// constructor) accumulates every problem found rather than stopping at
    /// the first one - callers check DiagnosticEngine::hasErrors() after.
    void check(Module& module);

    /// A snapshot of every top-level name resolved so far - meaningful
    /// even after a check() that reported body-level errors, since
    /// module-level registration (collectProcedures/collectTypes/...)
    /// always completes before any statement body is checked. Copies out
    /// (not a reference) so a caller can hold onto it after this Sema
    /// object is gone.
    SemaIndex index() const { return SemaIndex{symbols_, procedures_, structs_, namespaces_}; }

private:
    void collectLabels(std::vector<StmtPtr>& stmts);
    /// Registers SUB/FUNCTION signatures and NAMESPACE names up front (so
    /// forward/qualified references resolve regardless of order). Recurses
    /// one level into a NamespaceDecl's body with its qualified prefix -
    /// nested NAMESPACEs are not supported yet.
    void collectProcedures(std::vector<StmtPtr>& stmts, const std::string& prefix = "");
    void collectGosubUsage(std::vector<StmtPtr>& stmts);
    /// Registers every top-level TYPE's name first (so fields can reference
    /// a TYPE declared later in the file), then resolves each one's field
    /// types (built-in trivially valid; UserDefined must name an already-
    /// registered TYPE) now that every name is known.
    void collectTypes(std::vector<StmtPtr>& stmts);
    /// Registers free-standing (global) operator overloads, keyed by
    /// (BinOp, lhs type, rhs type) - run after collectTypes so a TYPE-named
    /// parameter can be validated regardless of declaration order. Requires
    /// at least one operand to be a UserDefined TYPE (matches FB's own
    /// restriction).
    void collectOperators(std::vector<StmtPtr>& stmts);
    /// Composite lookup key for operatorOverloads_ - deliberately an exact
    /// match on both operand types (no promotion/coercion), per the plan's
    /// "keep this narrow" guidance.
    std::string operatorKey(BinOp op, const Type& lhs, const Type& rhs) const;
    /// Rejects a STRING (use ZSTRING instead) or a non-standard-layout
    /// UserDefined TYPE (one with a constructor, destructor, or virtual
    /// method) used as a parameter/return type on an `isExtern` signature
    /// (M4) - both silently break C-ABI compatibility rather than failing
    /// to compile, so this is a Sema diagnostic instead of a bizarre
    /// runtime corruption. Run after collectTypes (needs structs_
    /// populated). A TYPE with no fields/methods at all (M4d's opaque
    /// "handle" types) has no ctor/dtor/virtual method by construction, so
    /// it's already allowed through with no special-casing needed here.
    void collectExternSignatureChecks(std::vector<StmtPtr>& stmts);
    void checkBlock(std::vector<StmtPtr>& stmts, bool atTopLevel);
    void checkStmt(Stmt& stmt, bool atTopLevel);
    void checkCondition(Expr& expr, const char* what);
    Type checkExpr(Expr& expr);

    /// Looks up `key` (unqualified canonical name) in locals_ first, then
    /// symbols_ - trying the current namespace's qualified form before the
    /// bare global one, so code inside a NAMESPACE sees its own members
    /// without qualification, falling back to the global scope. Copies the
    /// result out (SymbolInfo is small; sidesteps pointer-into-map lifetime
    /// concerns across subsequent inserts). Returns false if not found.
    bool lookupSymbol(const std::string& key, SymbolInfo& out) const;

    /// Same qualified-then-bare lookup as lookupSymbol, for procedures_.
    const ProcedureInfo* findProcedure(const std::string& key) const;

    /// Builds the key to register/look up a global-scope declaration
    /// (Dim/Const/Enum at non-procedure scope) under: qualified by the
    /// current namespace if inside one, else the bare canonical name.
    std::string qualifiedKey(const std::string& name) const;

    /// Shared by both the Call expression (array read / function call) and
    /// CallStmt (procedure call as a statement): validates argument count and
    /// per-argument type/BYREF-lvalue compatibility against `proc`.
    void checkCallArgs(const ProcedureInfo& proc, std::vector<ExprPtr>& args, SourceLoc loc);
    /// Same, for calling through a stored function pointer (`cb(1, 2)`)
    /// instead of a named procedure - `fnType.funcParamTypes` (a plain
    /// vector<Type>, ByVal-only, no names/defaults - see
    /// TypeKind::FunctionPointer's own doc comment) stands in for
    /// ProcedureInfo::params. Arity is checked exactly (no default-value
    /// tolerance, since a function-pointer parameter spec has none); no
    /// BYREF handling, since every parameter is implicitly ByVal.
    void checkIndirectCallArgs(const Type& fnType, std::vector<ExprPtr>& args, SourceLoc loc);

    /// Structurally: is `expr` addressable (a variable, a field, or an array
    /// element - not a function-call result or a computed value)? Used for
    /// BYREF-argument validation and for '@' (AddressOf)'s operand. Does not
    /// itself check constness; callers that care (BYREF) check separately.
    bool isLvalue(const Expr& expr) const;

    /// Checks a value's type against a target variable/const/param type. Two
    /// different user-defined types both report kind==UserDefined, so
    /// identity there needs comparing typeName too - and, for inheritance, a
    /// value of a TYPE derived (directly or transitively) from the target's
    /// TYPE is also compatible (an implicit upcast, matching C++'s own base-
    /// pointer/reference/slicing-assignment behavior once TYPE compiles to a
    /// real C++ struct with real inheritance). A member function (not a free
    /// function) so it can walk structs_'s inheritance chains.
    bool isAssignCompatible(const Type& targetType, const Type& valueType) const;

    /// Walks up `typeKey`'s EXTENDS chain (single inheritance - a short loop,
    /// not a general MRO algorithm), returning true if `typeKey` names
    /// `baseKey` itself or (transitively) derives from it. Cycle-guarded.
    bool isSameOrDerivedFrom(const std::string& typeKey, const std::string& baseKey) const;

    /// Looks up a field/method by walking `typeKey`'s own RecordInfo first,
    /// then its EXTENDS chain (single inheritance) - so an inherited member
    /// resolves the same as one declared directly. Cycle-guarded.
    const FieldDecl* findFieldInChain(const std::string& typeKey, const std::string& fieldKey) const;
    const ProcedureInfo* findMethodInChain(const std::string& typeKey, const std::string& methodKey) const;
    const PropertyInfo* findPropertyInChain(const std::string& typeKey, const std::string& propKey) const;

    /// Structural constant-expression check for CONST initializers: literals,
    /// and Idents that refer to an already-declared CONST/ENUM member,
    /// combined with unary/binary operators. Does not compute a value -
    /// codegen re-emits the original expression and lets C++ evaluate it.
    bool isConstantExpr(const Expr& expr) const;

    /// Small integer-only constant evaluator, used only for ENUM member
    /// values (needed for auto-increment). Reports its own diagnostics and
    /// returns false on failure.
    bool evalConstInt(const Expr& expr, long long& outValue);

    std::unordered_map<std::string, SymbolInfo> symbols_; // module-level (global) names
    std::unordered_map<std::string, SymbolInfo> locals_;   // current SUB/FUNCTION's params/DIMs
    bool insideProcedure_ = false;
    Type currentFunctionReturnType_; // valid while insideProcedure_ and it's a FUNCTION
    std::unordered_map<std::string, ProcedureInfo> procedures_;
    std::unordered_map<std::string, RecordInfo> structs_; // canonical TYPE name -> its fields (never namespace-qualified; TYPE stays global-only for now)
    /// operatorKey(op, lhsType, rhsType) -> the overload's return type. See
    /// collectOperators.
    std::unordered_map<std::string, Type> operatorOverloads_;
    std::unordered_set<std::string> namespaces_; // canonical NAMESPACE names, for qualified-access resolution
    /// Canonical name of the NAMESPACE currently being checked, or empty.
    /// Only one level deep - nested NAMESPACEs aren't supported yet.
    std::string currentNamespacePrefix_;
    /// Canonical name of the TYPE whose method/constructor/destructor body
    /// is currently being checked, or empty. Only one level deep - methods
    /// don't nest, mirroring insideProcedure_'s own simplification. Lets
    /// `This` resolve its type and bare identifiers fall back to a field
    /// lookup when no local/parameter/global matches.
    std::string currentClassName_;
    std::unordered_map<std::string, long long> constIntValues_; // CONST/ENUM int value, for evalConstInt
    std::unordered_set<std::string> labels_;
    std::unordered_set<std::string> gosubTargets_; // labels referenced by at least one GOSUB
    std::unordered_set<std::string> gotoTargets_;  // labels referenced by at least one GOTO
    /// True while sequentially walking top-level statements between a
    /// GOSUB-target Label and the next Label (of any kind) - lets a bare
    /// RETURN there be valid, the same as inside a SUB.
    bool insideGosubBody_ = false;
    std::vector<LoopKind> loopStack_;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
