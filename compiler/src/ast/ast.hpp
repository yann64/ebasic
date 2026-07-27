#pragma once

#include "diagnostics/diagnostics.hpp"

#include <cctype>
#include <memory>
#include <string>
#include <vector>

namespace ebasic {

// BASIC identifiers are case-insensitive; this is the canonical form used to
// key the symbol table.
inline std::string canonicalName(const std::string& name) {
    std::string r = name;
    for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

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
};

enum class ExprKind {
    IntLiteral,
    DoubleLiteral,
    StringLiteral,
    BoolLiteral,
    Ident,
    // Identifier applied to a parenthesized, comma-separated argument list:
    // stringValue = name, args = arguments. Ambiguous at parse time between
    // an array-element read (exactly 1 arg) and a function call - resolved
    // by Sema/Codegen by looking up what `stringValue` actually names.
    Call,
    Binary,
    UnaryNeg,
    UnaryNot,
};

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

// Integer-family types, including Boolean (stored as -1/0, so bitwise ops on
// it double as logical ops, matching FreeBASIC's AND/OR/XOR/NOT semantics).
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

// Byte width class used to pick the wider of two integer-family types when
// promoting a binary operation's operands. Boolean ranks with Byte (both are
// 1-byte types at the C++ level).
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

// Result type of combining two integer-family operands (arithmetic/bitwise).
// Wider rank wins; same rank with mixed signedness promotes to the unsigned
// member of the pair, matching C++'s own usual arithmetic conversions.
inline TypeKind promoteInteger(TypeKind a, TypeKind b) {
    int ra = integerRank(a);
    int rb = integerRank(b);
    if (ra != rb) return ra > rb ? a : b;
    if (isUnsignedType(a) != isUnsignedType(b)) return isUnsignedType(a) ? a : b;
    return a;
}

// Result type of combining two float-family operands: Double wins over Single.
inline TypeKind promoteFloat(TypeKind a, TypeKind b) {
    return (a == TypeKind::Double || b == TypeKind::Double) ? TypeKind::Double : TypeKind::Single;
}

// General numeric promotion for arithmetic/bitwise binary operators: a float
// operand always wins (keeping its own width) over an integer operand.
// Caller must ensure both types are numeric.
inline TypeKind promoteNumeric(TypeKind a, TypeKind b) {
    bool fa = isFloatFamily(a);
    bool fb = isFloatFamily(b);
    if (fa && fb) return promoteFloat(a, b);
    if (fa) return a;
    if (fb) return b;
    return promoteInteger(a, b);
}

struct Expr {
    ExprKind kind;
    SourceLoc loc;
    TypeKind type = TypeKind::Unknown;

    long long intValue = 0;
    double doubleValue = 0.0;
    std::string stringValue; // StringLiteral text, or Ident/Call name
    BinOp binOp = BinOp::Add;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
    std::vector<std::unique_ptr<Expr>> args; // Call
};

using ExprPtr = std::unique_ptr<Expr>;

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
};

// Which loop- or procedure-introducing keyword a scope was opened with.
// EXIT FOR/DO/WHILE each target the nearest enclosing loop of the matching
// kind specifically (which may not be the innermost loop, e.g. EXIT FOR from
// inside a nested DO loop exits the enclosing FOR, not just the DO). Sub and
// Function reuse the same "search the stack for a matching kind" mechanism
// for EXIT SUB/EXIT FUNCTION and to validate RETURN's context.
enum class LoopKind {
    For,
    Do,
    While,
    Sub,
    Function,
};

// One SUB/FUNCTION parameter. `byRef` is resolved by the parser from an
// explicit BYVAL/BYREF keyword, or FreeBASIC's default otherwise: BYREF for
// STRING, BYVAL for every other built-in type.
struct Param {
    std::string name;
    TypeKind type;
    bool byRef;
    SourceLoc loc;
};

// DO ... LOOP's optional pre-test (after DO) and post-test (after LOOP).
enum class LoopTest {
    None,
    While,
    Until,
};

struct Stmt;
using StmtPtr = std::unique_ptr<Stmt>;

// One CASE arm of a SELECT CASE: a list of values to match (empty + isElse
// for CASE ELSE) and the statements to run when matched. CASE val1 TO val2
// and CASE IS <op> val ranges are not supported yet.
struct CaseArm {
    std::vector<ExprPtr> matches;
    bool isElse = false;
    std::vector<StmtPtr> body;
};

// One member of an ENUM: an optional explicit value (nullptr => previous + 1,
// or 0 for the first member). `resolvedValue` is filled in by Sema (which
// must evaluate it to support auto-increment) so Codegen doesn't need its
// own constant evaluator.
struct EnumMember {
    std::string name;
    ExprPtr value;
    SourceLoc loc;
    long long resolvedValue = 0;
};

struct Stmt {
    StmtKind kind;
    SourceLoc loc;

    std::string name;                          // Dim, Const, Assign, ForNext (loop var), Goto/Label
    TypeKind declaredType = TypeKind::Unknown;  // Dim (required); Const (optional - inferred if Unknown); Redim (optional restated AS type)
    ExprPtr expr;                               // Assign/Const value; If/SelectCase/WhileWend condition/selector; ForNext start
    std::vector<ExprPtr> args;                  // Print

    bool isArray = false;         // Dim
    // Dim: array bounds. Both null => not an array. arrayUpper null but
    // isArray true => DIM name() - an empty-parens dynamic array (size 0
    // until REDIM'd; not a fixed-size array, so REDIM-able).
    // Redim: the new bounds (arrayUpper is always given; arrayLower null => 0).
    ExprPtr arrayLower;
    ExprPtr arrayUpper;
    ExprPtr index;                // Assign: non-null => array-element assignment to name(index)
    bool preserve = false;        // Redim: REDIM PRESERVE keeps existing elements

    std::vector<EnumMember> enumMembers; // Enum

    // If: one condition per IF/ELSEIF branch, and one body per branch in
    // `blocks`. If hasElse, `blocks` has one extra trailing entry for ELSE.
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
    // Assign: true if `name` is not a real variable but the enclosing
    // FUNCTION's own name, used as its return-value pseudo-assignment
    // (`FuncName = value` inside FUNCTION FuncName ... END FUNCTION).
    // Set by the parser, which already tracks the enclosing function.
    bool isReturnAssign = false;
};

struct Module {
    std::vector<StmtPtr> stmts;
};

} // namespace ebasic
