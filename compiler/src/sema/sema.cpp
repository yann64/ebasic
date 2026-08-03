#include "sema/sema.hpp"

#include <algorithm>
#include <cctype>

namespace ebasic {

namespace {
/// SELECT CASE's own, looser compatibility rule for a CASE value against the
/// selector's type: any two numeric types are comparable (unlike
/// isAssignCompatible's stricter assignment rule), and STRING only matches
/// STRING.
bool isCaseCompatible(TypeKind a, TypeKind b) {
    return (isNumericType(a) && isNumericType(b)) || (a == TypeKind::StringT && b == TypeKind::StringT);
}

/// Strict pointee-type identity (NOT the looser numeric-widening rule that
/// isAssignCompatible allows between plain variables - C++ pointers have no
/// implicit float*<->int* conversion, so `Integer PTR = Single PTR` must be
/// rejected, not silently accepted the way `DIM x AS INTEGER: x = 3.5` is).
/// Either side being ANY PTR (null pointee) is universally compatible.
/// Recurses for PTR PTR.
bool pointeesIdentical(const Type& a, const Type& b) {
    if (a.kind != b.kind) return false;
    if (a.kind == TypeKind::UserDefined) return canonicalName(a.typeName) == canonicalName(b.typeName);
    if (a.kind == TypeKind::Pointer) {
        if (!a.pointee || !b.pointee) return true;
        return pointeesIdentical(*a.pointee, *b.pointee);
    }
    return true;
}

/// After a successful isAssignCompatible(target, value.type) check
/// involving a pointer, stash `target` on `value` when the bridge that made
/// them compatible was `value` being a bare ANY PTR - see Expr::
/// pointerCastTo's own doc comment for why Codegen needs this (C++ has no
/// implicit void* -> T* conversion, unlike this language's own ANY PTR
/// bridging rule). Safe to call unconditionally right after any
/// isAssignCompatible check regardless of its result - a no-op when neither
/// operand needs it, and harmless dead data when the check failed, since
/// Codegen never runs on a program with reported Sema errors.
void annotatePointerBridge(const Type& target, Expr& value) {
    if (target.kind == TypeKind::Pointer && value.type.kind == TypeKind::Pointer &&
        !value.type.pointee) {
        value.pointerCastTo = std::make_shared<Type>(target);
    } else if (target.kind == TypeKind::ZStringT && value.type.kind == TypeKind::Pointer &&
               !value.type.pointee) {
        /// The ANY-PTR-value-as-ZSTRING bridge (see isAssignCompatible's own
        /// doc comment) - `static_cast<const char*>(...)` is exactly as
        /// valid for a void* source as the typed-PTR case above (`cppType`
        /// renders ZStringT as `const char*`), so this reuses the exact same
        /// `pointerCastTo`/`genExpr` cast-insertion machinery with no
        /// Codegen changes at all.
        value.pointerCastTo = std::make_shared<Type>(target);
    }
}


/// Recursively checks whether `type` is, or (for a `UserDefined` field)
/// transitively contains, a `STRING`. Used to enforce FreeBASIC's UNION
/// restriction ("cannot contain variable-length strings ... or fields with
/// constructors or destructors") - `STRING` is the only non-trivial type in
/// this language slice, so this check alone covers that whole rule. A
/// pointer's pointee doesn't matter here: a pointer is itself always a
/// trivial value regardless of what it points to. `visited` guards against
/// infinite recursion through a cyclic embedding (already invalid C++
/// regardless, per genTypeDecl's own cycle guard - just don't hang here).
bool typeContainsString(const Type& type, const std::unordered_map<std::string, RecordInfo>& structs,
                         std::unordered_set<std::string>& visited) {
    if (type.kind == TypeKind::StringT) return true;
    if (type.kind != TypeKind::UserDefined) return false;
    std::string key = canonicalName(type.typeName);
    if (!visited.insert(key).second) return false;
    auto it = structs.find(key);
    if (it == structs.end()) return false; // unknown type; already reported elsewhere
    for (const FieldDecl& field : it->second.fields) {
        if (typeContainsString(field.type, structs, visited)) return true;
    }
    return false;
}
} // namespace

void Sema::check(Module& module) {
    collectLabels(module.stmts);
    collectProcedures(module.stmts);
    collectTypes(module.stmts);
    collectOperators(module.stmts);
    collectExternSignatureChecks(module.stmts);
    collectGosubUsage(module.stmts);
    checkBlock(module.stmts, /*atTopLevel=*/true);
}

void Sema::collectTypes(std::vector<StmtPtr>& stmts) {
    auto isRecordDecl = [](StmtKind k) { return k == StmtKind::TypeDecl || k == StmtKind::UnionDecl; };

    /// Pass 1: register every TYPE/UNION's name first, so a field can
    /// reference one declared later in the file (mirrors
    /// collectProcedures/labels).
    for (auto& stmt : stmts) {
        if (!isRecordDecl(stmt->kind)) continue;
        std::string key = canonicalName(stmt->name);
        if (procedures_.count(key) || structs_.count(key)) {
            diags_.error(stmt->loc, "'" + stmt->name + "' is already declared");
            continue;
        }
        structs_[key] = RecordInfo{};
    }
    /// Pass 2: now that every name is known, resolve each one's fields.
    for (auto& stmt : stmts) {
        if (!isRecordDecl(stmt->kind)) continue;
        std::string key = canonicalName(stmt->name);
        auto it = structs_.find(key);
        if (it == structs_.end()) continue; // duplicate; already reported above

        RecordInfo info;
        info.declLoc = stmt->loc;
        for (const FieldDecl& field : stmt->fields) {
            std::string fieldKey = canonicalName(field.name);
            bool duplicate = false;
            for (const FieldDecl& existing : info.fields) {
                if (canonicalName(existing.name) == fieldKey) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                diags_.error(field.loc, "duplicate field name '" + field.name + "'");
                continue;
            }
            if (field.type.kind == TypeKind::UserDefined &&
                !structs_.count(canonicalName(field.type.typeName))) {
                diags_.error(field.loc,
                             "unknown TYPE '" + field.type.typeName + "' for field '" + field.name + "'");
            }
            info.fields.push_back(field);
        }

        /// EXTENDS (single inheritance only) - UNION cannot use it (kept
        /// simple: real FB allows a restricted form, but combining UNION's
        /// own "no complex members" restriction with inheritance isn't
        /// needed to prove TYPE inheritance/virtual dispatch, this slice's
        /// actual goal).
        if (!stmt->baseTypeName.empty()) {
            if (stmt->kind == StmtKind::UnionDecl) {
                diags_.error(stmt->loc, "UNION cannot use EXTENDS");
            } else {
                std::string baseKey = canonicalName(stmt->baseTypeName);
                if (!structs_.count(baseKey)) {
                    diags_.error(stmt->loc,
                                 "unknown base TYPE '" + stmt->baseTypeName + "' in EXTENDS");
                } else {
                    info.baseName = baseKey;
                }
            }
        }

        /// Method/constructor/destructor prototypes (Declare ... inside the
        /// TYPE body) - UNION cannot have any (FreeBASIC unions may not
        /// contain members with constructors/destructors).
        if (stmt->kind == StmtKind::UnionDecl && !stmt->methods.empty()) {
            diags_.error(stmt->methods.front()->loc,
                         "UNION cannot have member procedures (constructors, destructors, or "
                         "methods)");
        } else {
            /// Per-TYPE scratch data, just for this loop: which properties
            /// have already seen a getter/setter declaration, so a genuine
            /// getter+setter pair can be told apart from a duplicate
            /// getter/getter or setter/setter, and a type mismatch between
            /// the two halves can be caught.
            std::unordered_map<std::string, bool> propGetterSeen, propSetterSeen;
            for (const StmtPtr& method : stmt->methods) {
                if (method->isCtor) {
                    if (info.hasCtor) {
                        diags_.error(method->loc, "TYPE '" + stmt->name + "' already has a "
                                                   "declared constructor (only one is supported)");
                    }
                    info.hasCtor = true;
                    continue;
                }
                if (method->isDtor) {
                    if (info.hasDtor) {
                        diags_.error(method->loc, "TYPE '" + stmt->name + "' already has a "
                                                   "declared destructor (only one is allowed)");
                    }
                    info.hasDtor = true;
                    continue;
                }
                if (method->isProperty) {
                    std::string propKey = canonicalName(method->name);
                    bool isGetter = method->kind == StmtKind::FunctionDecl;
                    bool collides = info.methods.count(propKey);
                    for (const FieldDecl& field : info.fields) {
                        if (canonicalName(field.name) == propKey) collides = true;
                    }
                    if (collides) {
                        diags_.error(method->loc, "'" + method->name + "' is already declared as "
                                                   "a member of TYPE '" + stmt->name + "'");
                        continue;
                    }
                    bool& seen = isGetter ? propGetterSeen[propKey] : propSetterSeen[propKey];
                    if (seen) {
                        diags_.error(method->loc, "PROPERTY '" + method->name + "' already has a "
                                                   "declared " + std::string(isGetter ? "getter" : "setter"));
                        continue;
                    }
                    seen = true;
                    Type thisType = isGetter ? method->declaredType
                                              : (method->params.empty() ? Type(TypeKind::Unknown)
                                                                         : method->params[0].type);
                    auto propIt = info.properties.find(propKey);
                    if (propIt == info.properties.end()) {
                        info.properties[propKey] = PropertyInfo{thisType, method->loc};
                    } else if (!(propIt->second.type.kind == thisType.kind &&
                                 canonicalName(propIt->second.type.typeName) ==
                                     canonicalName(thisType.typeName))) {
                        diags_.error(method->loc, "PROPERTY '" + method->name +
                                                       "'s getter and setter must be the same type");
                    }
                    continue;
                }
                std::string methodKey = canonicalName(method->name);
                bool collides = info.methods.count(methodKey);
                for (const FieldDecl& field : info.fields) {
                    if (canonicalName(field.name) == methodKey) collides = true;
                }
                if (collides) {
                    diags_.error(method->loc, "'" + method->name + "' is already declared as a "
                                               "member of TYPE '" + stmt->name + "'");
                    continue;
                }
                ProcedureInfo procInfo;
                procInfo.isFunction = method->kind == StmtKind::FunctionDecl;
                procInfo.returnType = method->declaredType;
                procInfo.params = method->params;
                /// An override necessarily participates in the vtable too,
                /// whether or not `Virtual` was also explicitly written.
                procInfo.isVirtual = method->isVirtual || method->isOverride;
                procInfo.declLoc = method->loc;
                info.methods[methodKey] = std::move(procInfo);
            }
            /// Every declared property must have BOTH a getter and a setter
            /// declared (this version's deliberate simplification - see
            /// Stmt::isProperty's comment).
            for (const auto& [propKey, propInfo] : info.properties) {
                (void)propInfo;
                if (!propGetterSeen.count(propKey)) {
                    diags_.error(stmt->loc, "PROPERTY '" + propKey + "' on TYPE '" + stmt->name +
                                                 "' has a setter but no getter declared");
                }
                if (!propSetterSeen.count(propKey)) {
                    diags_.error(stmt->loc, "PROPERTY '" + propKey + "' on TYPE '" + stmt->name +
                                                 "' has a getter but no setter declared");
                }
            }
        }
        /// M4d: a TYPE with no fields, no methods/properties, no ctor/dtor,
        /// and no EXTENDS is an opaque external handle - computed here since
        /// every field/method/EXTENDS check above has already run for this
        /// TYPE. Deliberately excludes UnionDecl (an empty UNION is a
        /// degenerate case, not this feature's target).
        info.isOpaque = stmt->kind == StmtKind::TypeDecl && info.fields.empty() && info.methods.empty() &&
                         info.properties.empty() && !info.hasCtor && !info.hasDtor && info.baseName.empty();
        it->second = std::move(info);
    }
    /// Pass 3: M4d opaque-TYPE restrictions that need every TYPE/UNION's
    /// `isOpaque` fully resolved first (a forward-referenced opaque TYPE
    /// wouldn't have its flag set yet if checked during pass 2 above - same
    /// forward-reference concern as pass 4's UNION/STRING check below).
    /// Opaque types are PTR-only: reject by-value embedding as a field in
    /// another TYPE/UNION, and reject EXTENDS naming an opaque type as the
    /// base (unknown layout - nothing to inherit). Extending an opaque type
    /// is separately rejected because that assignment (`stmt->baseTypeName`)
    /// already forces `info.baseName` non-empty, which makes `isOpaque` false
    /// for the derived TYPE itself - so there's nothing further to check on
    /// that side.
    for (auto& stmt : stmts) {
        if (!isRecordDecl(stmt->kind)) continue;
        for (const FieldDecl& field : stmt->fields) {
            if (field.type.kind != TypeKind::UserDefined) continue;
            auto it = structs_.find(canonicalName(field.type.typeName));
            if (it != structs_.end() && it->second.isOpaque) {
                diags_.error(field.loc, "field '" + field.name + "' cannot embed opaque external "
                                         "TYPE '" + field.type.typeName + "' by value (unknown "
                                         "layout - only legal via PTR)");
            }
        }
        if (stmt->kind == StmtKind::TypeDecl && !stmt->baseTypeName.empty()) {
            auto it = structs_.find(canonicalName(stmt->baseTypeName));
            if (it != structs_.end() && it->second.isOpaque) {
                diags_.error(stmt->loc, "TYPE '" + stmt->name + "' cannot EXTENDS opaque external "
                                         "TYPE '" + stmt->baseTypeName + "' (unknown layout)");
            }
        }
    }
    /// Pass 4: UNION-only "no STRING, directly or nested" restriction. Run
    /// only after every TYPE/UNION's fields are fully resolved above (pass
    /// 2), since a UNION declared earlier in the file may embed a TYPE
    /// declared later - checking against a not-yet-populated RecordInfo
    /// would miss a nested STRING.
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::UnionDecl) continue;
        for (const FieldDecl& field : stmt->fields) {
            std::unordered_set<std::string> visited;
            if (typeContainsString(field.type, structs_, visited)) {
                diags_.error(field.loc, "UNION member '" + field.name + "' cannot be or contain a "
                                         "STRING (FreeBASIC unions may not contain variable-length "
                                         "strings)");
            }
        }
    }
    /// Pass 5: match each out-of-line method/constructor/destructor
    /// DEFINITION (a top-level SubDecl/FunctionDecl with `ownerType` set) to
    /// its declared prototype, marking it defined. A definition naming an
    /// unknown TYPE, an undeclared method, or a duplicate definition, is an
    /// error here - matches real FreeBASIC's "declared within, defined
    /// outside" split, just checked eagerly instead of surfacing as a
    /// backend link error.
    for (auto& stmt : stmts) {
        if (stmt->ownerType.empty()) continue;
        std::string typeKey = canonicalName(stmt->ownerType);
        auto it = structs_.find(typeKey);
        if (it == structs_.end()) {
            diags_.error(stmt->loc, "'" + stmt->ownerType + "' is not a declared TYPE");
            continue;
        }
        RecordInfo& info = it->second;
        if (stmt->isCtor) {
            if (!info.hasCtor) {
                diags_.error(stmt->loc, "TYPE '" + stmt->ownerType + "' has no declared constructor "
                                         "(add 'Declare Constructor()' inside the TYPE body)");
            } else if (info.ctorDefined) {
                diags_.error(stmt->loc,
                             "constructor for TYPE '" + stmt->ownerType + "' is already defined");
            } else {
                info.ctorDefined = true;
            }
            continue;
        }
        if (stmt->isDtor) {
            if (!info.hasDtor) {
                diags_.error(stmt->loc, "TYPE '" + stmt->ownerType + "' has no declared destructor "
                                         "(add 'Declare Destructor()' inside the TYPE body)");
            } else if (info.dtorDefined) {
                diags_.error(stmt->loc,
                             "destructor for TYPE '" + stmt->ownerType + "' is already defined");
            } else {
                info.dtorDefined = true;
            }
            continue;
        }
        if (stmt->isProperty) {
            std::string propKey = canonicalName(stmt->name);
            bool isGetter = stmt->kind == StmtKind::FunctionDecl;
            if (!info.properties.count(propKey)) {
                diags_.error(stmt->loc, "'" + stmt->ownerType + "." + stmt->name + "' has no "
                                         "matching 'Declare Property' inside TYPE '" +
                                             stmt->ownerType + "'");
                continue;
            }
            auto& definedSet = isGetter ? info.definedGetters : info.definedSetters;
            if (!definedSet.insert(propKey).second) {
                diags_.error(stmt->loc, "PROPERTY '" + stmt->ownerType + "." + stmt->name + "'s " +
                                             (isGetter ? "getter" : "setter") + " is already defined");
            }
            continue;
        }
        std::string methodKey = canonicalName(stmt->name);
        auto methodIt = info.methods.find(methodKey);
        if (methodIt == info.methods.end()) {
            diags_.error(stmt->loc, "'" + stmt->ownerType + "." + stmt->name + "' has no matching "
                                     "'Declare Sub/Function' inside TYPE '" + stmt->ownerType + "'");
            continue;
        }
        if (!info.definedMethods.insert(methodKey).second) {
            diags_.error(stmt->loc,
                         "method '" + stmt->ownerType + "." + stmt->name + "' is already defined");
            continue;
        }
        if (methodIt->second.params.size() != stmt->params.size()) {
            diags_.error(stmt->loc, "definition of '" + stmt->ownerType + "." + stmt->name +
                                         "' has a different parameter count than its declaration");
        }
    }
    /// Pass 6: every declared method/constructor/destructor must have a
    /// matching out-of-line definition - an undefined one would otherwise
    /// only surface as a confusing backend "incomplete type"/link error.
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::TypeDecl) continue;
        auto it = structs_.find(canonicalName(stmt->name));
        if (it == structs_.end()) continue;
        RecordInfo& info = it->second;
        if (info.hasCtor && !info.ctorDefined) {
            diags_.error(stmt->loc,
                         "TYPE '" + stmt->name + "' declares a constructor but never defines it");
        }
        if (info.hasDtor && !info.dtorDefined) {
            diags_.error(stmt->loc,
                         "TYPE '" + stmt->name + "' declares a destructor but never defines it");
        }
        for (const auto& [methodName, procInfo] : info.methods) {
            (void)procInfo;
            if (!info.definedMethods.count(methodName)) {
                diags_.error(stmt->loc, "TYPE '" + stmt->name + "' declares method '" + methodName +
                                             "' but never defines it");
            }
        }
        for (const auto& [propName, propInfo] : info.properties) {
            (void)propInfo;
            if (!info.definedGetters.count(propName)) {
                diags_.error(stmt->loc, "TYPE '" + stmt->name + "' declares PROPERTY '" + propName +
                                             "' but never defines its getter");
            }
            if (!info.definedSetters.count(propName)) {
                diags_.error(stmt->loc, "TYPE '" + stmt->name + "' declares PROPERTY '" + propName +
                                             "' but never defines its setter");
            }
        }
    }
    /// Pass 7: EXTENDS cycle check. Every base-name lookup above only
    /// required the base to *exist*, not that its own chain be acyclic -
    /// walk each TYPE's chain now that every baseName is resolved, so a
    /// cycle (A extends B, B extends A, ...) is reported once cleanly
    /// instead of surfacing later as unbounded recursion in codegen/lookup
    /// helpers (which are cycle-guarded defensively, but a clear diagnostic
    /// here is much friendlier than silent truncation there).
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::TypeDecl) continue;
        std::string key = canonicalName(stmt->name);
        auto it = structs_.find(key);
        if (it == structs_.end() || it->second.baseName.empty()) continue;
        std::unordered_set<std::string> visited{key};
        std::string cur = it->second.baseName;
        bool cyclic = false;
        while (!cur.empty()) {
            if (!visited.insert(cur).second) {
                cyclic = true;
                break;
            }
            auto curIt = structs_.find(cur);
            if (curIt == structs_.end()) break;
            cur = curIt->second.baseName;
        }
        if (cyclic) {
            diags_.error(stmt->loc, "circular inheritance involving TYPE '" + stmt->name + "'");
        }
    }
    /// Pass 8: an Override method must match a Virtual method somewhere up
    /// the (now fully-resolved and acyclic) base chain - run only after
    /// pass 6 confirms no cycles, so this walk is guaranteed to terminate
    /// without relying on its own cycle guard. A narrower "is it actually
    /// virtual" mismatch is left for the backend: Codegen emits a literal
    /// `override` regardless, and g++ already gives a precise error for
    /// that case ("marked override, but does not override").
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::TypeDecl) continue;
        std::string key = canonicalName(stmt->name);
        auto it = structs_.find(key);
        if (it == structs_.end()) continue;
        for (const StmtPtr& method : stmt->methods) {
            if (!method->isOverride || it->second.baseName.empty()) {
                if (method->isOverride && it->second.baseName.empty()) {
                    diags_.error(method->loc, "'" + method->name + "' is marked Override but TYPE '" +
                                                   stmt->name + "' has no base TYPE (no EXTENDS)");
                }
                continue;
            }
            if (!findMethodInChain(it->second.baseName, canonicalName(method->name))) {
                diags_.error(method->loc, "no method '" + method->name + "' found in a base TYPE "
                                           "of '" + stmt->name + "' to override");
            }
        }
    }
}

