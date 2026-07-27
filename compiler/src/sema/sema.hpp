#pragma once

#include "ast/ast.hpp"
#include "diagnostics/diagnostics.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ebasic {

// A declared name: its element type (for arrays, the element's type), and
// whether it's an array and/or immutable (CONST/ENUM member). isDynamicArray
// only matters when isArray is true: only an array declared with empty
// parens (DIM arr() AS Type) can later be REDIM'd, matching real FB
// ("REDIM cannot be used on fixed-size arrays").
struct SymbolInfo {
    Type type;
    bool isConst = false;
    bool isArray = false;
    bool isDynamicArray = false;
};

// A SUB/FUNCTION's signature, registered up front (before any body is
// checked) so calls - including recursive and mutually-recursive ones -
// resolve regardless of source order.
struct ProcedureInfo {
    bool isFunction = false;
    Type returnType; // unused for SUB
    std::vector<Param> params;
};

// A declared TYPE's fields, registered up front (collectTypes) so a field
// can reference another TYPE regardless of declaration order.
struct RecordInfo {
    std::vector<FieldDecl> fields;
};

class Sema {
public:
    explicit Sema(DiagnosticEngine& diags) : diags_(diags) {}

    void check(Module& module);

private:
    void collectLabels(std::vector<StmtPtr>& stmts);
    void collectProcedures(std::vector<StmtPtr>& stmts);
    void collectGosubUsage(std::vector<StmtPtr>& stmts);
    // Registers every top-level TYPE's name first (so fields can reference
    // a TYPE declared later in the file), then resolves each one's field
    // types (built-in trivially valid; UserDefined must name an already-
    // registered TYPE) now that every name is known.
    void collectTypes(std::vector<StmtPtr>& stmts);
    void checkBlock(std::vector<StmtPtr>& stmts, bool atTopLevel);
    void checkStmt(Stmt& stmt, bool atTopLevel);
    void checkCondition(Expr& expr, const char* what);
    Type checkExpr(Expr& expr);

    // Looks up `key` in locals_ first, then symbols_, copying the result out
    // (SymbolInfo is small; this sidesteps any pointer-into-map lifetime
    // concerns across subsequent inserts). Returns false if not found.
    bool lookupSymbol(const std::string& key, SymbolInfo& out) const;

    // Shared by both the Call expression (array read / function call) and
    // CallStmt (procedure call as a statement): validates argument count and
    // per-argument type/BYREF-lvalue compatibility against `proc`.
    void checkCallArgs(const ProcedureInfo& proc, std::vector<ExprPtr>& args, SourceLoc loc);

    // Structural constant-expression check for CONST initializers: literals,
    // and Idents that refer to an already-declared CONST/ENUM member,
    // combined with unary/binary operators. Does not compute a value -
    // codegen re-emits the original expression and lets C++ evaluate it.
    bool isConstantExpr(const Expr& expr) const;

    // Small integer-only constant evaluator, used only for ENUM member
    // values (needed for auto-increment). Reports its own diagnostics and
    // returns false on failure.
    bool evalConstInt(const Expr& expr, long long& outValue);

    std::unordered_map<std::string, SymbolInfo> symbols_; // module-level (global) names
    std::unordered_map<std::string, SymbolInfo> locals_;   // current SUB/FUNCTION's params/DIMs
    bool insideProcedure_ = false;
    Type currentFunctionReturnType_; // valid while insideProcedure_ and it's a FUNCTION
    std::unordered_map<std::string, ProcedureInfo> procedures_;
    std::unordered_map<std::string, RecordInfo> structs_; // canonical TYPE name -> its fields
    std::unordered_map<std::string, long long> constIntValues_; // CONST/ENUM int value, for evalConstInt
    std::unordered_set<std::string> labels_;
    std::unordered_set<std::string> gosubTargets_; // labels referenced by at least one GOSUB
    std::unordered_set<std::string> gotoTargets_;  // labels referenced by at least one GOTO
    // True while sequentially walking top-level statements between a
    // GOSUB-target Label and the next Label (of any kind) - lets a bare
    // RETURN there be valid, the same as inside a SUB.
    bool insideGosubBody_ = false;
    std::vector<LoopKind> loopStack_;
    DiagnosticEngine& diags_;
};

} // namespace ebasic
