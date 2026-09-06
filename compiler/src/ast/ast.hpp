#pragma once

#include "diagnostics/diagnostics.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace ebasic {

/// BASIC identifiers are case-insensitive; this is the canonical form used to
/// key the symbol table.
inline std::string canonicalName(const std::string& name) {
    std::string r = name;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

/// Every primitive type plus the two structural cases (UserDefined, Pointer)
/// - see Type below for how the latter two carry their extra payload.
enum class TypeKind {
    Unknown,
    Byte,
    UByte,
    Short,
    UShort,
    Integer,
    Long,
    UInteger,
    LongInt,
    ULongInt,
    Single,
    Double,
    Boolean,
    StringT,
    /// A C-compatible, null-terminated string (M4): `cppType` maps it
    /// straight to `const char*` (no separate "ZSTRING PTR" indirection
    /// needed for the common case - `ZSTRING PTR` still works too, via the
    /// ordinary Pointer-with-ZStringT-pointee path, meaning "pointer to a
    /// ZSTRING", e.g. a `char**`-shaped parameter). Bidirectionally
    /// assign-compatible with StringT: BString's own implicit `const
    /// char*` conversion operator and its `BString(const char*)`
    /// constructor do the actual marshaling in the generated C++, so
    /// neither Sema nor Codegen needs any per-call conversion logic beyond
    /// allowing the assignment.
    ZStringT,
    UserDefined, // a TYPE/CLASS instance; see Type::typeName
    Pointer,     // a PTR type; see Type::pointee (not used until M3c)
    /// A typed function pointer (`SUB (...)`/`FUNCTION (...) AS T`, with an
    /// optional Cdecl/Stdcall convention) - see Type::funcReturnType/
    /// funcParamTypes/funcCallConv. Distinct from a bare Pointer (never
    /// nested inside Pointer::pointee) so it gets its own assign-
    /// compatibility rules in Sema: identical FunctionPointer types match
    /// structurally, and it bridges freely to/from a bare ANY PTR (a
    /// Pointer with a null pointee) in both directions, matching how
    /// AddressOf's existing untyped ANY PTR callers keep working unchanged.
    FunctionPointer,
    /// M12: `Task(OF T)` (a single eventual value) or `Generator(OF T)` (a
    /// lazily-pulled sequence) - the return type of an `Async` SUB/
    /// FUNCTION. See Type::isGenerator/coroutineValueType. `T` (the OF
    /// clause) is optional only for a bare `Task` with no value at all
    /// (an `Async SUB`'s implicit return type - `coroutineValueType` null
    /// means void there); `Generator` always needs one in practice (a
    /// generator that could never yield anything isn't rejected outright,
    /// but isn't a useful shape either).
    Coroutine,
};

/// Every expression form Expr can represent - see Expr for which fields each
/// kind actually uses.
enum class ExprKind {
    IntLiteral,
    DoubleLiteral,
    StringLiteral,
    BoolLiteral,
    Ident,
    /// Identifier applied to a parenthesized, comma-separated argument list:
    /// stringValue = name, args = arguments. Ambiguous at parse time between
    /// an array-element read (exactly 1 arg) and a function call - resolved
    /// by Sema/Codegen by looking up what `stringValue` actually names.
    /// Optionally qualified (`lhs` set): `Namespace.Name(args)` or
    /// `obj.Method(args)` - lhs is the qualifier/receiver expression,
    /// resolved by Sema based on what it actually names (a namespace, or a
    /// UserDefined-typed value with a matching method) - the same
    /// "disambiguate by what it names" pattern used for the array-vs-
    /// function case above.
    Call,
    /// Field access `base.field`: lhs = base expression, stringValue = field
    /// name. `p->field` is desugared by the parser into
    /// Member{lhs=Deref{lhs=p}, stringValue=field} - Sema/Codegen need no
    /// special-casing for the arrow form.
    Member,
    Binary,
    UnaryNeg,
    UnaryNot,
    AddressOf, // `@x`: lhs = operand (must be an lvalue - see Sema::isLvalue)
    Deref,     // `*p`: lhs = pointer operand
    /// `This` - the implicit reference to the current instance inside a
    /// TYPE method. No fields used; its type is the enclosing TYPE,
    /// resolved from Sema::currentClassName_. Codegen emits the literal
    /// C++ `this` (a pointer) - `This.field` therefore needs the same
    /// arrow-vs-dot special case as a `Deref`-based Member.
    This,
    /// `Base` - refers to the current TYPE's immediate base (EXTENDS)
    /// inside a method. No fields used; its type is the base TYPE
    /// (Sema::currentClassName_'s baseName). `Base.Method(args)` is a
    /// non-virtual qualified call to the base's own implementation
    /// (bypassing any override) - Codegen emits literal `eb_base::eb_method(args)`.
    Base,
    /// M12: `AWAIT expr` - `lhs` is the awaited expression, which must
    /// resolve to a `Task(OF T)` (not `Generator` - a generator is pulled
    /// via MoveNext/Current, not awaited; not a void Task either, since
    /// there is no value to produce in an expression position - see
    /// Sema's own ExprKind::Await case). Resolves to `T` itself - the
    /// unwrapped value, exactly like real C++'s `co_await` on an
    /// Awaitable.
    Await,
};

/// Every overloadable/built-in binary operator - Codegen's cppOperatorToken
/// maps each one to its literal C++ spelling.
enum class BinOp {
    Add,
    Sub,
    Mul,
    Div,
    IDiv,
    Mod,
    Pow,
    Shl,
    Shr,
    Concat,
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge,
    And,
    Or,
    Xor,
};

/// Integer-family types, including Boolean (stored as -1/0, so bitwise ops on
/// it double as logical ops, matching FreeBASIC's AND/OR/XOR/NOT semantics).
inline bool isIntegerFamily(TypeKind t) {
    switch (t) {
        case TypeKind::Byte:
        case TypeKind::UByte:
        case TypeKind::Short:
        case TypeKind::UShort:
        case TypeKind::Integer:
        case TypeKind::Long:
        case TypeKind::UInteger:
        case TypeKind::LongInt:
        case TypeKind::ULongInt:
        case TypeKind::Boolean:
            return true;
        default:
            return false;
    }
}

inline bool isFloatFamily(TypeKind t) {
    return t == TypeKind::Single || t == TypeKind::Double;
}

inline bool isNumericType(TypeKind t) {
    return isIntegerFamily(t) || isFloatFamily(t);
}

inline bool isUnsignedType(TypeKind t) {
    switch (t) {
        case TypeKind::UByte:
        case TypeKind::UShort:
        case TypeKind::UInteger:
        case TypeKind::ULongInt:
            return true;
        default:
            return false;
    }
}

/// Byte width class used to pick the wider of two integer-family types when
/// promoting a binary operation's operands. Boolean ranks with Byte (both are
/// 1-byte types at the C++ level).
inline int integerRank(TypeKind t) {
    switch (t) {
        case TypeKind::Byte:
        case TypeKind::UByte:
        case TypeKind::Boolean:
            return 1;
        case TypeKind::Short:
        case TypeKind::UShort:
            return 2;
        case TypeKind::Integer:
        case TypeKind::Long:
        case TypeKind::UInteger:
            return 4;
        case TypeKind::LongInt:
        case TypeKind::ULongInt:
            return 8;
        default:
            return 0;
    }
}

/// Result type of combining two integer-family operands (arithmetic/bitwise).
/// Wider rank wins; same rank with mixed signedness promotes to the unsigned
/// member of the pair, matching C++'s own usual arithmetic conversions.
inline TypeKind promoteInteger(TypeKind a, TypeKind b) {
    int ra = integerRank(a);
    int rb = integerRank(b);
    if (ra != rb) return ra > rb ? a : b;
    if (isUnsignedType(a) != isUnsignedType(b)) return isUnsignedType(a) ? a : b;
    return a;
}

/// Result type of combining two float-family operands: Double wins over Single.
inline TypeKind promoteFloat(TypeKind a, TypeKind b) {
    return (a == TypeKind::Double || b == TypeKind::Double) ? TypeKind::Double : TypeKind::Single;
}

/// General numeric promotion for arithmetic/bitwise binary operators: a float
/// operand always wins (keeping its own width) over an integer operand.
/// Caller must ensure both types are numeric.
inline TypeKind promoteNumeric(TypeKind a, TypeKind b) {
    bool fa = isFloatFamily(a);
    bool fb = isFloatFamily(b);
    if (fa && fb) return promoteFloat(a, b);
    if (fa) return a;
    if (fb) return b;
    return promoteInteger(a, b);
}

/// A type: a bare TypeKind for primitives, plus a name (kind==UserDefined) or
/// a pointee (kind==Pointer, not used until M3c). The implicit constructor
/// and conversion mean every existing `TypeKind::X` literal and every
/// primitive-only helper above (isNumericType, promoteNumeric, ...) keeps
/// working unchanged - they only ever compare/switch on `.kind`, which
/// correctly falls through to "not applicable" for UserDefined/Pointer.
/// Comparing two Types by `==` is NOT the same as comparing two TypeKinds:
/// two different user-defined types both report kind==UserDefined, so
/// identity requires comparing `typeName` too - see isAssignCompatible.
struct Type {
    TypeKind kind = TypeKind::Unknown;
    std::string typeName;              // set when kind == UserDefined
    std::shared_ptr<Type> pointee;     // set when kind == Pointer

    // set when kind == FunctionPointer:
    std::shared_ptr<Type> funcReturnType;              // null == SUB (no return)
    std::shared_ptr<std::vector<Type>> funcParamTypes;  // ByVal-only, in order
    std::string funcCallConv;                           // "" (cdecl) or "stdcall"

    // set when kind == Coroutine (M12):
    bool isGenerator = false;                    // false == Task, true == Generator
    std::shared_ptr<Type> coroutineValueType;    // null == void (Task only - see TypeKind::Coroutine)

    Type() = default;
    Type(TypeKind k) : kind(k) {}
    operator TypeKind() const { return kind; }
};

/// One expression node. A single flat struct rather than a class hierarchy
/// (matching Stmt's own shape below) - `kind` selects which of the fields
/// below are meaningful, documented per-kind on ExprKind and per-field
/// where a field's use isn't already obvious from its name.
struct Expr {
    ExprKind kind;
    SourceLoc loc;
    Type type;

    /// Set by Sema exactly when this expression's own resolved type is a
    /// bare ANY PTR (Pointer with a null pointee - C++'s void*) but it's
    /// being used somewhere a specific pointer type - or (a narrower,
    /// one-directional extra bridge) a ZSTRING - is required (an
    /// assignment target, a CONST initializer, a function/method argument,
    /// or a RETURN value) - isAssignCompatible's ANY-PTR bridging rule
    /// permits this (matching FreeBASIC's own documented "implicitly
    /// converted to and from other pointer types" behavior), but C++ has no
    /// implicit void* -> T* (nor void* -> const char*) conversion (only the
    /// reverse, T* -> void*, is implicit). Codegen::genExpr wraps this
    /// expression's rendered text in an explicit static_cast<T*>(...) (or
    /// static_cast<const char*>(...) for the ZSTRING case) when this is
    /// set; null whenever no cast is needed.
    std::shared_ptr<Type> pointerCastTo;

    /// Set by Sema exactly when this is a StringLiteral being used
    /// somewhere a ZSTRING (not STRING) is expected - Codegen::genExpr
    /// then renders the bare C++ string literal instead of its usual
    /// `::ebasic::rt::BString("...")` wrap. A real string literal has
    /// static storage duration in the generated C++, so it's always safe
    /// to alias for the whole program's lifetime; the BString wrap is not
    /// just unnecessary here but actively dangerous - BString's own
    /// `operator const char*()` returns a pointer into that *temporary*
    /// BString's storage, which is destroyed at the end of the enclosing
    /// full expression. Invisible when the value is consumed within the
    /// same statement (a call argument - the temporary survives through
    /// the call), but a real, silent dangling-pointer bug for a ZSTRING
    /// value stored (a variable, an array element) for use in a *later*
    /// statement (found building a `GSubprocess` argv array: `DIM
    /// argv(n) AS ZSTRING` then `argv(0) = "echo"`, read back afterward).
    bool suppressStringWrap = false;

    long long intValue = 0;
    double doubleValue = 0.0;
    std::string stringValue; // StringLiteral text, or Ident/Call/Member name
    BinOp binOp = BinOp::Add;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    std::vector<std::unique_ptr<Expr>> args; // Call
    /// Member: true when `stringValue` names a PROPERTY rather than a
    /// plain field - set by Sema, read by Codegen to rewrite the access into
    /// a getter/setter method call (`.eb_name_get()` / `.eb_name_set(v)`)
    /// instead of plain `.eb_name` field access, since C++ has no native
    /// property syntax. Also set on a qualified Call (`expr.lhs` non-null)
    /// when `stringValue` names a function-pointer-typed PROPERTY being
    /// called through (`obj.SomeProp(1, 2)`) - Codegen appends the same
    /// `_get()` suffix before the call's own parens
    /// (`.eb_someprop_get()(1, 2)`: call the getter, then call its result).
    bool isProperty = false;
    /// AddressOf only: true when the operand (`lhs`, an Ident) names a
    /// top-level, non-extern, non-method SUB/FUNCTION (`@ProcName`) rather
    /// than an ordinary lvalue - set by Sema, read by Codegen to emit
    /// `reinterpret_cast<void*>(&eb_name)` instead of the ordinary
    /// `&(lvalue)` form. Needed for C callback-style APIs (e.g. GLib's
    /// GCallback for `g_signal_connect`), which take a callback as an
    /// untyped pointer (`ANY PTR` on the eBasic side - see Sema's
    /// AddressOf case) - converting a real function pointer to an object
    /// pointer isn't an implicit conversion in C++, unlike most other
    /// pointer conversions this language already allows, so it needs an
    /// explicit cast Codegen must insert itself.
    bool isProcAddress = false;
};

using ExprPtr = std::unique_ptr<Expr>;

/// Every statement/declaration form Stmt can represent - see Stmt for which
/// fields each kind actually uses.
enum class StmtKind {
    Dim,
    Redim,
    Const,
    Enum,
    Assign,
    Print,
    If,
    SelectCase,
    ForNext,
    DoLoop,
    WhileWend,
    Goto,
    Label,
    ExitLoop,
    SubDecl,
    FunctionDecl,
    CallStmt,
    Return,
    GoSub, // reuses `name` for the target label
    TypeDecl, // reuses `name` for the type's own name; see Stmt::fields
    NamespaceDecl, // reuses `name` and `body`; only CONST/ENUM/DIM/SUB/FUNCTION allowed directly inside
    /// A `UNION Name ... END UNION` declaration. Structurally identical to
    /// TypeDecl (reuses `name` + `fields`) - the only differences are
    /// Sema's extra "no STRING, directly or nested" restriction and
    /// Codegen emitting a C++ `union` instead of `struct`. Kept as a
    /// separate StmtKind (rather than a bool flag on TypeDecl) so that
    /// distinction reads directly off `stmt.kind` at every use site.
    UnionDecl,
    /// M12: `YIELD expr` - reuses `Stmt::expr`, the same shape `Return`
    /// already uses for its own value. Only valid inside an `Async`
    /// FUNCTION/SUB whose real return type is `Generator(OF T)`.
    Yield,
};

/// Which loop- or procedure-introducing keyword a scope was opened with.
/// EXIT FOR/DO/WHILE each target the nearest enclosing loop of the matching
/// kind specifically (which may not be the innermost loop, e.g. EXIT FOR from
/// inside a nested DO loop exits the enclosing FOR, not just the DO). Sub and
/// Function reuse the same "search the stack for a matching kind" mechanism
/// for EXIT SUB/EXIT FUNCTION and to validate RETURN's context.
enum class LoopKind {
    For,
    Do,
    While,
    Sub,
    Function,
};

/// One SUB/FUNCTION parameter. `byRef` is resolved by the parser from an
/// explicit BYVAL/BYREF keyword, or FreeBASIC's default otherwise: BYREF for
/// STRING and user-defined TYPE, BYVAL for every other built-in type.
///
/// `defaultValue` (null if this parameter is required) makes it optional -
/// a trailing `= <literal>` in the parameter list. Only ever set on a BYVAL
/// parameter (a BYREF default would need an addressable temporary to bind
/// to, not supported) and restricted by the parser to a literal expression
/// (an arbitrary per-call-site default expression isn't supported yet).
/// Once one parameter in a list has a default, every parameter after it
/// must too - matches real FreeBASIC/C++ default-argument rules, and is
/// what lets Codegen map this directly onto a real C++ default argument.
/// `shared_ptr`, not `ExprPtr` (`unique_ptr`) - `Param`/`ProcedureInfo` are
/// copied around (e.g. Sema registering a signature), and a parsed literal
/// default is small and immutable, so shared ownership is harmless.
struct Param {
    std::string name;
    Type type;
    bool byRef;
    SourceLoc loc;
    std::shared_ptr<Expr> defaultValue;
    /// M10 (generics): whether `byRef` above came from an explicit
    /// BYVAL/BYREF keyword rather than the FreeBASIC default rule -
    /// meaningless for an ordinary (non-generic) declaration, but needed
    /// when a generic SUB/FUNCTION's own type-parameter-typed parameter
    /// (defaulted to BYREF at parse time, since an unresolved type
    /// parameter parses as an ordinary UserDefined type) gets instantiated
    /// against a concrete type: Sema must re-derive `byRef` from the real,
    /// substituted type's own default (BYVAL for e.g. INTEGER) unless the
    /// user explicitly wrote BYVAL/BYREF themselves, which always wins.
    bool explicitByRef = false;
    bool explicitByVal = false;
};

/// One field of a TYPE. Deliberately not reusing Param - fields need no byRef.
struct FieldDecl {
    std::string name;
    Type type;
    SourceLoc loc;
};

/// DO ... LOOP's optional pre-test (after DO) and post-test (after LOOP).
enum class LoopTest {
    None,
    While,
    Until,
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

/// One CASE arm of a SELECT CASE: a list of values to match (empty + isElse
/// for CASE ELSE) and the statements to run when matched. CASE val1 TO val2
/// and CASE IS <op> val ranges are not supported yet.
struct CaseArm {
    std::vector<ExprPtr> matches;
    bool isElse = false;
    std::vector<StmtPtr> body;
};

/// One member of an ENUM: an optional explicit value (nullptr => previous + 1,
/// or 0 for the first member). `resolvedValue` is filled in by Sema (which
/// must evaluate it to support auto-increment) so Codegen doesn't need its
/// own constant evaluator.
struct EnumMember {
    std::string name;
    ExprPtr value;
    SourceLoc loc;
    long long resolvedValue = 0;
};

/// One statement or declaration node - covers everything from a plain DIM to
/// a whole TYPE/NAMESPACE body. A single flat struct rather than one class
/// per kind (deliberately - see StmtKind for the full list this must cover);
/// `kind` selects which of the many fields below are meaningful, each
/// annotated inline with which StmtKind(s) use it.
struct Stmt {
    StmtKind kind;
    SourceLoc loc;

    /// M7: the joined ("\n"-separated) text of every consecutive `'''` doc
    /// comment line immediately preceding this statement, empty if none.
    /// Only ever populated by the parser for top-level SubDecl/
    /// FunctionDecl/TypeDecl/UnionDecl/NamespaceDecl/Const/Enum (see
    /// Parser::collectDocComment) - never for anything else, even when a
    /// `'''` line happens to precede it syntactically.
    std::string docComment;

    std::string name;              // Dim, Const, Assign, ForNext (loop var), Goto/Label, TypeDecl
    Type declaredType;             // Dim (required); Const (optional - inferred if Unknown); Redim (optional restated AS type)
    ExprPtr expr;                               // Assign/Const value; If/SelectCase/WhileWend condition/selector; ForNext start
    std::vector<ExprPtr> args;                  // Print

    bool isArray = false;         // Dim
    /// Dim: array bounds. Both null => not an array. arrayUpper null but
    /// isArray true => DIM name() - an empty-parens dynamic array (size 0
    /// until REDIM'd; not a fixed-size array, so REDIM-able).
    /// Redim: the new bounds (arrayUpper is always given; arrayLower null => 0).
    ExprPtr arrayLower;
    ExprPtr arrayUpper;
    ExprPtr index;                // Assign: non-null => array-element assignment to name(index) (fast path)
    /// Assign: non-null => a general Member/Call-chain lvalue (e.g. obj.field
    /// or arr(i).field), used instead of name/index when the target involves
    /// a '.'. Takes priority over name/index when present.
    ExprPtr target;
    bool preserve = false;        // Redim: REDIM PRESERVE keeps existing elements

    std::vector<EnumMember> enumMembers; // Enum
    std::vector<FieldDecl> fields;       // TypeDecl
    /// TypeDecl only: `Declare Sub/Function/Constructor/Destructor` method
    /// prototypes found inside the TYPE body - SubDecl/FunctionDecl-kind
    /// Stmts with `params`/`declaredType` set but an empty `body` (the real
    /// body lives in a separate top-level out-of-line definition, matching
    /// real FreeBASIC's "declared within, defined outside" member procedure
    /// rule). UNION cannot have any (Sema-rejected - FB unions may not
    /// contain members with constructors/destructors).
    std::vector<StmtPtr> methods;
    /// TypeDecl/UnionDecl only: the raw, comma-separated name list from an
    /// EXTENDS clause, exactly as parsed (M11, multiple-interface
    /// implementation) - e.g. `EXTENDS BaseWidget, IClickable, IResizable`
    /// parses to `["BaseWidget", "IClickable", "IResizable"]`. UNION still
    /// cannot actually use EXTENDS (Sema-rejected, unchanged) - the parser
    /// itself doesn't know that yet, matching how baseTypeName's own
    /// rejection was already Sema's job, not the parser's.
    std::vector<std::string> extendsNames;
    /// TypeDecl only, set by Sema's collectTypes (not the parser - classifying
    /// each `extendsNames` entry needs that name's own already-parsed shape,
    /// which the parser doesn't have visibility into): the *one* name from
    /// `extendsNames` that resolved to an ordinary (non-interface) base, or
    /// empty if none - the single-inheritance chain every existing
    /// baseName-walking check (cycle detection, Override-matches-Virtual,
    /// field/method/property lookup) still follows completely unchanged.
    std::string baseTypeName;
    /// TypeDecl only, set by Sema's collectTypes: every other `extendsNames`
    /// entry that resolved to a *pure interface* (a TYPE with zero fields,
    /// no constructor/destructor, and every method Virtual) - additional
    /// `: public InterfaceX` base clauses Codegen emits alongside
    /// `baseTypeName`'s own. Checked by findMethodInChain/findPropertyInChain/
    /// isSameOrDerivedFrom at every level of the baseTypeName chain (see
    /// their own doc comments), not just the starting TYPE, so a
    /// transitively-derived TYPE still sees an ancestor's own interfaces.
    std::vector<std::string> interfaceNames;
    /// TypeDecl only, set by Sema's collectTypes: true when *this* TYPE's
    /// own shape (zero fields, no ctor/dtor, every declared method Virtual,
    /// at least one method) itself qualifies as a pure interface - the
    /// classification a *different* TYPE's own `extendsNames` resolution
    /// consults to decide whether a named base belongs in `baseTypeName` or
    /// `interfaceNames`. An interface's own declared methods are exempt
    /// from the ordinary "declared but never defined" check (Sema's Pass 6)
    /// and are emitted `= 0` (a real pure virtual, no out-of-line body -
    /// see Codegen) rather than expecting one - the whole point of an
    /// interface is that *implementers* provide the body, not the
    /// interface TYPE itself.
    bool isInterface = false;

    /// If: one condition per IF/ELSEIF branch, and one body per branch in
    /// `blocks`. If hasElse, `blocks` has one extra trailing entry for ELSE.
    std::vector<ExprPtr> conditions;
    std::vector<std::vector<StmtPtr>> blocks;
    bool hasElse = false;

    std::vector<CaseArm> cases; // SelectCase

    ExprPtr forEnd;               // ForNext: TO bound
    ExprPtr forStep;              // ForNext: STEP value (nullptr => 1)
    std::vector<StmtPtr> body;    // ForNext / WhileWend / DoLoop body

    LoopTest preTest = LoopTest::None;  // DoLoop: DO [WHILE|UNTIL cond]
    ExprPtr preCond;
    LoopTest postTest = LoopTest::None; // DoLoop: LOOP [WHILE|UNTIL cond]
    ExprPtr postCond;

    LoopKind exitKind = LoopKind::For; // ExitLoop: which loop kind to exit

    std::vector<Param> params;  // SubDecl/FunctionDecl (declaredType holds the FUNCTION's return type)
    /// SubDecl/FunctionDecl only (M10, generics): one or more type-parameter
    /// names from a `(OF T[, U...])` clause after the name, before the
    /// regular parameter list. Non-empty marks this declaration as generic -
    /// `T`/`U`/... are usable as ordinary type names anywhere in `params`/
    /// `declaredType`/`body` (parsed as an ordinary UserDefined Type, same as
    /// any ordinary TYPE reference; nothing in the parser itself needs to
    /// know these names are special). A generic declaration is never fully
    /// type-checked at its own definition site - only registered - and is
    /// never itself emitted by Codegen; Sema instead clones+substitutes a
    /// concrete copy per distinct call-site instantiation and appends that
    /// synthesized, ordinary (non-generic) Stmt to the module's own
    /// top-level stmts (see Sema's genericProcedures_/instantiations_).
    /// Only free (non-method, `ownerType` empty) SUB/FUNCTION supports this
    /// so far - a generic TYPE is a separate, later feature.
    std::vector<std::string> typeParams;
    /// Assign: true if `name` is not a real variable but the enclosing
    /// FUNCTION's own name, used as its return-value pseudo-assignment
    /// (`FuncName = value` inside FUNCTION FuncName ... END FUNCTION).
    /// Set by the parser, which already tracks the enclosing function.
    bool isReturnAssign = false;

    /// SubDecl/FunctionDecl only, for TYPE member procedures (both the
    /// Declare-prototype form stored in TypeDecl::methods, and the
    /// out-of-line definition form at top level): `ownerType` is the
    /// canonical-cased owning TYPE's name, empty for an ordinary free
    /// SUB/FUNCTION. `isCtor`/`isDtor` mark a Constructor/Destructor -
    /// always SubDecl-kind (no return value), named after the TYPE itself
    /// rather than a separate method name. Only a no-argument
    /// constructor/destructor is supported so far (deliberately deferred:
    /// parameterized construction, and constructor/method overloading).
    std::string ownerType;
    bool isCtor = false;
    bool isDtor = false;
    /// Method prototypes only (meaningful on the Declare-form Stmt stored in
    /// TypeDecl::methods; parsed-and-ignored on the out-of-line definition,
    /// where repeating `Virtual`/`Override` mirrors real FreeBASIC syntax
    /// but C++ itself only allows them on the in-class declaration).
    /// `isOverride` implies virtual too (an override necessarily
    /// participates in the vtable), whether or not `Virtual` was also
    /// explicitly written - Sema doesn't require both.
    bool isVirtual = false;
    bool isOverride = false;
    /// `Declare Property Name(...)` / out-of-line `Property TypeName.Name(...)`
    /// - a getter (kind==FunctionDecl, 0 params, has a return type) or a
    /// setter (kind==SubDecl, exactly 1 param, no return type),
    /// disambiguated by signature exactly like real FreeBASIC. This version
    /// requires every property to have both a getter and a setter (of the
    /// same type) - deliberately simpler than allowing a read-only/write-
    /// only property, which would need read/write-context-sensitive
    /// resolution at every use site instead of always-both being assumed.
    /// Also reused, on a `StmtKind::CallStmt` node specifically, for the
    /// unrelated-but-analogous "calling through a function-pointer-typed
    /// PROPERTY as a statement" case (`CALL obj.SomeProp(1, 2)`) - safe
    /// since a single Stmt is never simultaneously a SubDecl/FunctionDecl
    /// and a CallStmt; see Expr::isProperty's own doc comment for the
    /// expression-position sibling of this same feature.
    bool isProperty = false;
    /// `Operator SYMBOL(lhs, rhs) AS type ... End Operator` - a free-standing
    /// (global) binary operator overload, always `kind == FunctionDecl` and
    /// always top-level (`ownerType` empty - member-declared operators,
    /// which real FreeBASIC also supports, are deliberately out of scope
    /// this slice). `name` holds the operator's textual symbol (e.g. "+"),
    /// used only for diagnostics - lookup is keyed by `operatorBinOp` plus
    /// the two params' types, not by name.
    bool isOperator = false;
    BinOp operatorBinOp = BinOp::Add;

    /// `Declare Sub/Function ...` at the top level (standalone, or inside an
    /// `Extern ... End Extern` block) - a signature with no eBasic-side
    /// body at all (M4); the real definition lives in an external C/C++
    /// library. Always top-level (`ownerType` empty) - a genuinely
    /// different case from M3e's "declared in TYPE, defined out-of-line in
    /// this same file" methods, which still get a body eventually.
    bool isExtern = false;
    std::string externLinkage; // "C" or "C++"
    /// The real external symbol name, exactly as it must appear to the
    /// linker (case preserved, no namespace mangling) - sourced from an
    /// `Alias "name"` clause if given, else `name` itself as written.
    /// Codegen must emit this verbatim (never through mangleName) for both
    /// the prototype and every call site.
    std::string externAlias;
    std::string externLib; // a `Lib "name"` clause; empty = none given
    /// A `[Cdecl]`/`[Stdcall]` clause (M8f, and - since this field is no
    /// longer extern-only - the same clause on a plain top-level
    /// SUB/FUNCTION's own header, see parseSub/parseFunction): "" (the
    /// default, meaning the platform's own default convention - `cdecl`
    /// in practice) or "stdcall". On a DECLARE (`isExtern`), the parser
    /// (see parseExternDecl) forces `externLinkage` to "C" whenever
    /// Stdcall is written, the same way it already does for Cdecl, so
    /// "stdcall" paired with "C++" linkage never actually reaches
    /// Codegen. On a plain, non-extern top-level SUB/FUNCTION, marks the
    /// eBasic-compiled *definition* itself `__stdcall` - needed so
    /// `@ProcName`'s own resolved FunctionPointer type can reflect a real
    /// calling convention (see Sema's AddressOf case) instead of always
    /// defaulting to cdecl, e.g. to hand an eBasic-defined callback to a
    /// real Win32 API expecting `Stdcall` (`EnumWindows`, `SetTimer`) on
    /// 32-bit x86. Never set on a TYPE method (parseSub/parseFunction
    /// reject Stdcall there with an immediate diagnostic) or an Operator
    /// overload (parsed via a separate function entirely that never
    /// touches this field). A no-op on every non-Windows target, where
    /// `__stdcall` isn't defined at all - Codegen's `EBASIC_STDCALL`
    /// macro (generate()'s preamble) expands to nothing there, same
    /// shape as `EBASIC_EXPORT`'s own platform split.
    std::string callConv;

    /// `Sub`/`Function ... End Sub`/`End Function` with a real body,
    /// written *inside* an `Extern "C" ... End Extern` block (shared-
    /// library support) - the opt-in mechanism for giving a real,
    /// eBasic-authored procedure a stable, unmangled, dynamically-
    /// loadable C symbol (e.g. a Haiku add-on's own real entry point).
    /// Distinct from `isExtern` (which always means "no body, imported
    /// from elsewhere") - this one always has a real `body`. Reuses
    /// `externLinkage`/`externAlias` above for the same purpose those
    /// already serve for imports; `externLinkage` is always "C" here
    /// (enforced by the parser - a mangled "C++"-linkage "export" isn't
    /// a stable ABI boundary) and `externLib` is unused/empty (no `Lib`
    /// clause is meaningful for a real definition).
    bool isExported = false;
    /// M12: SubDecl/FunctionDecl only - an `Async` SUB/FUNCTION, parsed
    /// right after the parameter list (`FUNCTION Name(...) Async AS
    /// Task(OF T)` / `SUB Name(...) Async`). A FUNCTION's `declaredType`
    /// is the *full* `Task(OF T)`/`Generator(OF T)` Coroutine type (what
    /// calling it as a plain expression, without AWAIT, produces - real
    /// C++'s own "calling a coroutine function returns its coroutine
    /// return-object type immediately" semantics); a SUB's is implicitly
    /// `Task` with no value (never written explicitly - a bare SUB has no
    /// `AS` clause at all, matching every ordinary SUB). Never combined
    /// with a TYPE method or a generic (OF T) clause this round.
    bool isAsync = false;
};

/// The parsed result of one compilation: every top-level statement, plus the
/// bookkeeping (externLibs) that must survive past the AST itself into the
/// driver's final link step.
struct Module {
    std::vector<StmtPtr> stmts;
    /// Library names named in a `Lib "name"` clause on an EXTERN/DECLARE
    /// (M4), collected directly by the parser (a static string needs no
    /// type-checking) and passed straight through Codegen to the driver,
    /// which appends one `-l<name>` per entry to the backend invocation.
    std::vector<std::string> externLibs;
};

} // namespace ebasic