std::string Sema::operatorKey(BinOp op, const Type& lhs, const Type& rhs) const {
    auto typeKey = [](const Type& t) -> std::string {
        if (t.kind == TypeKind::UserDefined) return "U:" + canonicalName(t.typeName);
        return "K:" + std::to_string(static_cast<int>(t.kind));
    };
    return std::to_string(static_cast<int>(op)) + "|" + typeKey(lhs) + "|" + typeKey(rhs);
}

void Sema::collectOperators(std::vector<StmtPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::FunctionDecl || !stmt->isOperator) continue;
        if (stmt->params.size() != 2) continue; // already reported by the parser

        const Type& lhsType = stmt->params[0].type;
        const Type& rhsType = stmt->params[1].type;
        bool lhsIsUser = lhsType.kind == TypeKind::UserDefined;
        bool rhsIsUser = rhsType.kind == TypeKind::UserDefined;
        if (!lhsIsUser && !rhsIsUser) {
            diags_.error(stmt->loc, "at least one operand of an 'Operator " + stmt->name +
                                         "' overload must be a user-defined TYPE");
            continue;
        }
        if (lhsIsUser && !structs_.count(canonicalName(lhsType.typeName))) {
            diags_.error(stmt->loc, "unknown TYPE '" + lhsType.typeName + "'");
            continue;
        }
        if (rhsIsUser && !structs_.count(canonicalName(rhsType.typeName))) {
            diags_.error(stmt->loc, "unknown TYPE '" + rhsType.typeName + "'");
            continue;
        }

        std::string key = operatorKey(stmt->operatorBinOp, lhsType, rhsType);
        if (!operatorOverloads_.emplace(key, stmt->declaredType).second) {
            diags_.error(stmt->loc, "operator '" + stmt->name + "' is already overloaded for "
                                     "these operand types");
        }
    }
}

