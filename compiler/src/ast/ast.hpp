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
    /// being used somewhere a specific pointer type is required (an
    /// assignment target, a CONST initializer, a function/method argument,
    /// or a RETURN value) - isAssignCompatible's ANY-PTR bridging rule
    /// permits this (matching FreeBASIC's own documented "implicitly
    /// converted to and from other pointer types" behavior), but C++ has no
    /// implicit void* -> T* conversion (only the reverse, T* -> void*, is
    /// implicit). Codegen::genExpr wraps this expression's rendered text in
    /// an explicit static_cast<T*>(...) when this is set; null whenever no
    /// cast is needed.
    std::shared_ptr<Type> pointerCastTo;

    long long intValue = 0;
    double doubleValue = 0.0;
    std::string stringValue; // StringLiteral text, or Ident/Call/Member name
    BinOp binOp = BinOp::Add;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    std::vector<std::unique_ptr<Expr>> args; // Call
    /// Member only: true when `stringValue` names a PROPERTY rather than a
    /// plain field - set by Sema, read by Codegen to rewrite the access into
    /// a getter/setter method call (`.eb_name_get()` / `.eb_name_set(v)`)
    /// instead of plain `.eb_name` field access, since C++ has no native
    /// property syntax.
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
struct Param {
    std::string name;
    Type type;
    bool byRef;
    SourceLoc loc;
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
    /// TypeDecl only: the name after EXTENDS, empty if none. FreeBASIC is
    /// single-inheritance only - one base, not a list.
    std::string baseTypeName;

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