void Sema::collectExternSignatureChecks(std::vector<StmtPtr>& stmts) {
    auto checkType = [&](const Type& type, SourceLoc loc, const std::string& what) {
        if (type.kind == TypeKind::StringT) {
            diags_.error(loc, what + " cannot be STRING in an EXTERN/DECLARE signature - use "
                               "ZSTRING (or ZSTRING PTR) for a C-compatible string");
            return;
        }
        if (type.kind != TypeKind::UserDefined) return;
        auto it = structs_.find(canonicalName(type.typeName));
        if (it == structs_.end()) return; // unknown type; already reported elsewhere
        bool hasVirtualMethod = false;
        for (const auto& [methodName, proc] : it->second.methods) {
            (void)methodName;
            if (proc.isVirtual) {
                hasVirtualMethod = true;
                break;
            }
        }
        if (it->second.hasCtor || it->second.hasDtor || hasVirtualMethod) {
            diags_.error(loc, what + " TYPE '" + type.typeName + "' has a constructor, "
                               "destructor, or virtual method and is not C-ABI-compatible - only "
                               "plain, standard-layout TYPEs (or an opaque TYPE used via PTR) can "
                               "cross an EXTERN boundary");
        }
    };

    for (auto& stmt : stmts) {
        if (!stmt->isExtern) continue;
        for (const Param& p : stmt->params) {
            checkType(p.type, p.loc, "parameter '" + p.name + "'");
        }
        if (stmt->kind == StmtKind::FunctionDecl) {
            checkType(stmt->declaredType, stmt->loc, "the return type");
        }
    }
}

void Sema::collectLabels(std::vector<StmtPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::Label) continue;
        std::string key = canonicalName(stmt->name);
        if (!labels_.insert(key).second) {
            diags_.error(stmt->loc, "label '" + stmt->name + "' is already declared");
        }
    }
}

void Sema::collectGosubUsage(std::vector<StmtPtr>& stmts) {
    for (auto& stmt : stmts) {
        if (stmt->kind == StmtKind::GoSub) {
            gosubTargets_.insert(canonicalName(stmt->name));
        } else if (stmt->kind == StmtKind::Goto) {
            gotoTargets_.insert(canonicalName(stmt->name));
        }
    }
    for (auto& stmt : stmts) {
        if (stmt->kind != StmtKind::Label) continue;
        std::string key = canonicalName(stmt->name);
        if (gosubTargets_.count(key) && gotoTargets_.count(key)) {
            diags_.error(stmt->loc, "label '" + stmt->name + "' is used as both a GOTO target and a "
                                     "GOSUB target, which is not supported");
        }
    }
}

void Sema::collectProcedures(std::vector<StmtPtr>& stmts, const std::string& prefix) {
    for (auto& stmt : stmts) {
        if (stmt->kind == StmtKind::NamespaceDecl) {
            std::string nsName = canonicalName(stmt->name);
            namespaces_.insert(nsName); // namespaces can be reopened; no collision check
            std::string nsPrefix = prefix.empty() ? nsName : prefix + "::" + nsName;
            collectProcedures(stmt->body, nsPrefix);
            continue;
        }
        if (stmt->kind != StmtKind::SubDecl && stmt->kind != StmtKind::FunctionDecl) continue;
        /// A TYPE method/constructor/destructor definition - registered
        /// into its own RecordInfo::methods by collectTypes, not the global
        /// procedures_ map (two different TYPEs' same-named methods, or a
        /// method sharing a name with an unrelated free function, must not
        /// collide here).
        if (!stmt->ownerType.empty()) continue;
        /// An operator overload - registered into operatorOverloads_ by
        /// collectOperators, keyed by (BinOp, lhs type, rhs type), never by
        /// name (its `name` is just the symbol text, for diagnostics only).
        if (stmt->isOperator) continue;
        std::string key = prefix.empty() ? canonicalName(stmt->name) : prefix + "::" + canonicalName(stmt->name);
        if (symbols_.count(key) || procedures_.count(key)) {
            diags_.error(stmt->loc, "'" + stmt->name + "' is already declared");
            continue;
        }
        ProcedureInfo info;
        info.isFunction = stmt->kind == StmtKind::FunctionDecl;
        info.returnType = stmt->declaredType;
        info.params = stmt->params;
        info.isExtern = stmt->isExtern;
        info.declLoc = stmt->loc;
        procedures_[key] = std::move(info);
    }
}

std::string Sema::qualifiedKey(const std::string& name) const {
    std::string key = canonicalName(name);
    return currentNamespacePrefix_.empty() ? key : currentNamespacePrefix_ + "::" + key;
}

bool Sema::lookupSymbol(const std::string& key, SymbolInfo& out) const {
    auto lit = locals_.find(key);
    if (lit != locals_.end()) {
        out = lit->second;
        return true;
    }
    if (!currentNamespacePrefix_.empty()) {
        auto qit = symbols_.find(currentNamespacePrefix_ + "::" + key);
        if (qit != symbols_.end()) {
            out = qit->second;
            return true;
        }
    }
    auto git = symbols_.find(key);
    if (git != symbols_.end()) {
        out = git->second;
        return true;
    }
    return false;
}

const ProcedureInfo* Sema::findProcedure(const std::string& key) const {
    if (!currentNamespacePrefix_.empty()) {
        auto qit = procedures_.find(currentNamespacePrefix_ + "::" + key);
        if (qit != procedures_.end()) return &qit->second;
    }
    auto it = procedures_.find(key);
    return it != procedures_.end() ? &it->second : nullptr;
}

bool Sema::isSameOrDerivedFrom(const std::string& typeKey, const std::string& baseKey) const {
    std::string key = typeKey;
    std::unordered_set<std::string> visited;
    while (!key.empty() && visited.insert(key).second) {
        if (key == baseKey) return true;
        auto it = structs_.find(key);
        if (it == structs_.end()) return false;
        key = it->second.baseName;
    }
    return false;
}

const FieldDecl* Sema::findFieldInChain(const std::string& typeKey, const std::string& fieldKey) const {
    std::string key = typeKey;
    std::unordered_set<std::string> visited;
    while (!key.empty() && visited.insert(key).second) {
        auto it = structs_.find(key);
        if (it == structs_.end()) return nullptr;
        for (const FieldDecl& field : it->second.fields) {
            if (canonicalName(field.name) == fieldKey) return &field;
        }
        key = it->second.baseName;
    }
    return nullptr;
}

const ProcedureInfo* Sema::findMethodInChain(const std::string& typeKey, const std::string& methodKey) const {
    std::string key = typeKey;
    std::unordered_set<std::string> visited;
    while (!key.empty() && visited.insert(key).second) {
        auto it = structs_.find(key);
        if (it == structs_.end()) return nullptr;
        auto methodIt = it->second.methods.find(methodKey);
        if (methodIt != it->second.methods.end()) return &methodIt->second;
        key = it->second.baseName;
    }
    return nullptr;
}

const PropertyInfo* Sema::findPropertyInChain(const std::string& typeKey, const std::string& propKey) const {
    std::string key = typeKey;
    std::unordered_set<std::string> visited;
    while (!key.empty() && visited.insert(key).second) {
        auto it = structs_.find(key);
        if (it == structs_.end()) return nullptr;
        auto propIt = it->second.properties.find(propKey);
        if (propIt != it->second.properties.end()) return &propIt->second;
        key = it->second.baseName;
    }
    return nullptr;
}

bool Sema::isAssignCompatible(const Type& targetType, const Type& valueType) const {
    /// Pointer target/value: ANY PTR (null pointee) is universally compatible
    /// with any other pointer on either side (verified against FB docs -
    /// "implicitly converted to and from other pointer types"); two typed
    /// pointers require identical pointees. Assigning an integer-family value
    /// (the null-literal `0` convention) to a pointer target is allowed
    /// structurally here - the backend (g++) rejects any non-zero
    /// non-pointer-constant case, the same "defer to the backend" pattern
    /// used elsewhere in this codebase. Assigning a pointer to a non-pointer
    /// target is never allowed.
    bool targetIsPtr = targetType.kind == TypeKind::Pointer;
    bool valueIsPtr = valueType.kind == TypeKind::Pointer;

    /// A bare ANY PTR value (a real void*, e.g. a malloc'd buffer an
    /// external C function handed back) may also be read as a ZSTRING -
    /// both are raw C-level pointers under the hood, and this is the only
    /// way to safely turn such a value into a real STRING (via ZSTRING's
    /// own existing STRING conversion below) without a dedicated CAST
    /// operator, e.g. so it can be freed correctly afterward instead of
    /// leaking. Deliberately one-directional (ZSTRING -> ANY PTR is not
    /// bridged here) - the reverse needs a const_cast, not a static_cast
    /// (removing const isn't implicit even via static_cast), a genuinely
    /// different codegen shape nothing currently needs; add it only if a
    /// real use case shows up. Also deliberately narrower than the
    /// typed-PTR bridge below: only a bare ANY PTR bridges to ZSTRING, not
    /// any other typed pointer.
    if (targetType.kind == TypeKind::ZStringT && valueIsPtr && !valueType.pointee) {
        return true;
    }

    if (targetIsPtr || valueIsPtr) {
        if (targetIsPtr && valueIsPtr) {
            if (!targetType.pointee || !valueType.pointee) return true;
            return pointeesIdentical(*targetType.pointee, *valueType.pointee);
        }
        return targetIsPtr && isIntegerFamily(valueType.kind);
    }

    /// STRING and ZSTRING are mutually assign-compatible (verified against
    /// FB docs: any string type argument may be passed directly to a
    /// ZSTRING PTR parameter) - the actual marshaling in both directions is
    /// handled entirely by BString's own implicit `const char*` conversion
    /// operator and its `BString(const char*)` constructor in the generated
    /// C++ (M4), so this is purely a type-compatibility rule; Codegen needs
    /// no per-call conversion logic. Neither is compatible with anything
    /// else (numeric, UserDefined, ...).
    bool targetIsStringLike = targetType.kind == TypeKind::StringT || targetType.kind == TypeKind::ZStringT;
    bool valueIsStringLike = valueType.kind == TypeKind::StringT || valueType.kind == TypeKind::ZStringT;
    if (targetIsStringLike || valueIsStringLike) return targetIsStringLike && valueIsStringLike;

    bool targetIsUser = targetType.kind == TypeKind::UserDefined;
    bool valueIsUser = valueType.kind == TypeKind::UserDefined;
    if (targetIsUser != valueIsUser) return false;
    if (targetIsUser) {
        /// An implicit upcast: a value of a TYPE derived (directly or
        /// transitively) from the target's TYPE is compatible, matching
        /// C++'s own base-pointer/reference/slicing-assignment behavior -
        /// needed for e.g. passing a Derived instance to a BYREF Base
        /// parameter to demonstrate virtual dispatch.
        return isSameOrDerivedFrom(canonicalName(valueType.typeName), canonicalName(targetType.typeName));
    }
    return true;
}

bool Sema::isLvalue(const Expr& expr) const {
    switch (expr.kind) {
        case ExprKind::Ident:
            return true; // undeclared names are reported separately by checkExpr
        case ExprKind::Member:
            return true; // fields are always assignable (no const fields yet)
        case ExprKind::Deref:
            return true; // *p is always addressable when p is a pointer
        case ExprKind::Call: {
            /// An array-element read is an lvalue; a function-call result is not.
            if (expr.lhs) return false; // qualified calls are always procedure calls
            SymbolInfo info;
            return lookupSymbol(canonicalName(expr.stringValue), info) && info.isArray;
        }
        default:
            return false;
    }
}

void Sema::checkCallArgs(const ProcedureInfo& proc, std::vector<ExprPtr>& args, SourceLoc loc) {
    if (args.size() != proc.params.size()) {
        diags_.error(loc, "expected " + std::to_string(proc.params.size()) + " argument(s), got " +
                               std::to_string(args.size()));
    }
    size_t n = std::min(args.size(), proc.params.size());
    for (size_t i = 0; i < n; ++i) {
        const Param& param = proc.params[i];
        Expr& arg = *args[i];
        Type argType = checkExpr(arg);
        if (!isAssignCompatible(param.type, argType)) {
            diags_.error(arg.loc, "argument " + std::to_string(i + 1) +
                                       " type does not match parameter '" + param.name + "'");
        }
        annotatePointerBridge(param.type, arg);
        if (param.byRef) {
            bool isConstVar = false;
            if (arg.kind == ExprKind::Ident) {
                SymbolInfo info;
                isConstVar = lookupSymbol(canonicalName(arg.stringValue), info) && info.isConst;
            }
            if (!isLvalue(arg) || isConstVar) {
                diags_.error(arg.loc, "argument " + std::to_string(i + 1) + " passed to BYREF "
                                       "parameter '" + param.name + "' must be an addressable "
                                       "value (a variable, field, or array element)");
            }
        }
    }
    /// Any extra args beyond the parameter count still get type-checked so
    /// their own errors (undeclared names, etc.) aren't silently skipped.
    for (size_t i = n; i < args.size(); ++i) {
        checkExpr(*args[i]);
    }
}

void Sema::checkBlock(std::vector<StmtPtr>& stmts, bool atTopLevel) {
    for (auto& stmt : stmts) {
        checkStmt(*stmt, atTopLevel);
    }
}

void Sema::checkCondition(Expr& expr, const char* what) {
    TypeKind t = checkExpr(expr);
    if (!isNumericType(t)) {
        diags_.error(expr.loc, std::string(what) + " must be a numeric expression");
    }
}

void Sema::checkStmt(Stmt& stmt, bool atTopLevel) {
    switch (stmt.kind) {
        case StmtKind::Dim: {
            std::string key = insideProcedure_ ? canonicalName(stmt.name) : qualifiedKey(stmt.name);
            auto& scope = insideProcedure_ ? locals_ : symbols_;
            if (scope.count(key) || procedures_.count(key) || structs_.count(key)) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is already declared");
                return;
            }
            if (stmt.declaredType.kind == TypeKind::UserDefined) {
                auto typeIt = structs_.find(canonicalName(stmt.declaredType.typeName));
                if (typeIt == structs_.end()) {
                    diags_.error(stmt.loc, "unknown TYPE '" + stmt.declaredType.typeName + "'");
                } else if (typeIt->second.isOpaque) {
                    diags_.error(stmt.loc, "'" + stmt.name + "' cannot be a by-value DIM of opaque "
                                 "external TYPE '" + stmt.declaredType.typeName + "' (unknown "
                                 "layout - declare it as '" + stmt.declaredType.typeName + " PTR' "
                                 "instead)");
                }
            }
            if (stmt.isArray) {
                if (stmt.arrayLower && !isIntegerFamily(checkExpr(*stmt.arrayLower))) {
                    diags_.error(stmt.arrayLower->loc, "array lower bound must be an integer expression");
                }
                if (stmt.arrayUpper && !isIntegerFamily(checkExpr(*stmt.arrayUpper))) {
                    diags_.error(stmt.arrayUpper->loc, "array upper bound must be an integer expression");
                }
            }
            SymbolInfo info;
            info.type = stmt.declaredType;
            info.isConst = false;
            info.isArray = stmt.isArray;
            info.isDynamicArray = stmt.isArray && !stmt.arrayUpper;
            info.declLoc = stmt.loc;
            scope[key] = info;
            return;
        }
        case StmtKind::Redim: {
            std::string key = canonicalName(stmt.name);
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
                return;
            }
            if (!info.isArray) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is not an array");
                return;
            }
            if (!info.isDynamicArray) {
                diags_.error(stmt.loc,
                             "'" + stmt.name + "' is a fixed-size array and cannot be REDIM'd "
                             "(declare it with DIM " + stmt.name + "() to make it dynamic)");
            }
            if (stmt.declaredType.kind != TypeKind::Unknown &&
                !isAssignCompatible(stmt.declaredType, info.type)) {
                diags_.error(stmt.loc, "REDIM type does not match the array's declared type");
            }
            if (stmt.arrayLower && !isIntegerFamily(checkExpr(*stmt.arrayLower))) {
                diags_.error(stmt.arrayLower->loc, "array lower bound must be an integer expression");
            }
            if (!isIntegerFamily(checkExpr(*stmt.arrayUpper))) {
                diags_.error(stmt.arrayUpper->loc, "array upper bound must be an integer expression");
            }
            return;
        }
        case StmtKind::Const: {
            std::string key = insideProcedure_ ? canonicalName(stmt.name) : qualifiedKey(stmt.name);
            auto& scope = insideProcedure_ ? locals_ : symbols_;
            if (scope.count(key) || procedures_.count(key) || structs_.count(key)) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is already declared");
                return;
            }
            Type exprType = checkExpr(*stmt.expr);
            Type constType = (stmt.declaredType.kind != TypeKind::Unknown) ? stmt.declaredType : exprType;
            stmt.declaredType = constType; // resolve for codegen, even when inferred
            if (!isAssignCompatible(constType, exprType)) {
                diags_.error(stmt.loc, "CONST '" + stmt.name + "' initializer type does not match its "
                                       "declared type");
            }
            annotatePointerBridge(constType, *stmt.expr);
            if (!isConstantExpr(*stmt.expr)) {
                diags_.error(stmt.expr->loc,
                             "CONST initializer must be a constant expression (literals and "
                             "other CONST/ENUM names only)");
            }
            SymbolInfo info;
            info.type = constType;
            info.isConst = true;
            info.declLoc = stmt.loc;
            scope[key] = info;
            return;
        }
        case StmtKind::Enum: {
            auto& scope = insideProcedure_ ? locals_ : symbols_;
            long long next = 0;
            for (auto& member : stmt.enumMembers) {
                long long value = next;
                if (member.value) {
                    long long v = 0;
                    if (evalConstInt(*member.value, v)) value = v;
                }
                member.resolvedValue = value;
                std::string key = insideProcedure_ ? canonicalName(member.name) : qualifiedKey(member.name);
                if (scope.count(key) || procedures_.count(key) || structs_.count(key)) {
                    diags_.error(member.loc, "'" + member.name + "' is already declared");
                } else {
                    SymbolInfo info;
                    info.type = Type(TypeKind::Integer);
                    info.isConst = true;
                    info.declLoc = member.loc;
                    scope[key] = info;
                    constIntValues_[key] = value;
                }
                next = value + 1;
            }
            return;
        }
        case StmtKind::Assign: {
            if (stmt.isReturnAssign) {
                Type exprType = checkExpr(*stmt.expr);
                if (!isAssignCompatible(currentFunctionReturnType_, exprType)) {
                    diags_.error(stmt.loc, "return value type does not match FUNCTION '" + stmt.name +
                                               "'s declared return type");
                }
                annotatePointerBridge(currentFunctionReturnType_, *stmt.expr);
                return;
            }
            if (stmt.target) {
                /// A general Member/Call-chain lvalue (obj.field, arr(i).field,
                /// ...) - checkExpr's own resolution (Ident/Call/Member) already
                /// validates the chain structurally; just check the assigned
                /// value's type against it.
                Type targetType = checkExpr(*stmt.target);
                Type exprType = checkExpr(*stmt.expr);
                if (!isAssignCompatible(targetType, exprType)) {
                    diags_.error(stmt.loc, "assigned value's type does not match the target's type");
                }
                annotatePointerBridge(targetType, *stmt.expr);
                return;
            }
            std::string key = canonicalName(stmt.name);
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                /// Implicit `This.field = value` when inside a method, no
                /// array index, and no local/parameter/global matches -
                /// mirrors the read-side Ident fallback in checkExpr. Walks
                /// the base chain too, so assigning an inherited field works.
                if (!stmt.index && !currentClassName_.empty()) {
                    if (const FieldDecl* field = findFieldInChain(currentClassName_, key)) {
                        Type exprType = checkExpr(*stmt.expr);
                        if (!isAssignCompatible(field->type, exprType)) {
                            diags_.error(stmt.loc, "assigned value's type does not match "
                                                        "member '" + stmt.name + "'");
                        }
                        annotatePointerBridge(field->type, *stmt.expr);
                        return;
                    }
                }
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
                return;
            }
            if (info.isConst) {
                diags_.error(stmt.loc, "cannot assign to constant '" + stmt.name + "'");
                return;
            }
            if (stmt.index) {
                if (!info.isArray) {
                    diags_.error(stmt.loc, "'" + stmt.name + "' is not an array");
                }
                if (!isIntegerFamily(checkExpr(*stmt.index))) {
                    diags_.error(stmt.index->loc, "array index must be an integer expression");
                }
            } else if (info.isArray) {
                diags_.error(stmt.loc, "array '" + stmt.name + "' must be indexed, e.g. " + stmt.name +
                                           "(i) = ...");
            }
            Type exprType = checkExpr(*stmt.expr);
            if (!isAssignCompatible(info.type, exprType)) {
                if (info.type.kind == TypeKind::StringT) {
                    diags_.error(stmt.loc, "cannot assign a non-string value to string variable '" +
                                               stmt.name + "'");
                } else if (exprType.kind == TypeKind::StringT) {
                    diags_.error(stmt.loc, "cannot assign a string value to numeric variable '" +
                                               stmt.name + "'");
                } else {
                    diags_.error(stmt.loc, "assigned value's type does not match variable '" +
                                               stmt.name + "'");
                }
            }
            annotatePointerBridge(info.type, *stmt.expr);
            return;
        }
        case StmtKind::Print: {
            for (auto& arg : stmt.args) {
                checkExpr(*arg);
            }
            return;
        }
        case StmtKind::If: {
            for (auto& cond : stmt.conditions) {
                checkCondition(*cond, "IF/ELSEIF condition");
            }
            for (auto& block : stmt.blocks) {
                checkBlock(block, /*atTopLevel=*/false);
            }
            return;
        }
        case StmtKind::SelectCase: {
            TypeKind selectorType = checkExpr(*stmt.expr);
            for (size_t i = 0; i < stmt.cases.size(); ++i) {
                CaseArm& arm = stmt.cases[i];
                if (arm.isElse && i + 1 != stmt.cases.size()) {
                    diags_.error(stmt.loc, "CASE ELSE must be the last CASE in a SELECT CASE");
                }
                for (auto& match : arm.matches) {
                    TypeKind matchType = checkExpr(*match);
                    if (!isCaseCompatible(selectorType, matchType)) {
                        diags_.error(match->loc,
                                     "CASE value is not comparable to the SELECT CASE expression");
                    }
                }
                checkBlock(arm.body, /*atTopLevel=*/false);
            }
            return;
        }
        case StmtKind::ForNext: {
            std::string key = canonicalName(stmt.name);
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                diags_.error(stmt.loc, "variable '" + stmt.name + "' is not declared");
            } else if (info.isConst) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' cannot be a constant");
            } else if (info.isArray) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' cannot be an array");
            } else if (!isNumericType(info.type)) {
                diags_.error(stmt.loc, "FOR loop variable '" + stmt.name + "' must be numeric");
            }
            checkCondition(*stmt.expr, "FOR start value");
            checkCondition(*stmt.forEnd, "FOR end value (TO)");
            if (stmt.forStep) checkCondition(*stmt.forStep, "FOR step value");
            loopStack_.push_back(LoopKind::For);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();
            return;
        }
        case StmtKind::DoLoop: {
            if (stmt.preTest != LoopTest::None) checkCondition(*stmt.preCond, "DO condition");
            loopStack_.push_back(LoopKind::Do);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();
            if (stmt.postTest != LoopTest::None) checkCondition(*stmt.postCond, "LOOP condition");
            return;
        }
        case StmtKind::WhileWend: {
            checkCondition(*stmt.expr, "WHILE condition");
            loopStack_.push_back(LoopKind::While);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();
            return;
        }
        case StmtKind::Goto: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "GOTO is only supported at the top level of a program in this "
                             "version of ebc");
            }
            if (!labels_.count(canonicalName(stmt.name))) {
                diags_.error(stmt.loc, "label '" + stmt.name + "' is not defined");
            }
            return;
        }
        case StmtKind::Label: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "labels are only supported at the top level of a program in this "
                             "version of ebc");
            }
            /// A new label always ends any GOSUB body span in progress, and
            /// starts a new one if this label is itself a GOSUB target.
            insideGosubBody_ = gosubTargets_.count(canonicalName(stmt.name)) != 0;
            return;
        }
        case StmtKind::GoSub: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "GOSUB is only supported at the top level of a program in this "
                             "version of ebc");
            }
            if (!labels_.count(canonicalName(stmt.name))) {
                diags_.error(stmt.loc, "label '" + stmt.name + "' is not defined");
            }
            return;
        }
        case StmtKind::ExitLoop: {
            bool found = false;
            for (auto it = loopStack_.rbegin(); it != loopStack_.rend(); ++it) {
                if (*it == stmt.exitKind) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                const char* kind = stmt.exitKind == LoopKind::For        ? "FOR"
                                   : stmt.exitKind == LoopKind::Do       ? "DO"
                                   : stmt.exitKind == LoopKind::While    ? "WHILE"
                                   : stmt.exitKind == LoopKind::Sub      ? "SUB"
                                                                         : "FUNCTION";
                diags_.error(stmt.loc,
                             std::string("EXIT ") + kind + " used outside of a matching " + kind);
            }
            return;
        }
        case StmtKind::SubDecl:
        case StmtKind::FunctionDecl: {
            if (!atTopLevel) {
                diags_.error(stmt.loc,
                             "SUB/FUNCTION declarations are only supported at the top level of a "
                             "program");
                return;
            }
            bool isFunction = stmt.kind == StmtKind::FunctionDecl;

            std::unordered_map<std::string, SymbolInfo> savedLocals = std::move(locals_);
            bool savedInsideProcedure = insideProcedure_;
            Type savedReturnType = currentFunctionReturnType_;
            std::string savedClassName = currentClassName_;

            locals_.clear();
            insideProcedure_ = true;
            currentFunctionReturnType_ = isFunction ? stmt.declaredType : Type(TypeKind::Unknown);
            /// An out-of-line method/constructor/destructor definition
            /// (`ownerType` set) - not a free SUB/FUNCTION. Methods don't
            /// nest, so a plain save/restore (no stack) suffices, mirroring
            /// insideProcedure_'s own simplification.
            currentClassName_ = stmt.ownerType.empty() ? std::string() : canonicalName(stmt.ownerType);

            for (const Param& param : stmt.params) {
                std::string key = canonicalName(param.name);
                if (locals_.count(key)) {
                    diags_.error(param.loc, "duplicate parameter name '" + param.name + "'");
                    continue;
                }
                SymbolInfo info;
                info.type = param.type;
                info.declLoc = param.loc;
                locals_[key] = info;
            }

            loopStack_.push_back(isFunction ? LoopKind::Function : LoopKind::Sub);
            checkBlock(stmt.body, /*atTopLevel=*/false);
            loopStack_.pop_back();

            locals_ = std::move(savedLocals);
            insideProcedure_ = savedInsideProcedure;
            currentFunctionReturnType_ = savedReturnType;
            currentClassName_ = savedClassName;
            return;
        }
        case StmtKind::CallStmt: {
            if (stmt.target) {
                bool targetIsNamespaceIdent = stmt.target->kind == ExprKind::Ident &&
                                               namespaces_.count(canonicalName(stmt.target->stringValue));
                if (targetIsNamespaceIdent) {
                    /// CALL Namespace.Name(args) - target is the qualifier.
                    std::string nsKey = canonicalName(stmt.target->stringValue);
                    auto it = procedures_.find(nsKey + "::" + canonicalName(stmt.name));
                    if (it == procedures_.end()) {
                        diags_.error(stmt.loc, "namespace '" + stmt.target->stringValue +
                                                    "' has no member '" + stmt.name + "'");
                        for (auto& arg : stmt.args) checkExpr(*arg);
                        return;
                    }
                    checkCallArgs(it->second, stmt.args, stmt.loc);
                    return;
                }
                /// CALL obj.Method(args) / CALL This.Method(args) / CALL
                /// Base.Method(args) - target is a receiver expression,
                /// resolved by what its type is (walking the base chain, so
                /// an inherited method resolves too).
                Type receiverType = checkExpr(*stmt.target);
                if (receiverType.kind != TypeKind::UserDefined) {
                    if (receiverType.kind != TypeKind::Unknown) {
                        diags_.error(stmt.target->loc, "'" + stmt.name + "' is not a namespace or "
                                                            "method - the left-hand side is not a "
                                                            "user-defined TYPE value");
                    }
                    for (auto& arg : stmt.args) checkExpr(*arg);
                    return;
                }
                std::string typeKey = canonicalName(receiverType.typeName);
                if (!structs_.count(typeKey)) {
                    diags_.error(stmt.loc, "unknown TYPE '" + receiverType.typeName + "'");
                    for (auto& arg : stmt.args) checkExpr(*arg);
                    return;
                }
                const ProcedureInfo* method = findMethodInChain(typeKey, canonicalName(stmt.name));
                if (!method) {
                    diags_.error(stmt.loc, "TYPE '" + receiverType.typeName + "' has no method '" +
                                                stmt.name + "'");
                    for (auto& arg : stmt.args) checkExpr(*arg);
                    return;
                }
                checkCallArgs(*method, stmt.args, stmt.loc);
                return;
            }
            const ProcedureInfo* proc = findProcedure(canonicalName(stmt.name));
            if (!proc) {
                diags_.error(stmt.loc, "'" + stmt.name + "' is not a declared SUB or FUNCTION");
                for (auto& arg : stmt.args) checkExpr(*arg);
                return;
            }
            checkCallArgs(*proc, stmt.args, stmt.loc);
            return;
        }
        case StmtKind::Return: {
            bool insideSub = false;
            bool insideFunction = false;
            for (auto it = loopStack_.rbegin(); it != loopStack_.rend(); ++it) {
                if (*it == LoopKind::Sub) { insideSub = true; break; }
                if (*it == LoopKind::Function) { insideFunction = true; break; }
            }
            if (!insideSub && !insideFunction && !insideGosubBody_) {
                diags_.error(stmt.loc, "RETURN used outside of a SUB, FUNCTION, or GOSUB target");
                if (stmt.expr) checkExpr(*stmt.expr);
                return;
            }
            if (insideFunction) {
                if (!stmt.expr) {
                    diags_.error(stmt.loc, "RETURN inside a FUNCTION requires a value");
                    return;
                }
                Type exprType = checkExpr(*stmt.expr);
                if (!isAssignCompatible(currentFunctionReturnType_, exprType)) {
                    diags_.error(stmt.expr->loc,
                                 "RETURN value type does not match the FUNCTION's declared return "
                                 "type");
                }
                annotatePointerBridge(currentFunctionReturnType_, *stmt.expr);
            } else if (stmt.expr) {
                diags_.error(stmt.expr->loc,
                             "a SUB or GOSUB target cannot RETURN a value; use a bare RETURN");
            }
            return;
        }
        case StmtKind::TypeDecl:
        case StmtKind::UnionDecl: {
            /// Name registration, field-type resolution, and (for UNION) the
            /// no-STRING restriction already happened in the collectTypes
            /// pre-pass (fields have no expressions or control flow to
            /// check) - just enforce the top-level rule, matching
            /// SUB/FUNCTION, so a nested TYPE/UNION isn't silently accepted
            /// as a no-op while being unusable (never registered).
            if (!atTopLevel) {
                diags_.error(stmt.loc, "TYPE/UNION declarations are only supported at the top level "
                                        "of a program");
            }
            return;
        }
        case StmtKind::NamespaceDecl: {
            if (!atTopLevel) {
                diags_.error(stmt.loc, "NAMESPACE declarations are only supported at the top level "
                                        "of a program");
                return;
            }
            std::string savedPrefix = currentNamespacePrefix_;
            currentNamespacePrefix_ =
                savedPrefix.empty() ? canonicalName(stmt.name) : savedPrefix + "::" + canonicalName(stmt.name);

            for (auto& member : stmt.body) {
                switch (member->kind) {
                    case StmtKind::Const:
                    case StmtKind::Enum:
                    case StmtKind::Dim:
                    case StmtKind::SubDecl:
                    case StmtKind::FunctionDecl:
                        checkStmt(*member, /*atTopLevel=*/true);
                        break;
                    default:
                        diags_.error(member->loc,
                                     "only CONST, ENUM, DIM, SUB, and FUNCTION are allowed directly "
                                     "inside a NAMESPACE in this version of ebc");
                        break;
                }
            }

            currentNamespacePrefix_ = savedPrefix;
            return;
        }
    }
}

Type Sema::checkExpr(Expr& expr) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
            expr.type = TypeKind::Integer;
            return expr.type;
        case ExprKind::DoubleLiteral:
            expr.type = TypeKind::Double;
            return expr.type;
        case ExprKind::StringLiteral:
            expr.type = TypeKind::StringT;
            return expr.type;
        case ExprKind::BoolLiteral:
            expr.type = TypeKind::Boolean;
            return expr.type;
        case ExprKind::Ident: {
            std::string key = canonicalName(expr.stringValue);
            SymbolInfo info;
            if (!lookupSymbol(key, info)) {
                /// Implicit `This.field` when inside a method and no
                /// local/parameter/global matches - a local/param always
                /// wins (verified FB rule: it "hides" the member). Walks the
                /// base chain too, so an inherited field resolves.
                if (!currentClassName_.empty()) {
                    if (const FieldDecl* field = findFieldInChain(currentClassName_, key)) {
                        expr.type = field->type;
                        return expr.type;
                    }
                }
                diags_.error(expr.loc, "variable '" + expr.stringValue + "' is not declared");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            if (info.isArray) {
                diags_.error(expr.loc, "array '" + expr.stringValue + "' must be indexed, e.g. " +
                                           expr.stringValue + "(i)");
            }
            expr.type = info.type;
            return expr.type;
        }
        case ExprKind::This: {
            if (currentClassName_.empty()) {
                diags_.error(expr.loc, "'This' can only be used inside a TYPE method");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            Type t;
            t.kind = TypeKind::UserDefined;
            t.typeName = currentClassName_;
            expr.type = t;
            return expr.type;
        }
        case ExprKind::Base: {
            if (currentClassName_.empty()) {
                diags_.error(expr.loc, "'Base' can only be used inside a TYPE method");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            auto it = structs_.find(currentClassName_);
            std::string baseName = it != structs_.end() ? it->second.baseName : std::string();
            if (baseName.empty()) {
                diags_.error(expr.loc, "TYPE has no base TYPE (no EXTENDS) - 'Base' cannot be used");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            Type t;
            t.kind = TypeKind::UserDefined;
            t.typeName = baseName;
            expr.type = t;
            return expr.type;
        }
        case ExprKind::Call: {
            if (expr.lhs) {
                bool lhsIsNamespaceIdent = expr.lhs->kind == ExprKind::Ident &&
                                           namespaces_.count(canonicalName(expr.lhs->stringValue));
                if (lhsIsNamespaceIdent) {
                    /// Namespace.Name(args) - lhs is the qualifier.
                    std::string nsKey = canonicalName(expr.lhs->stringValue);
                    auto procIt = procedures_.find(nsKey + "::" + canonicalName(expr.stringValue));
                    if (procIt == procedures_.end()) {
                        diags_.error(expr.loc, "namespace '" + expr.lhs->stringValue +
                                                    "' has no member '" + expr.stringValue + "'");
                        for (auto& arg : expr.args) checkExpr(*arg);
                        expr.type = TypeKind::Unknown;
                        return expr.type;
                    }
                    const ProcedureInfo& proc = procIt->second;
                    if (!proc.isFunction) {
                        diags_.error(expr.loc, "'" + expr.stringValue +
                                                    "' is a SUB and cannot be used in an expression");
                    }
                    checkCallArgs(proc, expr.args, expr.loc);
                    expr.type = proc.returnType;
                    return expr.type;
                }
                /// obj.Method(args) / This.Method(args) / Base.Method(args) -
                /// lhs is a receiver expression, resolved by looking up what
                /// its type actually is, walking the base chain (a third
                /// instance of this codebase's "disambiguate by what it
                /// names" pattern, after array-vs-function Call and
                /// NAMESPACE's qualified lookup).
                Type receiverType = checkExpr(*expr.lhs);
                if (receiverType.kind != TypeKind::UserDefined) {
                    if (receiverType.kind != TypeKind::Unknown) {
                        diags_.error(expr.loc, "'" + expr.stringValue + "' is not a namespace or "
                                                    "method - the left-hand side is not a "
                                                    "user-defined TYPE value");
                    }
                    for (auto& arg : expr.args) checkExpr(*arg);
                    expr.type = TypeKind::Unknown;
                    return expr.type;
                }
                std::string typeKey = canonicalName(receiverType.typeName);
                if (!structs_.count(typeKey)) {
                    diags_.error(expr.loc, "unknown TYPE '" + receiverType.typeName + "'");
                    for (auto& arg : expr.args) checkExpr(*arg);
                    expr.type = TypeKind::Unknown;
                    return expr.type;
                }
                const ProcedureInfo* method = findMethodInChain(typeKey, canonicalName(expr.stringValue));
                if (!method) {
                    diags_.error(expr.loc, "TYPE '" + receiverType.typeName + "' has no method '" +
                                                expr.stringValue + "'");
                    for (auto& arg : expr.args) checkExpr(*arg);
                    expr.type = TypeKind::Unknown;
                    return expr.type;
                }
                if (!method->isFunction) {
                    diags_.error(expr.loc, "'" + expr.stringValue +
                                                "' is a SUB and cannot be used in an expression");
                }
                checkCallArgs(*method, expr.args, expr.loc);
                expr.type = method->returnType;
                return expr.type;
            }
            std::string key = canonicalName(expr.stringValue);
            SymbolInfo info;
            bool isVar = lookupSymbol(key, info);
            if (isVar && info.isArray) {
                if (expr.args.size() != 1) {
                    diags_.error(expr.loc,
                                 "array '" + expr.stringValue + "' takes exactly one index");
                }
                for (auto& arg : expr.args) {
                    if (!isIntegerFamily(checkExpr(*arg))) {
                        diags_.error(arg->loc, "array index must be an integer expression");
                    }
                }
                expr.type = info.type;
                return expr.type;
            }
            const ProcedureInfo* proc = findProcedure(key);
            if (proc) {
                if (!proc->isFunction) {
                    diags_.error(expr.loc, "'" + expr.stringValue +
                                                "' is a SUB and cannot be used in an expression");
                }
                checkCallArgs(*proc, expr.args, expr.loc);
                expr.type = proc->returnType;
                return expr.type;
            }
            if (isVar) {
                diags_.error(expr.loc, "'" + expr.stringValue + "' is not an array or function");
            } else {
                diags_.error(expr.loc, "'" + expr.stringValue + "' is not declared");
            }
            for (auto& arg : expr.args) checkExpr(*arg);
            expr.type = TypeKind::Unknown;
            return expr.type;
        }
        case ExprKind::Member: {
            if (expr.lhs->kind == ExprKind::Ident && namespaces_.count(canonicalName(expr.lhs->stringValue))) {
                std::string nsKey = canonicalName(expr.lhs->stringValue);
                auto it = symbols_.find(nsKey + "::" + canonicalName(expr.stringValue));
                if (it == symbols_.end()) {
                    diags_.error(expr.loc, "namespace '" + expr.lhs->stringValue + "' has no member '" +
                                               expr.stringValue + "'");
                    expr.type = TypeKind::Unknown;
                    return expr.type;
                }
                expr.type = it->second.type;
                return expr.type;
            }
            Type baseType = checkExpr(*expr.lhs);
            if (baseType.kind != TypeKind::UserDefined) {
                if (baseType.kind != TypeKind::Unknown) {
                    diags_.error(expr.loc, "'.' requires a value of a user-defined TYPE");
                }
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            std::string typeKey = canonicalName(baseType.typeName);
            if (!structs_.count(typeKey)) {
                diags_.error(expr.loc, "unknown TYPE '" + baseType.typeName + "'");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            /// Walks the base chain too, so an inherited field resolves the
            /// same as one declared directly.
            if (const FieldDecl* field = findFieldInChain(typeKey, canonicalName(expr.stringValue))) {
                expr.type = field->type;
                return expr.type;
            }
            /// A PROPERTY looks exactly like a field at its access site (no
            /// parens) - resolved the same way, walking the base chain too.
            /// Every declared property is guaranteed to have both a getter
            /// and setter (Sema's own simplifying requirement), so no
            /// separate "does a getter/setter actually exist" check is
            /// needed here regardless of read/write context.
            if (const PropertyInfo* prop = findPropertyInChain(typeKey, canonicalName(expr.stringValue))) {
                expr.type = prop->type;
                expr.isProperty = true;
                return expr.type;
            }
            diags_.error(expr.loc,
                         "TYPE '" + baseType.typeName + "' has no field '" + expr.stringValue + "'");
            expr.type = TypeKind::Unknown;
            return expr.type;
        }
        case ExprKind::AddressOf: {
            /// `@ProcName` - takes a real C function pointer to a
            /// top-level, non-extern SUB/FUNCTION, needed to satisfy a C
            /// callback-style API (e.g. GLib's GCallback for
            /// `g_signal_connect`). Checked before the ordinary lvalue path
            /// since a bare procedure name is never itself a valid Ident
            /// expression (checkExpr(Ident) only knows about
            /// variables/fields, not procedures). Deliberately narrow
            /// scope: produces ANY PTR (matching how such APIs take the
            /// callback as an untyped pointer) rather than a distinct
            /// function-pointer type - see Expr::isProcAddress for the
            /// Codegen side.
            if (expr.lhs->kind == ExprKind::Ident) {
                std::string key = canonicalName(expr.lhs->stringValue);
                if (const ProcedureInfo* proc = findProcedure(key)) {
                    if (proc->isExtern) {
                        diags_.error(expr.loc,
                                     "'@' cannot take the address of EXTERN-declared '" +
                                         expr.lhs->stringValue +
                                         "' (it has no eBasic-compiled body to take the address of)");
                    }
                    bool abiSafe = proc->returnType.kind != TypeKind::StringT;
                    for (const Param& p : proc->params) {
                        if (p.type.kind == TypeKind::StringT) abiSafe = false;
                    }
                    if (!abiSafe) {
                        diags_.error(expr.loc, "'@" + expr.lhs->stringValue +
                                                    "' is not C-ABI-compatible (STRING parameters/"
                                                    "return aren't - use ZSTRING instead)");
                    }
                    expr.isProcAddress = true;
                    Type result;
                    result.kind = TypeKind::Pointer;
                    result.pointee = nullptr; // ANY PTR
                    expr.type = result;
                    return expr.type;
                }
            }
            if (!isLvalue(*expr.lhs)) {
                diags_.error(expr.loc,
                             "'@' requires an addressable value (a variable, field, or array element)");
            }
            Type operandType = checkExpr(*expr.lhs);
            Type result;
            result.kind = TypeKind::Pointer;
            result.pointee = std::make_shared<Type>(operandType);
            expr.type = result;
            return expr.type;
        }
        case ExprKind::Deref: {
            Type operandType = checkExpr(*expr.lhs);
            if (operandType.kind != TypeKind::Pointer) {
                if (operandType.kind != TypeKind::Unknown) {
                    diags_.error(expr.loc, "'*' requires a pointer operand");
                }
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            if (!operandType.pointee) {
                diags_.error(expr.loc, "cannot dereference an ANY PTR (its type is unknown)");
                expr.type = TypeKind::Unknown;
                return expr.type;
            }
            expr.type = *operandType.pointee;
            return expr.type;
        }
        case ExprKind::UnaryNeg: {
            TypeKind t = checkExpr(*expr.lhs);
            if (!isNumericType(t)) {
                diags_.error(expr.loc, "unary '-' requires a numeric operand");
                expr.type = TypeKind::Integer;
                return expr.type;
            }
            expr.type = t;
            return expr.type;
        }
        case ExprKind::UnaryNot: {
            TypeKind t = checkExpr(*expr.lhs);
            if (!isIntegerFamily(t)) {
                diags_.error(expr.loc, "'NOT' requires an integer or boolean operand");
                expr.type = TypeKind::Integer;
                return expr.type;
            }
            expr.type = t;
            return expr.type;
        }
        case ExprKind::Binary: {
            Type lt = checkExpr(*expr.lhs);
            Type rt = checkExpr(*expr.rhs);

            /// A user-defined operand always resolves through the operator
            /// overload table instead of the built-in rules below -
            /// deliberately an exact (BinOp, lhsType, rhsType) match, no
            /// promotion, to keep this first real overload-resolution
            /// mechanism narrow.
            if (lt.kind == TypeKind::UserDefined || rt.kind == TypeKind::UserDefined) {
                auto it = operatorOverloads_.find(operatorKey(expr.binOp, lt, rt));
                if (it == operatorOverloads_.end()) {
                    diags_.error(expr.loc, "no matching 'Operator' overload for these operand types");
                    expr.type = TypeKind::Unknown;
                    return expr.type;
                }
                expr.type = it->second;
                return expr.type;
            }

            switch (expr.binOp) {
                case BinOp::Concat:
                    if (lt != TypeKind::StringT || rt != TypeKind::StringT) {
                        diags_.error(expr.loc, "operator '&' requires string operands");
                    }
                    expr.type = TypeKind::StringT;
                    return expr.type;

                case BinOp::Add:
                    /// Pointer arithmetic (verified against FB docs): p + n
                    /// scales by the pointee's size, matching C++'s own
                    /// pointer arithmetic natively - codegen just reuses the
                    /// plain '+' emission and lets C++ do the scaling.
                    if (lt.kind == TypeKind::Pointer && isIntegerFamily(rt.kind)) {
                        expr.type = lt;
                        return expr.type;
                    }
                    if (rt.kind == TypeKind::Pointer && isIntegerFamily(lt.kind)) {
                        expr.type = rt;
                        return expr.type;
                    }
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "arithmetic operators require numeric operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteNumeric(lt, rt);
                    return expr.type;

                case BinOp::Sub:
                    if (lt.kind == TypeKind::Pointer && rt.kind == TypeKind::Pointer) {
                        /// Pointer difference (verified: legal, result is in
                        /// elements, like C++'s own ptrdiff_t subtraction).
                        bool compatible = !lt.pointee || !rt.pointee ||
                                          pointeesIdentical(*lt.pointee, *rt.pointee);
                        if (!compatible) {
                            diags_.error(expr.loc,
                                         "pointer subtraction requires two pointers of the same type");
                        }
                        expr.type = TypeKind::LongInt;
                        return expr.type;
                    }
                    if (lt.kind == TypeKind::Pointer && isIntegerFamily(rt.kind)) {
                        expr.type = lt;
                        return expr.type;
                    }
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "arithmetic operators require numeric operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteNumeric(lt, rt);
                    return expr.type;

                case BinOp::Mul:
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "arithmetic operators require numeric operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteNumeric(lt, rt);
                    return expr.type;

                case BinOp::Div:
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "arithmetic operators require numeric operands");
                        expr.type = TypeKind::Double;
                        return expr.type;
                    }
                    /// '/' is always real division in FreeBASIC, even between
                    /// two integer operands (use '\' for integer division).
                    expr.type = (!isFloatFamily(lt) && !isFloatFamily(rt))
                                    ? TypeKind::Double
                                    : promoteNumeric(lt, rt);
                    return expr.type;

                case BinOp::Pow:
                    if (!isNumericType(lt) || !isNumericType(rt)) {
                        diags_.error(expr.loc, "'^' requires numeric operands");
                    }
                    /// Exponentiation always yields a real result here; FB's
                    /// integer-power special case is deferred (see roadmap).
                    expr.type = TypeKind::Double;
                    return expr.type;

                case BinOp::IDiv:
                case BinOp::Mod:
                    if (!isIntegerFamily(lt) || !isIntegerFamily(rt)) {
                        diags_.error(expr.loc,
                                     "'\\' and 'MOD' require integer operands "
                                     "(convert floating-point operands explicitly)");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteInteger(lt, rt);
                    return expr.type;

                case BinOp::Shl:
                case BinOp::Shr:
                    if (!isIntegerFamily(lt) || !isIntegerFamily(rt)) {
                        diags_.error(expr.loc, "'SHL' and 'SHR' require integer operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = lt; // shift result takes the shifted value's type
                    return expr.type;

                case BinOp::Eq:
                case BinOp::Ne:
                case BinOp::Lt:
                case BinOp::Le:
                case BinOp::Gt:
                case BinOp::Ge: {
                    bool bothPointers = lt.kind == TypeKind::Pointer && rt.kind == TypeKind::Pointer;
                    /// A pointer compared against a plain integer covers the
                    /// common `p = 0` / `p <> 0` null check.
                    bool pointerVsInt = (lt.kind == TypeKind::Pointer && isIntegerFamily(rt.kind)) ||
                                        (rt.kind == TypeKind::Pointer && isIntegerFamily(lt.kind));
                    if (bothPointers || pointerVsInt) {
                        expr.type = TypeKind::Boolean;
                        return expr.type;
                    }
                    if (!((isNumericType(lt) && isNumericType(rt)) ||
                          (lt == TypeKind::StringT && rt == TypeKind::StringT))) {
                        diags_.error(expr.loc,
                                     "relational operators require two numeric operands or two "
                                     "string operands");
                    }
                    expr.type = TypeKind::Boolean;
                    return expr.type;
                }

                case BinOp::And:
                case BinOp::Or:
                case BinOp::Xor:
                    if (!isIntegerFamily(lt) || !isIntegerFamily(rt)) {
                        diags_.error(expr.loc,
                                     "'AND'/'OR'/'XOR' require integer or boolean operands");
                        expr.type = TypeKind::Integer;
                        return expr.type;
                    }
                    expr.type = promoteInteger(lt, rt);
                    return expr.type;
            }
            expr.type = TypeKind::Unknown;
            return expr.type;
        }
    }
    expr.type = TypeKind::Unknown;
    return expr.type;
}

bool Sema::isConstantExpr(const Expr& expr) const {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
        case ExprKind::DoubleLiteral:
        case ExprKind::StringLiteral:
        case ExprKind::BoolLiteral:
            return true;
        case ExprKind::Ident: {
            SymbolInfo info;
            return lookupSymbol(canonicalName(expr.stringValue), info) && info.isConst;
        }
        case ExprKind::Call:
            return false; // array elements and function calls are never compile-time constants here
        case ExprKind::Member:
            return false; // field access is never a compile-time constant here
        case ExprKind::AddressOf:
        case ExprKind::Deref:
            return false; // pointer ops are never compile-time constants here
        case ExprKind::This:
        case ExprKind::Base:
            return false; // the current instance is never a compile-time constant
        case ExprKind::UnaryNeg:
        case ExprKind::UnaryNot:
            return isConstantExpr(*expr.lhs);
        case ExprKind::Binary:
            return isConstantExpr(*expr.lhs) && isConstantExpr(*expr.rhs);
    }
    return false;
}

bool Sema::evalConstInt(const Expr& expr, long long& outValue) {
    switch (expr.kind) {
        case ExprKind::IntLiteral:
        case ExprKind::BoolLiteral:
            outValue = expr.intValue;
            return true;
        case ExprKind::Ident: {
            auto it = constIntValues_.find(canonicalName(expr.stringValue));
            if (it == constIntValues_.end()) {
                diags_.error(expr.loc,
                             "'" + expr.stringValue + "' is not a constant integer expression");
                return false;
            }
            outValue = it->second;
            return true;
        }
        case ExprKind::UnaryNeg: {
            long long v = 0;
            if (!evalConstInt(*expr.lhs, v)) return false;
            outValue = -v;
            return true;
        }
        case ExprKind::UnaryNot: {
            long long v = 0;
            if (!evalConstInt(*expr.lhs, v)) return false;
            outValue = ~v;
            return true;
        }
        case ExprKind::Binary: {
            long long l = 0, r = 0;
            if (!evalConstInt(*expr.lhs, l) || !evalConstInt(*expr.rhs, r)) return false;
            switch (expr.binOp) {
                case BinOp::Add: outValue = l + r; return true;
                case BinOp::Sub: outValue = l - r; return true;
                case BinOp::Mul: outValue = l * r; return true;
                case BinOp::IDiv:
                case BinOp::Div:
                    if (r == 0) {
                        diags_.error(expr.loc, "division by zero in constant expression");
                        return false;
                    }
                    outValue = l / r;
                    return true;
                case BinOp::Mod:
                    if (r == 0) {
                        diags_.error(expr.loc, "division by zero in constant expression");
                        return false;
                    }
                    outValue = l % r;
                    return true;
                case BinOp::Shl: outValue = l << r; return true;
                case BinOp::Shr: outValue = l >> r; return true;
                case BinOp::And: outValue = l & r; return true;
                case BinOp::Or: outValue = l | r; return true;
                case BinOp::Xor: outValue = l ^ r; return true;
                default:
                    diags_.error(expr.loc, "expression is not a valid constant integer expression");
                    return false;
            }
        }
        default:
            diags_.error(expr.loc, "expression is not a valid constant integer expression");
            return false;
    }
}

} // namespace ebasic
