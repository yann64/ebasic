# eBasic — Compiler Architecture & Roadmap Plan

## Context

The `ebasic` repository is brand new (just `git init`, no commits, no files). Per the project's own `README.md` (hosted at `git.barbel.synology.me/yann64/ebasic`, unreachable from this sandboxed environment due to network policy, so pasted by the user):

> The eBasic compiler is an extended version of the BASIC programming language that uses g++/clang++ as an intermediate compiler.
>
> Features: Open source (MIT) · Same syntax as FreeBASIC, fully reimplemented from scratch · Precompiled standard libs for faster compilation · Allow re-use of C/C++ headers/libs (extends FreeBASIC syntax/keywords) · Module management similar to cargo · Namespaces · Classes · Automated documentation generation.
>
> OS Support: Linux, Windows, macOS, Haiku.

The user (yann64) maintains forks of FreeBASIC (`fbc`), PureBasic, yabasic, Dark Basic Pro, and a Yab-to-C++ transpiler (`Yab2Cpp`), and contributes to Haiku OS — strong signal that a **transpile-to-C++** architecture (BASIC → C++ → native binary via system compiler) is the right approach, consistent with how FreeBASIC itself works and with the user's own prior transpiler project.

This plan defines the initial architecture and a phased build-out roadmap for a from-scratch greenfield project. It intentionally does **not** attempt to spec the entire language in one pass — that would take a huge, unreviewable one-shot implementation. Instead it defines a real, buildable first milestone plus a sequenced roadmap.

**Assumptions made (user did not respond to follow-up questions; proceeding with recommended defaults, to confirm/adjust once implementation starts):**
- Tool naming: compiler binary `ebc`, package manager `ebpm`, runtime library `ebasic-rt` (all easy to rename later).
- C/C++ interop (M4) targets **plain C headers/libs first** (matches FreeBASIC's existing `EXTERN`/`DECLARE` model); arbitrary C++ class/template interop is deferred to a later, separate milestone.
- Dialect scope targets **modern FreeBASIC (`-lang fb`) semantics only** initially; legacy `qb`/`deprecated` dialect compatibility is deferred.

## Architecture

**Pipeline:** `.bas` source → Lexer+Preprocessor → Parser → AST → Semantic Analysis (typed AST + symbol/type tables) → C++ Codegen → invoke `g++`/`clang++` → native binary (linked against `ebasic-rt` runtime + stdlib + any FFI'd C libs).

- **Lexer/Preprocessor**: tokenizes BASIC source (case-insensitive, `:`/newline statement separators, type-suffix sigils `$ % & ! #`, `_` continuation, `'`/`REM` comments); handles `#include`/`#define`/`#ifdef` as a macro-expanding pass over tokens before parsing.
- **Parser**: hand-written recursive descent (better diagnostics than generated parsers) producing a pure, untyped `Ast::Module` (Stmt hierarchy: VarDecl, If, SelectCase, ForNext, DoLoop, SubDecl, FunctionDecl, TypeDecl, NamespaceDecl, etc.; Expr hierarchy: Literal, Ident, Binary, Call, MemberAccess, New, etc.), each node carrying `SourceLoc`.
- **Semantic Analysis**: scoped `SymbolTable`, `TypeTable` (primitives, TYPE/CLASS descriptors, PTR/array/function-signature types), name resolution, type inference/checking, overload resolution, `EXTENDS` inheritance linearization, control-flow validation. Produces a **typed AST** — codegen never re-resolves names/types.
- **Codegen**: lowers typed AST to readable C++ (`.cpp`/`.hpp` per BASIC module) plus a source-location manifest (to remap `g++` errors back to `.bas` lines — needed from early on). Key mappings: `STRING` → runtime `BString` (ref-counted/COW), dynamic arrays → `BArray<T>`, `TYPE`/`CLASS` → C++ struct/class with public inheritance, `NAMESPACE` → C++ `namespace`, `GOTO`/labels → native C++ `goto` (with care around C++'s "no jumping into scope of non-trivial-ctor variable" restriction — needs an explicit codegen strategy before GOTO lands).
- **Backend invocation**: shells out to `g++`/`clang++` (auto-detected, `-cxx` override), links generated code + `ebasic-rt` + stdlib + FFI'd libs, remaps compiler diagnostics via the manifest.
- **Runtime (`ebasic-rt`)**: `BString`, fixed-length string wrapper, `BArray<T>` (bounds-checked in debug builds), `ON ERROR`/`RESUME` support, console I/O, core intrinsics (LEN/MID$/INSTR/etc). Kept distinct from the larger, optional **standard library** (file I/O, extended math/string, later GUI/graphics) — the distinction matters for the precompiled-stdlib milestone (M6).

## Repository Layout

```
ebasic/
  CMakeLists.txt
  cmake/                # toolchain helpers (Haiku, MSVC/MinGW, macOS)
  compiler/src/{lexer,preprocessor,parser,ast,sema,codegen,driver,diagnostics}/
  runtime/{include,src}/ebasic/runtime/     # ebasic-rt
  stdlib/{core,file,...}/
  pkg/                  # ebpm: manifest schema, resolver, build/run/test/new/init
  docgen/               # doc generator tool + templates
  tests/{lexer,parser,sema,codegen,e2e,fixtures}/
  docs/{language-reference,architecture,roadmap.md}
  examples/
  third_party/          # vendored/fetched deps (CLI parser, TOML parser, test framework)
  scripts/              # CI helpers, golden-test runner
  .github/workflows/
  LICENSE (MIT), CONTRIBUTING.md, CHANGELOG.md
```

Compiler/runtime/stdlib/pkg/docgen are separate top-level components (each with its own `CMakeLists.txt`, wired via `add_subdirectory`) since they have different release cadences — runtime/stdlib get precompiled per-target-triple (M6), `ebc`/`ebpm`/`docgen` are independent CLI tools that could split into separate repos later, but stay in-tree for now to keep iteration fast.

## Build System

**CMake** for the compiler's own build. Rationale: working, maintained Haiku support (an explicit OS target where the user is an active contributor), first-class Linux/Windows(MSVC+MinGW)/macOS support, good `FetchContent`/`find_package` story for the few third-party deps needed (CLI parsing, TOML parsing for `ebpm` manifests, a unit-test framework like Catch2/doctest), and the widest contributor familiarity of the realistic options (Meson considered and rejected: weaker Haiku track record, smaller contributor pool). Use modern CMake (≥3.20), `CMakePresets.json` per target (linux-gcc, linux-clang, windows-msvc, windows-mingw, macos, haiku).

## Phased Roadmap

| Milestone | Scope | Done when |
|---|---|---|
| **M0** | Repo/CMake/CI skeleton; minimal lexer+parser+codegen for `DIM`, `PRINT`, literals, assignment, arithmetic; trivial sema; runtime stub | `ebc hello.bas -o hello && ./hello` compiles/runs a real hello-world `.bas`, captured as first e2e golden test |
| **M1** | Full primitive types, all operators w/ FB precedence, `IF/SELECT CASE/FOR/DO/WHILE`, `GOTO`/labels, `CONST`, `ENUM`, static arrays, basic preprocessor | 30–50 golden `.bas` programs covering each construct pass; sema catches basic type/undeclared-var errors |
| **M2** | `SUB`/`FUNCTION`, `BYVAL`/`BYREF`, scoping/recursion, `REDIM`/`REDIM PRESERVE` dynamic arrays, `GOSUB/RETURN`, `EXIT`, multi-file `#include` programs | Golden tests incl. recursive functions, array manipulation, multi-file programs; `BArray` bounds-checking works in debug |
| **M3** | `TYPE`/`CLASS` (fields, `EXTENDS`, ctors/dtors, member subs/funcs, `PROPERTY`, operator overloading), `NAMESPACE`, pointers (`PTR`/`@`), `UNION` | Golden tests incl. inheritance hierarchy, operator-overloaded value type, namespaced code — "same syntax as FreeBASIC" becomes substantially true |
| **M4** | C/C++ interop: `EXTERN`/`DECLARE`-style syntax binding to C headers/libs and C++ free functions/namespaces, linking against `.a`/`.so`/`.lib`. Real C++ class/object interop deferred to a later milestone | Golden tests link a small real C library and a small real C++ library (incl. an opaque handle-style C API and a namespaced/overloaded C++ function) and call them from `.bas` |
| **M5** | `ebpm` package manager: TOML manifest (name/version/deps/targets), path/git deps (registry deferred), lockfile, `build`/`run`/`test`/`new`/`init` | Multi-package sample project (lib + app depending on it) builds/runs via `ebpm build && ebpm run` |
| **M6** | Precompiled standard library: prebuilt static libs/PCH per (compiler, platform, arch, ABI), versioning checks against compiler | Measurable compile-time win on a benchmark project vs. full source rebuild |
| **M7** | Doc-comment syntax (FB has none — eBasic defines its own), `docgen` extracting signatures + doc comments to HTML/Markdown | `docgen` run against the stdlib produces browsable docs |
| **M8** | Windows/macOS/Haiku ports: CI matrix expansion, platform runtime shims, compiler-detection/link-flag differences, Haiku packaging | Full e2e golden suite passes in CI on all four platforms |

M1–M3 are the bulk of "FreeBASIC-compatible" work and will likely split further in practice (e.g. M1a numeric types, M1b control flow) — the table above is the right roadmap granularity, finer breakdown happens once work starts.

## M1a Implementation Notes (types & operators, done)

- Types added: `BYTE`/`UBYTE`/`SHORT`/`USHORT`/`INTEGER`/`LONG`/`UINTEGER`/`LONGINT`/`ULONGINT`/`SINGLE`/`BOOLEAN` alongside the existing `DOUBLE`/`STRING`. `INTEGER` and `LONG` both map to `int32_t` for now; true platform-native `INTEGER` width is still deferred. `BOOLEAN` is represented as `int8_t` with `TRUE`/`FALSE` = `-1`/`0`, so `AND`/`OR`/`XOR`/`NOT` can be implemented as plain C++ bitwise ops that double as logical ops (matches classic BASIC/FB semantics).
- Operators implemented with FreeBASIC's documented precedence: `^` > unary `-` > `*`/`/` > `\` > `MOD` > `SHL`/`SHR` > `+`/`-` > `&` > relational (`=`/`<>`/`<`/`<=`/`>=`/`>`) > `NOT` > `AND` > `OR` > `XOR`. Not yet implemented (deferred, no user-facing need yet): `EQV`/`IMP`/`ANDALSO`/`ORELSE`, and the `CAST`/pointer/array-index/`Is` tiers (need pointers/arrays/OOP first).
- `/` is always real division (promotes to `DOUBLE` even for two integer operands); `\` and `MOD` require integer-family operands (no implicit float truncation yet — pass already-integer values). `^` always yields a `DOUBLE` result; FB's integer-result special case for non-negative integer exponents is deferred.

## M1b Implementation Notes (control flow, done)

- Implemented: block-form `IF`/`ELSEIF`/`ELSE`/`END IF`; `SELECT CASE` with comma-separated value lists and `CASE ELSE` (must be last); `FOR`/`NEXT` with optional `STEP` (direction picked at runtime from the step's sign; `TO` bound and `STEP` are each evaluated once at loop entry, matching BASIC semantics, not re-evaluated per iteration); `DO`/`LOOP` with any combination of pre-test (`WHILE`/`UNTIL` after `DO`) and post-test (after `LOOP`); `WHILE`/`WEND`; `EXIT FOR`/`EXIT DO`/`EXIT WHILE`; `GOTO`/labels.
- Not yet implemented (deferred): single-line `IF cond THEN stmt` shorthand (block `IF`/`END IF` only); `CASE val1 TO val2` and `CASE IS <op> val` range forms; the loop variable in `FOR` must already be `DIM`'d (no implicit declaration).
- `EXIT FOR`/`DO`/`WHILE` target the nearest enclosing loop of that *specific* kind, which may not be the innermost loop (e.g. `EXIT FOR` from inside a nested `DO`). Codegen implements this with a per-loop-kind label placed after each loop and a `goto` to the nearest matching one — plain C++ `break` can't reach past an inner loop of a different kind.
- `GOTO`/labels are restricted to the top level of a program (rejected inside `IF`/`SELECT CASE`/loop bodies) rather than attempting the general case, since jumping into/across nested C++ scopes that contain non-trivially-constructed locals (e.g. a `DIM ... AS STRING`) is exactly the open hazard already flagged above ("`GOTO` into scopes with non-trivial destructors"). Even at the top level, a forward `GOTO` that jumps over a `STRING` `DIM` is still possible to write and will surface as a backend `g++` error rather than being caught by `ebc` itself — a real gap, not yet closed.

## M1c Implementation Notes (CONST, ENUM, static arrays, preprocessor - M1 done)

- `CONST name [AS type] = expr`: Sema only checks *structurally* that the initializer is built from literals and other CONST/ENUM names (no value computation) - Codegen re-emits the original expression into a `const <type> ...= ...;` and lets `g++` fold it. A `CONST`'d name cannot be reassigned or used as a `FOR` loop variable.
- `ENUM name / IDENT [= expr] .../ END ENUM`: one enumerator per line (no comma-separated lists yet). Unspecified values auto-increment (previous + 1, or 0 for the first); explicit values are evaluated by a small integer-only constant evaluator (literals, unary +/-/NOT, and +-*\MOD SHL SHR AND OR XOR between foldable operands) that can reference earlier members of the enum, but not arbitrary CONSTs. Members become plain unscoped integer constants (`Red`, not `Color.Red`) - matches classic/FB's default unscoped enum behavior; there's no user-defined `TYPE`/namespace system yet for a scoped alternative anyway.
- Static arrays: `DIM name(upper) AS type` (0-based, inclusive upper bound - `DIM a(5)` is 6 elements, a real FB gotcha carried over deliberately) or `DIM name(lower TO upper) AS type`. Implemented as a runtime-sized `std::vector<T>` with the lower bound cached once at declaration time into a hidden temporary (real FB arrays are fixed-size with compile-time-constant bounds; using a runtime-sized vector is a deliberate simplification - strictly more permissive, much simpler, and behaviorally identical for any program whose bounds happen to be constant). Only single-dimension arrays; multi-dimension is deferred.
- Preprocessor: line-based textual pass over the raw source before lexing (not token-level), supporting object-like `#define` (no parameters) and `#ifdef`/`#ifndef`/`#else`/`#endif`. Macro names are matched case-sensitively (a deliberate departure from the rest of the case-insensitive language, matching common BASIC-community convention of all-caps macro names). Substitution skips string-literal contents and stops at a `'` comment. `#include` is intentionally not implemented here - it's already scheduled as part of M2's multi-file `#include` program support, and needs the module system to make sense of.
- **M1 is now complete** per the phased roadmap table (types/operators, control flow, CONST/ENUM/arrays, preprocessor all in place, each with e2e golden coverage). Next up per the table: M2 (`SUB`/`FUNCTION`, scoping/recursion, `REDIM`/`REDIM PRESERVE`, `GOSUB`/`RETURN`, `EXIT`, multi-file `#include`).

## M2a Implementation Notes (SUB/FUNCTION, done; rest of M2 still open)

- `SUB Name(params)` / `FUNCTION Name(params) AS type`: each compiles to a real C++ function (prototypes emitted up front so forward references and mutual recursion just work; recursion is native, no special-casing needed). Top-level `DIM`/`CONST`/`ENUM` are hoisted to genuine C++ globals (not left as `main()` locals) specifically so procedure bodies can see them - sema still enforces the existing sequential declare-before-use rule (including for globals referenced from inside a procedure body), so a procedure can only use a global declared *earlier* in the source.
- Parameter passing verified against FreeBASIC's actual `-lang fb` default rather than assumed: **BYVAL is the default for every built-in type except STRING, which defaults to BYREF**; an explicit `BYVAL`/`BYREF` keyword always overrides. A BYREF argument must be a plain variable (not a literal, expression, array element, or CONST) - checked in Sema.
- `FUNCTION` returns a value either via `RETURN expr` (immediate exit, verified against FB docs) or by assigning to the function's own name (`FuncName = expr`, does not exit) - the parser tracks which FUNCTION it's currently inside so it can recognize `FuncName = expr` as this return-assignment rather than an regular/undeclared-variable assignment. A function body always falls through to a final `return eb__ret;` as a safety net even if every branch returns explicitly.
- `EXIT SUB`/`EXIT FUNCTION` compile to a plain C++ `return;`/`return eb__ret;` - unlike `EXIT FOR`/`DO`/`WHILE`, no label/`goto` scheme is needed, since a `return` already unwinds out of any number of enclosing loops within the current function on its own.
- Statement-level procedure invocation requires `CALL Name(args)` (parens optional with zero args) - bare `Name args` without `CALL` is not supported, to avoid a real grammar ambiguity against assignment (`Name(idx) = val` vs `Name(args)` as a call) without unbounded lookahead. Calling a FUNCTION or SUB from *within an expression* (e.g. `x = Square(5)`) needs no `CALL` and was already unambiguous.
- `ExprKind::Index` (array read) was generalized into `ExprKind::Call` (name + argument list), since array reads and function calls are grammatically identical (`name(args)`) and can only be disambiguated once the name's symbol/procedure-table entry is known - Sema and Codegen each resolve it by looking up `name` (array vs. procedure), not the parser.
- Not yet supported (deferred): array parameters, default/optional parameter values, variadic parameters, and (per the phased roadmap) `REDIM`/`REDIM PRESERVE`, `GOSUB`/`RETURN`-for-GOSUB, and multi-file `#include` - these remain open for the rest of M2.

## REDIM / REDIM PRESERVE Implementation Notes (done; rest of M2 still open)

- `DIM arr() AS Type` (empty parens) declares a *dynamic* array (starts at size 0); `DIM arr(n) AS Type` / `DIM arr(lo TO hi) AS Type` declares a *fixed-size* one. Only a dynamic array can be `REDIM`'d - matches real FB ("REDIM cannot be used on fixed-size arrays"), verified against the docs rather than assumed. Attempting `REDIM` on a fixed array is a Sema error.
- `REDIM [PRESERVE] name(bound [TO bound]) [AS type]`: since M1c already backs every array with a runtime-sized `std::vector<T>` (a deliberate simplification over real FB's fixed/dynamic distinction), `REDIM` is just a resize of that same vector, which came for nearly free. Plain `REDIM` discards old contents (`.assign(n, {})`); `REDIM PRESERVE` keeps them (`.resize(n)`, which is `std::vector`'s native grow/shrink-preserving behavior).
- `REDIM`'s bounds are re-evaluated and re-cached into the array's existing lower-bound temporary (the same one `DIM` created), so later reads/writes keep using the up-to-date bound. Known limitation, not implemented: if `PRESERVE` is combined with *also* changing the lower bound in the same `REDIM`, existing elements are preserved by their internal (relative) position, not re-indexed to line up with the new bound - matches `std::vector::resize()`'s natural semantics but not necessarily FB's, and is an unusual enough combination (most real code preserves with the same base) that it's left as-is rather than adding manual re-indexing.
- Remaining in M2: `GOSUB`/`RETURN`, multi-file `#include`.

## GOSUB / RETURN Implementation Notes (done; M2's last item is multi-file #include)

- Verified against the docs first (legacy/deprecated but still real syntax): "the line label where GoSub jumps must be in the same main/function/sub block as GoSub" - i.e. it's a genuine call-with-implicit-return-address, sharing variables with its enclosing scope, not a plain jump.
- Implementation: each label that's the target of at least one `GOSUB` has the top-level statements between it and the next label (of any kind, or end of program) hoisted into a synthesized parameterless `void` function (reusing the exact same machinery as `SUB`/`FUNCTION` - prototype + body + a trailing `return;` safety net); `GOSUB label` becomes a call to that function, and a bare `RETURN` inside the span becomes a plain `return;`. This is why multiple call sites to the same `GOSUB` target each correctly resume at their own call site, and why nested/recursive `GOSUB` (a target calling another, or itself) just works - it's real C++ call/return underneath.
- A label may be a `GOTO` target *or* a `GOSUB` target, not both - Sema rejects the ambiguous dual-use case outright rather than trying to support it.
- **Accepted gap, by design, per discussion with the user**: normal top-to-bottom fallthrough into a `GOSUB`-target label is not supported - since that code is hoisted into its own function, it can only be reached via an explicit `GOSUB` call, never by execution simply reaching the label inline. Real classic-BASIC code relying on such fallthrough is already considered a footgun/anti-pattern (most programs explicitly `GOTO`/`END` past their `GOSUB` targets specifically to avoid it), so this was judged an acceptable, clearly-documented deviation rather than a reason to build a more general (and substantially more complex) call-stack-based interpreter-style solution.
- Remaining in M2: multi-file `#include`.

## Multi-file #include Implementation Notes (done - M2 is now fully complete)

- `#include "path"` (always splices, re-including if hit again - a real footgun without a guard, matching plain C `#include`) and `#include once "path"` (skips if that same file was already brought in by *any* prior `#include`, plain or `once`) are both real FreeBASIC syntax, confirmed against the docs. Relative paths resolve against the *including file's own directory* (not the main file's, and not the CWD), matching the standard C-family quoted-include convention FB already mirrors - so a chain of nested relative includes resolves correctly at each level.
- Since the existing preprocessor already worked line-by-line on raw text before lexing, `#include` fits naturally as recursive splicing at that same stage - no separate module/compilation-unit system was needed. Circular includes are detected via a stack of canonical (`std::filesystem::canonical`) paths currently being expanded, rather than recursing until a crash. `#define` macros are shared globally across the whole expansion (a `#define` in one file is visible in files included after it, matching C); `#ifdef`/`#ifndef` nesting is *not* shared - it's local to each file, and an unclosed one is reported as an error against that file specifically, rather than silently leaking into whatever follows the `#include`.
- The real complexity wasn't the splicing itself but keeping diagnostics honest afterward: a flattened multi-file blob no longer has line numbers that mean anything on their own. `SourceLoc` gained a `fileId` (indexing a registry now owned by `DiagnosticEngine`), and `preprocess()` returns a per-output-line `{fileId, originalLine}` map alongside the flattened source; the Lexer translates its own flattened-line counter through that map before stamping any token's `SourceLoc`, so every diagnostic - lexer, parser, sema alike - reports the *true* originating file and line, verified by hand for an error seeded inside an included file.
- **M2 is now fully complete**: SUB/FUNCTION (BYVAL/BYREF, recursion, scoping), REDIM/REDIM PRESERVE, GOSUB/RETURN, and multi-file #include are all in place, each with e2e golden coverage. Next per the roadmap: M3 (TYPE/CLASS, NAMESPACE, pointers, UNION).

## M3a Implementation Notes (type system foundation + plain TYPE, done)

- **Opened up the type system**: `TypeKind` gained `UserDefined`/`Pointer` (the latter unused until M3c), wrapped in a new `Type{kind, typeName, pointee}` value struct with an implicit `TypeKind` constructor/conversion, so every existing primitive-only helper (`isNumericType`, `promoteNumeric`, etc.) and every `TypeKind::X` literal kept compiling unchanged. `Expr::type`, `Stmt::declaredType`, `Param::type`, `SymbolInfo::type`, and `ProcedureInfo::returnType` all became `Type`. The one place that genuinely needed new logic (not just a mechanical type swap): `isAssignCompatible` now compares `typeName` when both sides are `UserDefined` - two different UDTs both report the same bare `TypeKind`, so a naive tag-only comparison would have silently accepted assigning a `Foo` to a `Bar`-typed target.
- `TYPE Name / field AS type ... END TYPE`, dotted read/write (`v.field`, including chains like `arr(i).field` and `obj.nested.field`), arrays-of-a-TYPE, and TYPE-typed SUB/FUNCTION parameters. Verified against the docs rather than assumed: **UDT parameters default to BYREF** (same rule as STRING); **whole-struct assignment (`a = b`) is a default memberwise/shallow copy** - which real C++ structs already do natively via their own default copy-assignment, so this needed zero special codegen, it just came for free from compiling `TYPE` to a real C++ `struct`.
- **TYPE-name/variable namespace sharing**: real FB actually *allows* reusing a TYPE name as a variable name in the same scope (with murky precedence rules the FB team's own docs call "not recommended" and note actual compiler bugs around) - eBasic makes the simpler, safer call of treating TYPE names, variables, and procedures as one shared namespace that conflicts on collision, a deliberate simplification rather than replicating FB's own acknowledged messy edge case.
- `ExprKind::Call` (already used for array reads/function calls) was deliberately *not* generalized to also cover indexing an arbitrary expression as a base (e.g. `obj.arr(i)`, indexing a field that's itself an array) - `Call`'s base is still always a plain name. Only `ExprKind::Member`'s `lhs` was generalized to accept any expression (`Ident`, `Call`, or nested `Member`), which is sufficient for `obj.field` and `arr(i).field` but not for indexing *through* a field. **Deferred**: array-typed fields (a direct consequence of the above), methods, `EXTENDS`/inheritance, `PROPERTY`, operator overloading, self-referential/forward-embedded TYPEs (impossible without pointers anyway - needs M3c).
- Struct definitions are emitted in **dependency order**, not source order: Sema's `collectTypes` already does a two-pass resolution (register all names, then resolve fields) so a field can reference a TYPE declared later in the file, but Codegen additionally needs any *embedded* TYPE's full C++ definition to textually precede the embedding struct's (C++ requires a complete type for a value-typed member, unlike a pointer). `genTypeDecl` recurses into a TYPE's field types first, is idempotent (a dependency may already have been pulled in), and is cycle-safe (a self-referential embedding is invalid C++ regardless of ordering - the recursion just breaks rather than looping forever, leaving the backend to reject the resulting incomplete type).
- Next per the M3 breakdown: `NAMESPACE` (M3b).

## M3b Implementation Notes (NAMESPACE, done)

- Verified against the docs rather than assumed: qualified access uses `Namespace.Identifier` - the same dot syntax as `TYPE`/record field access (confirmed via FB's own docs); namespaces **can be reopened** (the same name declared in multiple separate `NAMESPACE` blocks just keeps adding members - not an error, unlike `TYPE`/`SUB`/`FUNCTION` redeclaration); unqualified access to another namespace's members requires a `USING` directive, which is **not implemented** (deferred - qualified access always works, it's only the unqualified-import convenience that's missing).
- **Scoped down from the original plan on purpose**: a `NAMESPACE` body may contain `CONST`/`ENUM`/`DIM`/`SUB`/`FUNCTION` only - not nested `TYPE` and not nested `NAMESPACE` (both deferred, Sema-rejected with a clear message). This was a deliberate scope cut made mid-implementation: supporting `TYPE` inside a `NAMESPACE` would have required threading namespace-qualification through `collectTypes`' forward-reference resolution and Codegen's dependency-ordering topological sort, on top of everything else - real complexity for a rarer pattern than namespaced functions/constants. `TYPE` stays global-only for now.
- Reuses the existing flat-map, richer-key architecture rather than introducing a nested-scope stack: `symbols_`/`procedures_` are keyed by `namespace::name` when inside a `NAMESPACE`, with a two-step lookup (try the current namespace's qualified key first, then the bare global key) for unqualified access - so code inside a namespace sees its own members without qualification, falling back to global scope, exactly like real namespace resolution. Explicit external qualification (`Namespace.Member`) skips straight to the qualified key with no fallback.
- The same "disambiguate a `name(args)`/`name.member` shape by what it actually names" philosophy (already used for array-vs-function `Call`) extends naturally to a third case: `Call.lhs` (previously always null) is now optionally a qualifier expression, so `Namespace.Sub(args)` and `obj.field` share the same parser chain-building code (`parseMemberOrCallChain`), disambiguated later by Sema/Codegen based on whether the base name is a known namespace. This also happens to be exactly the shape a future method call (`obj.Method(args)`) will need in M3e.
- Codegen leans entirely on real C++ namespaces rather than manually prefixing mangled names: a `NAMESPACE`'s members are wrapped in a literal `namespace eb_x { ... }` in whichever of the type/prototype/body/globals streams it actually contributes to (skipping streams it doesn't, to avoid empty-namespace noise) - reopening "just works" since C++ namespaces are reopenable too, for free.
- Next per the M3 breakdown: pointers (`PTR`/`@`).

## M3c Implementation Notes (pointers, done)

- Verified against the docs rather than assumed: declaration is a postfix `Type PTR` suffix (multi-level `Type PTR PTR` legal); deref is `*p`; member-through-pointer is `(*p).field` or the `p->field` shorthand (both legal, `->` is pure sugar); pointer arithmetic (`p +/- n`) is legal and **scales by the pointee's size**, matching C++'s own pointer arithmetic exactly; pointer difference (`p1 - p2`, same pointee type) is legal and yields a count **in elements**, again matching C++ natively; the null pointer literal is plain `0`, and an uninitialized/no-initializer pointer auto-initializes to `0`; `ANY PTR` is FB's untyped/`void*`-equivalent pointer, implicitly convertible to/from any other pointer type, illegal to dereference directly (though `ANY PTR PTR` may be dereferenced to yield an `ANY PTR`).
- Because pointer arithmetic and pointee-scaling match real C++ pointers exactly, and `TYPE`→`struct` was already a real C++ struct, almost the entire feature "fell out for free" from targeting real C++ pointers rather than needing hand-rolled semantics: `cppType(Pointer)` just recurses (`cppType(*pointee) + "*"`, or `"void*"` for `ANY PTR`'s null pointee); a `DIM p AS T PTR` with no initializer already emits `T* eb_p{};`, which value-initializes to `nullptr` with zero special-casing, matching FB's own auto-init-to-0 rule exactly; and `p + n`/`p - n`/`p1 - p2` reuse the *exact same* `Binary` codegen path as ordinary arithmetic (`"(" + lhs + " " + op + " " + rhs + ")"`) - C++'s native pointer arithmetic does the scaling, so Codegen needed no pointer-specific branch for any of it.
- `ExprKind::AddressOf` (`@x`) and `ExprKind::Deref` (`*p`) both reuse `lhs`. `p->field` is desugared **at parse time** into the identical AST shape as `(*p).field` (`Member{lhs=Deref{lhs=p}, stringValue=field}`) inside `parseMemberOrCallChain`, so Sema and Codegen needed zero pointer-specific logic for the arrow form - it was already handled the moment `Member`'s existing lhs-is-`UserDefined` resolution and `Deref`'s new type-checking existed independently.
- New `Sema::isLvalue(expr)` helper (true for `Ident`/`Member`/`Deref`, and for `Call` only when it resolves to an array-element read, never a function call), used both for `@`'s operand and - a genuine, in-scope improvement discovered while factoring it out - the pre-existing BYREF-argument check, which had only ever accepted a bare `Ident` and would incorrectly reject perfectly valid lvalues like `arr(i)` or `obj.field` passed to a BYREF parameter. Verified fixed: `SUB Bump(BYREF v AS INTEGER)` called as `CALL Bump(arr(0))` now correctly mutates `arr(0)`.
- `isAssignCompatible` gained pointer handling: `ANY PTR` (null `pointee`) is universally compatible with any other pointer on either side; two typed pointers require **strictly identical pointees**, checked via a new `pointeesIdentical` helper - deliberately *not* the existing looser `isAssignCompatible` numeric-widening rule, since C++ pointers have no implicit `float*`↔`int*` conversion the way plain numeric variables have implicit widening/narrowing. This was caught as a real bug during hand-testing: an early version reused `isAssignCompatible` recursively for pointee comparison, which silently accepted `DIM ip AS INTEGER PTR: ip = sp` (`sp` a `SINGLE PTR`) at the Sema level, only for it to surface later as a confusing backend `g++` type error - fixed by introducing the stricter helper, re-verified to now be caught cleanly at the Sema level with a proper diagnostic.
- Assigning an integer-family expression (not just a literal `0`) to a pointer target is accepted **structurally** by Sema; a genuinely non-zero/non-null-constant assignment is left for `g++` to reject as a backend error (`invalid conversion from 'int' to 'T*'`) - the same "defer genuinely narrow edge cases to the backend" pattern already used elsewhere in this codebase, rather than building a constant-value analyzer just to distinguish "the literal 0" from "any other int" in Sema.
- `Binary`'s `Eq`/`Ne`/`Lt`/`Le`/`Gt`/`Ge` now also accept two pointers of the same family, or a pointer against a plain integer (covering the common `p = 0` / `p <> 0` null check) - codegen needed no changes here either, since C++ pointer comparison is native.
- Forward-declares every `TYPE` (`struct eb_x;`) at the top of `typesOut_` before any full definitions, discovered as a necessary fix while testing: a pointer field doesn't need its pointee's *full* definition (unlike an embedded-by-value field, which does and is why `genTypeDecl`'s dependency-ordering exists), so a pointer to a **different**, not-yet-emitted `TYPE` would otherwise reach `g++` as an undeclared type. Self-referential pointer fields (the linked-list case) don't strictly need this - a struct can always contain a pointer to its own not-yet-complete type inside its own body - but the forward-declare pass covers the mutual/forward-reference case too, for free, with one line per `TYPE`.
- Smoke-tested end-to-end with a self-referential linked list (`TYPE Node ... nxt AS Node PTR`): construction via `@`, traversal via `->` in a `DO WHILE p <> 0` loop, the `(*p).field`/`p->field` equivalence, `ANY PTR` round-tripping through `<> 0`, pointer arithmetic and pointer difference through an array (`@arr(0)`, `*(ip + 1)`, `ip2 - ip0`), and deref-assignment (`*ip = 999`) - all verified to produce the expected output. Error paths verified: dereferencing an `ANY PTR` and assigning between mismatched typed pointers are both rejected with a proper Sema diagnostic (not left to surface as a backend error).
- **Deferred** (explicitly out of scope for this slice, per the M3 plan): function pointers, `CAST`/`SIZEOF`, and ctor-invoking `NEW`/`DELETE` (the latter needs real constructors, which don't exist until `CLASS` in M3e - a bare, non-ctor `NEW`/`DELETE` was considered but skipped to avoid revisiting allocation semantics twice).
- Next per the M3 breakdown: `UNION` (M3d).

## M3d Implementation Notes (UNION, done)

- Verified against the docs rather than assumed: syntax is `UNION Name / field AS type ... END UNION` - structurally identical to `TYPE`. A union's size is the size of its largest member (not the sum) and all members share the same starting address. **Unions cannot contain variable-length strings, arrays, or any field/base with a constructor or destructor** - `STRING` is the only non-trivial type in this language slice, so that whole restriction collapses to "no `STRING`, anywhere in the member's type (directly, or nested inside an embedded `TYPE`)". Nesting is bidirectional and confirmed supported: a `UNION` may contain a `TYPE`-typed field and a `TYPE` may contain a `UNION`-typed field.
- Kept `UnionDecl` as its **own `StmtKind`** (not a bool flag on `TypeDecl`) specifically so the struct-vs-union distinction reads directly off `stmt.kind` at every use site, rather than threading an extra flag through `RecordInfo`/Codegen - the parser (`parseRecordDecl`, replacing the old `parseTypeDecl`) dispatches on which opening keyword (`TYPE`/`UNION`) is current and produces the right `StmtKind`, but is otherwise one shared body (identical field-list grammar). Sema's `structs_`/`RecordInfo`/`collectTypes` and Codegen's `typeDeclsByName_`/`genTypeDecl` were broadened to treat `TypeDecl`/`UnionDecl` uniformly (a small `isRecordDecl(kind)` check replacing the old `kind != TypeDecl` filter) - this is also why `TYPE`-embeds-`UNION`/`UNION`-embeds-`TYPE` needed zero extra code: `Type{kind=UserDefined, typeName}` never distinguished struct vs. union underneath in the first place.
- New `collectTypes` **pass 3** (after both existing passes fully resolve every `TYPE`/`UNION`'s fields): walks every `UnionDecl`'s fields and rejects any that is, or transitively contains (through a nested `TYPE`/`UNION`), a `STRING`, via a new recursive `typeContainsString` helper (cycle-guarded, mirroring `genTypeDecl`'s own guard). This has to be a separate pass run *after* all fields are resolved, not folded into the existing field-resolution pass - a `UNION` declared before the `TYPE` it embeds would otherwise see that `TYPE`'s not-yet-populated (empty) `RecordInfo` and miss a `STRING` nested inside it. Verified by hand: a `UNION` declared *before* a `TYPE` containing a `STRING` that it embeds is still correctly rejected.
- Codegen emits a real C++ `union` instead of `struct`, with one necessary difference from `TYPE`'s per-field `Type name{};` codegen: **a C++ union may have at most one member with a default (in-class) initializer**, so union fields are declared bare (`Type name;`, no `{}`) and the *whole object* is zero-initialized instead, at the `DIM` site - which the existing `Dim` codegen already does unconditionally (`Type var{};` for every non-array `DIM`, regardless of struct/union), so no separate codegen path was needed there. This also sidesteps a subtler pitfall: embedding a `TYPE` with per-field `{}` initializers (e.g. `Point` with `x{}`/`y{}`) as a union member gives that `TYPE` a *non-trivial* default constructor, which would make the union's own *implicit* default constructor deleted - but since a union is an aggregate (no user-declared constructors) and `{}` on an aggregate is aggregate-initialization rather than a call to that (deleted) implicit constructor, this never actually gets hit. Confirmed empirically: `union eb_pointorlong { eb_point eb_pt; ... }; eb_pointorlong x{};` compiles and behaves correctly despite `eb_point`'s NSDMIs.
- The forward-declaration pass added in M3c (one `struct eb_x;`/`union eb_x;` per `TYPE`/`UNION`, before any full definitions) now picks its class-key per name from `typeDeclsByName_`'s stored `Stmt*` - a forward declaration's class-key must match the eventual definition's, unlike C's more permissive elaborated-type-specifier rules.
- Smoke-tested end-to-end: byte-level type punning (an `INTEGER` unioned with a nested `TYPE` of four `BYTE` fields - not four sibling `BYTE`s directly in the union, which would all alias the *same* address and was an early test-design mistake caught by hand-verifying the output against expected byte values, not a compiler bug), `UNION`-embeds-`TYPE` and `TYPE`-embeds-`UNION` both ways, and reading back shared memory after writing through the wider member. Error paths verified: a direct `STRING` union member, and a `STRING` reached only transitively through a nested `TYPE`, are both rejected with a proper Sema diagnostic - including when the offending `TYPE` is declared *after* the `UNION` in source order.
- Next per the M3 breakdown: `CLASS` core (M3e) - flagged in the plan as the highest-risk slice; must verify value-vs-reference instance semantics before writing any code.

## M3e Implementation Notes (TYPE methods/constructors/destructors - "CLASS core", done)

- **Major finding from verification, reshaping the whole slice**: real FreeBASIC's `CLASS` keyword is a reserved word that was **never actually implemented** - the official wiki states outright "this feature isn't implemented in the compiler yet." All of FB's real, working OOP (methods, constructors/destructors, inheritance, `PROPERTY`, operator overloading) lives under `TYPE`, not a separate `CLASS` construct. Raised to the user before writing any code (per this slice's own stated risk level); decided to extend `TYPE` only, with no `CLASS` keyword at all in eBasic either - matching real, working FB exactly rather than inventing a feature real fbc doesn't have.
- Verified concrete syntax against the docs: a method is **declared** inside `TYPE...END TYPE` via `Declare Sub`/`Declare Function`/`Declare Constructor`/`Declare Destructor`, but **defined** separately, out-of-line, via `SUB TypeName.Method(...) ... END SUB` / `FUNCTION TypeName.Method(...) AS type ... END FUNCTION` / `Constructor TypeName(...) ... End Constructor` / `Destructor TypeName() ... End Destructor`. The implicit self-reference keyword is `This` (capitalized) - members are accessible directly by bare name inside a method; `This.` qualification is only needed when a parameter/local shadows a member name. A default (no-arg) constructor is auto-synthesized unless the TYPE declares its own; a TYPE can have multiple (overloaded) constructors but only one destructor; parameterized construction uses `DIM x AS T = T(args)` syntax our `DIM` doesn't parse yet.
- **Scoped down deliberately** (raised to and confirmed by the user before implementation): this slice supports only a **no-argument** constructor and destructor - `Declare Constructor()`/`Declare Destructor()` only, both parser-rejected if given any parameters. Deferred: parameterized construction (needs `DIM`-with-initializer parsing), constructor/method overloading in general, `PROPERTY`, `EXTENDS`/inheritance, operator overloading, static members, and explicit `obj.Constructor(...)`-style placement re-init.
- The "declared within TYPE, defined outside" split maps directly onto C++'s own out-of-line member-function convention (`struct Foo { void bar(); }; void Foo::bar() { ... }`), so `genTypeDecl` emits method/ctor/dtor *declarations* inside the struct body (a bare `eb_owner();`/`~eb_owner();` for ctor/dtor - no return type, unlike a real method) and a new `Codegen::genMethodDefinition` emits the out-of-line body as `RetType eb_owner::eb_method(params) { ... }` (or the ctor/dtor-specific unqualified form) - almost no new codegen machinery, just reusing the existing procedure-body-emission shape (`eb__ret` local + final `return`) via a small shared `buildParamList` helper factored out of `genProcedure`.
- `This` compiles to the literal C++ `this` (a pointer), so `This.field`/`This.Method(args)` need the same dot-vs-arrow special case already established for `Deref`-based `Member` access in M3c - both `genExpr`'s `Member`/`Call` cases and (a second copy that was missed on the first pass and caught by the smoke test, not code review) the statement-position `CallStmt` codegen needed a `this->` branch. Bare member access (no `This.` qualifier) needed **no** Codegen change at all: since a real C++ method is emitted, C++'s own implicit-`this` name lookup already resolves a bare `eb_field` correctly.
- Constructor/destructor auto-invocation (on `DIM`/at scope exit) needed **zero** special Codegen logic: `TYPE` already compiles to a real C++ struct with real RAII (established since M3a), so a user-declared C++ constructor/destructor is invoked by the language itself exactly the same way the *implicit* one always was - the existing unconditional `Type var{};` `DIM` codegen already triggers a user-defined default constructor as much as an implicit one.
- `obj.Method(args)` generalizes `Member`/`Call` resolution a third time (after array-vs-function `Call`, and `NAMESPACE`'s qualified lookup): Sema's `Call`-with-`lhs` case now checks lhs-is-a-known-namespace-Ident *first*, and otherwise `checkExpr`s the receiver and looks up a method by the receiver's `UserDefined` type - covering `obj.Method(...)`, `This.Method(...)`, and even a field/pointer-deref receiver uniformly, all through the same code path. `CallStmt` (statement-position calls, e.g. `CALL obj.Method(...)`) needed the identical three-way split.
- New per-`TYPE` `RecordInfo::methods` map (own signature table, deliberately **not** folded into the global `procedures_` map used by free `SUB`/`FUNCTION` and `NAMESPACE` members) - avoids any collision risk between two different `TYPE`s' same-named methods, or a method sharing a name with an unrelated free function, and matches how method calls are actually resolved anyway (`obj`'s type is already known at the call site, so lookup is always scoped to that one `TYPE`'s own table). Required an explicit fix in `collectProcedures`, which had to learn to *skip* top-level `SubDecl`/`FunctionDecl` stmts with a non-empty `ownerType` (an out-of-line method/ctor/dtor definition) - otherwise every method definition would have also been silently (and wrongly) registered as a global free function.
- New `collectTypes` passes (4 and 5, after the existing field/UNION-string passes): pass 4 matches each out-of-line definition to its declared prototype (rejecting a definition for an unknown `TYPE`, an undeclared method, a duplicate definition, or a declaration/definition parameter-count mismatch); pass 5 then verifies every *declared* method/constructor/destructor has a matching definition. Catching "declared but never defined" here - rather than letting it surface as a backend "incomplete type"/link error - was verified by hand with a `Declare Sub Bar()` left undefined, producing a clean Sema diagnostic instead.
- Implicit bare-member access (`data` meaning `This.data` inside a method, with a local/parameter shadowing it correctly taking priority - the verified FB rule) needed matching fallbacks in **two** separate places, since they're separate code paths in this codebase: the read-side `ExprKind::Ident` case in `checkExpr`, and the *fast-path* (non-`target`, non-`index`) branch of `StmtKind::Assign`'s own separate lookup (it doesn't route through `checkExpr(Ident)` at all). Both fall back to a field lookup on `structs_[currentClassName_]` only after the normal local/global lookup fails.
- New `Sema::currentClassName_`, threaded exactly like `insideProcedure_`/`currentFunctionReturnType_` (plain save/restore around one recursive call - methods don't nest, matching every other "doesn't nest" simplification already made for procedures and namespaces).
- Smoke-tested end-to-end with a `Counter` TYPE: constructor initializing state and printing, destructor printing at program-end scope exit (observed reading the *current*, mutated state - confirming real RAII ordering), a method calling another method on itself via `CALL This.Increment()`, a parameter shadowing a member name disambiguated via `This.label`, and plain bare-member access/assignment with no qualifier. Error paths verified: `UNION` with a `Declare` member, a duplicate constructor declaration, a declared-but-undefined method, and `This` used inside a free (non-method) `SUB` - all four rejected with clean Sema diagnostics.
- Next per the M3 breakdown: `EXTENDS` + virtual dispatch (M3f).

## M3f Implementation Notes (EXTENDS + virtual dispatch, done)

- Verified against the docs rather than assumed: `TYPE Derived Extends Base` (single inheritance only - one base, not a list); virtual dispatch requires an **explicit** `Virtual` marker (FB is C++-style explicit-virtual, not Java-style implicit) - confirmed via `Declare Virtual Function Foo() As String` inside the base and `Declare Virtual Function Foo() As String Override` inside the derived TYPE, both re-stating `Virtual` on the out-of-line definition too (though real C++ can't repeat `virtual`/`override` on an out-of-line member definition - that repetition is parsed and discarded, only the in-class declaration's flags matter for Codegen); `Base.Method()` calls the base's own implementation non-virtually (bypassing any override), matching C++'s own `Base::method()` syntax; base-constructor-chaining uses an explicit `Base(args)` call at the top of the derived constructor - **not implemented this slice**, since it's only needed for parameterized constructors (already deferred since M3e) and C++'s own implicit base-subobject default-construction already covers every no-arg-only case this slice supports "for free."
- Single inheritance walked via a **short loop, not a general MRO algorithm** (per the plan's own guidance) - `RecordInfo` gained a `baseName` (canonical, empty if none), and three small cycle-guarded helpers (`isSameOrDerivedFrom`, `findFieldInChain`, `findMethodInChain`) replace what were previously direct single-TYPE lookups everywhere a field/method is resolved (`Member`/`Call` in `checkExpr`, the implicit bare-member fallbacks in `Ident`/`Assign`, and `CallStmt`) - an inherited field/method now resolves identically to one declared directly, with zero special-casing at each call site beyond walking up one extra "hop" per ancestor.
- `isAssignCompatible` (already a free function, promoted this slice to a `Sema` member so it can walk `structs_`) gained the one genuinely new rule: a value of a TYPE derived (directly or transitively) from a target's TYPE is now compatible - the implicit-upcast rule C++ itself already applies to structs, needed so `SUB PrintInfo(BYREF s AS Shape)` can be called with a `Circle` or `Square` argument at all. This is also the mechanism that makes virtual dispatch demonstrable without pointers/`NEW`: a **BYREF parameter is already a C++ reference**, and passing a derived value to it binds the reference without slicing, so a virtual call through that parameter correctly reaches the derived override.
- Codegen leans almost entirely on real C++ inheritance: `struct eb_derived : public eb_base { ... }`, with the base treated as a dependency needing its *full* definition first (recursed into by `genTypeDecl`, exactly like an embedded-by-value field already was) rather than just a forward declaration - inheriting from an incomplete type is equally illegal. A method's in-class declaration gets a literal `virtual` prefix (if `Virtual` or `Override` was written - `Override` implies vtable participation whether or not `Virtual` was also explicitly stated, Sema doesn't require both) and/or ` override` suffix; the out-of-line *definition* repeats neither (real C++ forbids it there). `Base.Method(args)` compiles to a qualified, non-virtual `eb_base::eb_method(args)` - Codegen tracks the currently-emitting method's owning TYPE (`currentOwnerType_`, mirroring `Sema::currentClassName_`) and a `TYPE -> its base` map (`baseTypeOf_`) purely to resolve this one case.
- **Real bug found and fixed while designing `Base`, pre-existing since M3e**: a bare `This` used as a *value* (not immediately followed by `.field`/`.Method()`, which have their own special-cased codegen that never reaches this path) compiled to the literal pointer `this` - correct for nothing, since `This` used this way (e.g. passed to a parameter, or under `@This`) needs the *pointed-to value*. `&this` is flatly ill-formed C++ (`this` is a prvalue), so `@This` would have failed to compile the moment anyone wrote it. Fixed by emitting `(*this)` for a bare `This`/`Base` instead - verified this doesn't affect `This.field`/`This.Method()`/`Base.Method()`, which bypass this code path entirely with their own direct `this->`/qualified-call emission.
- New `collectTypes` passes, run only after every TYPE's fields/methods/base names are fully resolved (the same forward-reference-ordering lesson from M3d's UNION-string check applies here too - a TYPE's base may be declared later in the file): pass 6 walks every TYPE's base chain checking for a cycle; pass 7 checks that every `Override` method has a same-named method somewhere up its (now cycle-safe) base chain, and that a TYPE using `Override` actually has a base at all. A narrower mismatch - the base method existing but not itself `Virtual` - is deliberately left for the backend: Codegen emits a literal `override` regardless, and g++'s own "marked override, but does not override" error already covers that case precisely, so replicating it in Sema would be pure duplication.
- Smoke-tested end-to-end with `Shape`/`Circle`/`Square`: a `SUB` taking a `BYREF Shape` parameter, called with both a `Circle` and a `Square` instance, correctly dispatching to each one's own `Area`/`Describe` override through the reference (the core architectural risk this slice existed to prove); `Square` inheriting `Describe` unchanged (no override) still resolves correctly; `Circle.Describe` calling `Base.Describe` to build on the base's own result; and direct inherited-field access (`c.name`, set by `Shape`'s auto-invoked base constructor and then overwritten by `Circle`'s own). Error paths verified: `Override` with no matching base method, `Override` on a TYPE with no base at all, a genuine inheritance cycle (`A Extends B`, `B Extends A`), and `UNION ... Extends ...` - all four rejected with clean Sema diagnostics.
- Next per the M3 breakdown: `PROPERTY` (M3g).

## M3g Implementation Notes (PROPERTY, done)

- Verified against the docs rather than assumed: `Declare Property Name(...)` inside `TYPE`, defined outside via `Property TypeName.Name(...) ... End Property` - the same "declared within, defined outside" split as every other member procedure since M3e. Getter vs setter is disambiguated purely by **signature** (0 params + `AS type` = getter; exactly 1 param + no return type = setter), confirmed via FB's own canonical example, not distinct `Get`/`Let`/`Set` keywords. Both halves share one identifier and are accessed with plain field syntax (`w.title = "x"`, `Print w.title`) - **no parentheses, no new grammar at the access site at all**, since this already matches the existing bare `Member` shape used for plain fields.
- **Deliberate scope decision** (not explicitly required by the docs, but adopted to sidestep a real architectural asymmetry): every declared property **must have both a getter and a setter, of the same type** - a getter-only or setter-only property is rejected outright. The alternative (allowing just one) would require every `Member` resolution site to know whether it's being read or written before deciding if a property access is even valid, but `checkExpr(Member)` has exactly one code path used by both a plain read *and* (via `Assign`'s target-checking, which also calls `checkExpr`) the value-type-check half of a write - there's no single choke point that's read-only. Requiring both sidesteps this cleanly, at the cost of not supporting the (less common) asymmetric case; matches the one canonical FB example found, which always shows the pair together.
- `Stmt` gained a plain `isProperty` flag (both on the Declare-prototype and the out-of-line definition) rather than a new `StmtKind` - a getter is exactly a property-flagged `FunctionDecl`, a setter exactly a property-flagged `SubDecl`, reusing 100% of the existing method-declaration/out-of-line-definition machinery from M3e (parameter parsing, `ownerType`, body-with-return-assign for the getter). The one new parser detail: `PropName = value` inside a getter body needed `currentFunctionName_` threaded through `parseProperty` exactly like `parseFunction` already does - caught immediately by the smoke test (a getter's own `PropName = value` failed to parse as the existing return-assignment pseudo-syntax) rather than found by inspection, and fixed before shipping.
- New `RecordInfo::properties` (canonical name -> value `Type`) plus `definedGetters`/`definedSetters` sets, registered/matched by the same collectTypes passes as methods (collision-checked against fields/methods, cross-checked for getter/setter type agreement, matched to out-of-line definitions, and verified both halves are eventually defined) - and a `findPropertyInChain` helper mirroring `findFieldInChain`/`findMethodInChain` for inherited-property lookup, reusing M3f's single-inheritance chain-walk machinery unchanged.
- `Expr` gained a plain `isProperty` bool, set only on a `Member` node once `checkExpr` resolves it against a property (checked *after* the existing field lookup fails, so a field of the same name always wins - not reachable in practice since Sema already rejects a property colliding with a field name at declaration time, just a natural fallback order). No separate `ExprKind` needed - a property access is structurally identical to a field access up until Codegen, which is the one place the two need to diverge.
- Codegen rewrites a property access into two plain C++ methods, `eb_name_get()`/`eb_name_set(value)` (never a getter/setter *pair* sharing one C++ name via overloading - deliberately explicit, matching the plan's own recommendation over any operator-overload trick, since C++ has no native property syntax to lean on here the way inheritance/virtual dispatch could lean on real C++ inheritance). A **read** (`expr.isProperty` on a `Member` reached through the normal `genExpr` path) becomes `.eb_name_get()`; a **write** needs a completely different shape (a method *call*, not `target = value`) so it's special-cased directly in `Assign`'s own codegen rather than routed through `genExpr(target)` at all. A new `memberReceiverPrefix` helper (factored out of what were three near-identical `This`/`Base`/plain-receiver prefix blocks already duplicated across `Call`, `Member`, and `CallStmt`) computes the `this->`/`eb_base::`/`recv.` prefix once and is reused by all four sites (the three original plus the new property setter).
- Smoke-tested end-to-end with a `Thermometer` TYPE: a `Celsius` property with a **validated setter** (clamping to a physically valid range) and a `Fahrenheit` property whose getter/setter are both defined *in terms of* `Celsius` (a property built on another property, through explicit `This.Celsius` - bare unqualified property access inside a method is not supported this slice, only `This.`/`Base.`/`obj.`-qualified access, an explicit scope cut since it would need the same read/write-context-sensitivity problem the getter+setter requirement above already sidesteps). Verified the setter's clamping logic actually runs (an out-of-range assignment reads back clamped, not the raw value). Error paths verified: a getter declared with no matching setter, and a getter/setter type mismatch - both rejected with clean Sema diagnostics.
- Next per the M3 breakdown: operator overloading (M3h) - the last M3 slice.

## M3h Implementation Notes (operator overloading - M3 done)

- Verified against the docs rather than assumed: FreeBASIC supports **both** free-standing (global) and member (TYPE-declared) operator overloads. Global syntax takes both operands as explicit parameters (`Operator +(ByRef lhs As Rational, ByRef rhs As Rational) As Rational ... End Operator`); member syntax follows the same "declared within TYPE, defined outside" split as every other member procedure, with only the RHS explicit (`this` supplies the LHS). At least one operand must be a user-defined TYPE. There is **no `<>`-for-free from defining `=`** and no documented default-synthesized assignment operator - every operator needs its own explicit definition. The overloadable list is large (compound-assignment shortcuts, `Cast`, `[]`, `New`/`Delete`, math functions like `Sqr`/`Abs`, unary operators, `->`) - **this slice implements only free-standing binary operators** (matching the plan's own framing of a flat `(BinOp, lhsType, rhsType)` registry, and its own suggested Vector2/Complex smoke test); member operators and every operator in the extended list above are deliberately deferred.
- `Stmt` gained `isOperator`/`operatorBinOp` rather than a new `StmtKind` - an operator overload is exactly an `isOperator`-flagged, always-top-level `FunctionDecl` (operators always return a value), reusing the entire existing free-function parsing/body-checking/codegen path. New `Operator SYMBOL(lhs, rhs) As type ... End Operator` parsing matches one of the existing lexer tokens for `+ - * / \ ^ & = <> < <= > >=` plus the `Mod`/`Shl`/`Shr`/`And`/`Or`/`Xor` keyword-operators directly to a `BinOp`, reusing the enum wholesale - eBasic's `BinOp` already covers essentially the complete FB-overloadable binary set (nothing new needed there).
- New `Sema::operatorOverloads_`, keyed by a composite `(BinOp, lhsType, rhsType)` string built by `operatorKey` - a **deliberately exact match, no promotion** (per the plan's explicit "keep this narrow" guidance), verified by hand: multiplying a `Vector2` by a plain `INTEGER` literal correctly fails to resolve against a declared `Operator *(..., ByVal s As SINGLE)` overload (no int-to-single promotion), while multiplying by an explicit `SINGLE`-typed variable resolves correctly. `Binary`'s existing type-check gained exactly one new early branch: whenever either operand is `UserDefined`, resolve through this table instead of the built-in numeric/string rules below it - a single, minimal insertion point rather than touching each of the dozen existing `BinOp` cases individually.
- **Real, load-bearing bug found and fixed during design, before it could surface as a confusing backend error**: this codebase's existing `BYREF` parameter convention compiles to a plain, non-`const` C++ reference (`T&`) - which cannot bind to a temporary. A chained operator expression like `a + b + c` passes the `a + b` temporary as an operand to the second `+`, so operator overloads taking their operands `ByRef` (exactly as FB's own canonical example declares them) would have failed to compile the *moment* anyone chained two overloaded operators together. Fixed with a dedicated `buildOperatorParamList`: a `UserDefined`-typed operator parameter is **always** rendered `const T&` in the emitted C++, regardless of the source's `BYVAL`/`BYREF` (operators aren't expected to mutate their operands - that's what compound-assignment operators, out of scope here, are for); a primitive-typed parameter stays plain by-value. Verified directly: `total = v1 + v2 + v3` compiles and evaluates correctly.
- Two of the existing `Binary` codegen's built-in special cases - forced real division for `/` between two non-float operands, and `std::pow` for `^` - unconditionally tried to `static_cast<double>` their operands. Both are now guarded to skip whenever either operand is `UserDefined` (Sema has already routed those through the overload table and supplied the correct return type; the plain textual operator, added to the generic fallback switch for `Pow` specifically - it previously had no entry there, always intercepted earlier - correctly resolves to the user's own overloaded C++ operator instead). The relational-operator branch (`==`/`!=`/`<`/...) needed no equivalent guard: its `(lhs op rhs) ? -1 : 0` wrapping is redundant-but-harmless even when `lhs op rhs` is itself already a fully-formed BOOLEAN (`-1`/`0`) from a user's own overloaded `operator==`, since C++ treats `-1` and `0` as truthy/falsy identically to a native comparison.
- **Known, documented limitation, inherent to reusing C++'s own operator tokens rather than named methods**: eBasic's `BinOp` enum intentionally maps more than one BASIC concept onto the same lexical token in a few places (`Div`/`IDiv` both use `/`; `Add`/`Concat` both use `+`; `Pow`/`Xor` both use `^`) - a `TYPE` cannot overload both operators in such a pair for the same operand types, since they'd collide on the same C++ operator function. Not specifically guarded against (surfaces as a natural backend "redefinition" error); not hit by anything this slice's scope actually needs.
- Smoke-tested end-to-end with a `Vector2` value TYPE: `+`, `-`, and a scalar `*`, plus `=`/`<>` built from `AND`/`NOT` over per-component comparisons - all free-standing overloads. `total = v1 + v2 + v3` specifically exercises the const-ref fix (the `v1 + v2` intermediate is an unnamed temporary). Error paths verified: an operator with no user-defined operand, a duplicate overload for the same operand-type pair, and a unary-arity `Operator -(...)` (exactly one parameter) all rejected with clean diagnostics.
- **M3 is now fully complete**: `TYPE`/`UNION` records and field access (M3a), `NAMESPACE` (M3b), pointers (M3c), `UNION` (M3d - reordered after pointers since it shares field-decl infra), `TYPE` methods/constructors/destructors (M3e - "CLASS core," retargeted from a nonexistent `CLASS` keyword onto real, working `TYPE` once verification revealed FB's `CLASS` was never implemented), `EXTENDS`/virtual dispatch (M3f), `PROPERTY` (M3g), and free-standing operator overloading (M3h) all shipped with golden e2e coverage, clean `-Wall -Wextra` builds, and roadmap notes documenting every verified FB semantic and deliberate scope cut along the way. Next per the phased roadmap: M4 (C/C++ interop).

## M4 Plan Summary (C/C++ interop, in progress)

A detailed sub-slice plan (M4a-d) was written with the user before implementation started, mirroring M3's a-h breakdown. Verified against real FreeBASIC docs first: `Extern {"C"|"C++"|"Windows"|"Windows-MS"|"rtlib"} [Lib "libname"] ... End Extern` blocks (plus an inline single-declaration form with `Cdecl`/`Alias "name"`/`Lib "name"`); `ZSTRING` is FB's C-compatible string type, with any `STRING` argument implicitly converting to a `ZSTRING PTR` parameter at a call site; real FB bindings are hand-written or tool-generated (`fbfrog`/`h_2_bi`) `.bi` files, not live header parsing - building an equivalent header-translator tool is explicitly out of scope. **Scope decision, confirmed with the user**: M4 covers C interop plus C++ free-function/namespace/overload interop (leveraging that eBasic transpiles to *real* C++ and can just emit genuine C++ declarations rather than emulating g++'s name mangling the way `fbc` itself has to) - calling methods on a real C++ object is a distinctly larger problem, deliberately deferred to its own later milestone. `Stdcall`/`Windows` calling convention is deferred to M8's Windows port (cdecl is the only relevant convention on today's Linux/macOS/Haiku-only targets). Full sub-slice detail lives in the plan file used to drive implementation; each sub-slice gets its own "M4x Implementation Notes" section below as it ships, exactly like M3's sections.

## M4a Implementation Notes (driver/build plumbing + test fixtures, done)

- `Module` (`ast.hpp`) gained `externLibs` (a plain `vector<string>`, empty until M4b's parser work populates it from `Lib "name"` clauses) - `Codegen::generate()` copies it into a new `externLibs_` member exposed via a public `externLibs()` accessor, and the driver appends one `-l<name>` per entry to the backend invocation, positioned *after* the generated source file (required so a traditional linker resolves symbols from it correctly).
- **Real, necessary plumbing that didn't exist at all before this slice**: the driver previously built a fixed g++ command line with no way to pass extra linker flags. Added a repeatable `-L <dir>` CLI flag (`main.cpp`) for library search paths - needed because a `Lib "name"` clause only names a library, never a path to it (exactly like g++'s own `-L`/`-l` split), and a test fixture's build-tree location can't be known from `.bas` source.
- **Verified empirically, not assumed**: whether a bare `-l<name>` finds a freshly-built static fixture library. It does not (confirmed directly with `gcc`: linking against `libebfixturec.a` fails with "cannot find -lebfixturec" using `-l` alone, succeeds once `-L <builddir>/tests/fixtures` is added) - confirming the `-L` flag above is required, not a redundant nicety.
- New `tests/fixtures/c/fixture.c` and `tests/fixtures/cpp/fixture.cpp` - tiny, real, separately-compiled static libraries (`add_library(... STATIC ...)` in `tests/CMakeLists.txt`, output to a fixed `tests/fixtures/` build-tree subdirectory) to link the M4b/c/d golden tests against, rather than relying on system libc/libm quirks. The C fixture already includes the functions those later slices need (a plain `int` function, a `char*`-returning/taking pair including a null-return case, and a `create`/`get`/`add`/`destroy` opaque-handle API) so this file doesn't need revisiting per slice; the C++ fixture has a namespaced free function and an overloaded pair. Required enabling the `C` language in the top-level `CMakeLists.txt` (`project(ebasic LANGUAGES CXX C)`), previously `CXX`-only.
- `tests/e2e/run_case.sh` gained an optional third argument (a fixture library directory), forwarded to `ebc` as `-L <dir>` - backward compatible, since it's optional and every existing golden test still invokes the script with exactly two arguments.
- No BASIC-syntax surface yet (by design - this slice is pure plumbing) and so no golden e2e test of its own; M4b's test is the first real end-to-end proof that a `.bas` program can declare, call, and link against an external library through this machinery. Verified instead: the fixture libraries build cleanly, a clean `-Wall -Wextra` rebuild of both `ebc` and the fixtures produces zero warnings, and the full prior e2e suite (17/17) still passes unchanged.
- Next per the M4 breakdown: `EXTERN`/`DECLARE` core (C interop) + `ZSTRING` (M4b).

## M4b Implementation Notes (EXTERN/DECLARE core + ZSTRING, done)

- Verified against real FB docs/examples rather than assumed, and caught one real ordering mistake in the process: the confirmed grammar is `Declare Function Name [Cdecl] [Lib "libname"] [Alias "name"] (params) As type` - the calling-convention/`Lib`/`Alias` clauses all come *before* the parameter list (e.g. real FB's own canonical binding: `Declare Function strcpy CDecl Alias "strcpy" (ByVal dest As ZString Ptr, ...) As ZString Ptr`). An early implementation placed these clauses *after* the signature instead (a plausible-looking but unverified guess) - caught by going back to verify a second, more specific source once the first fetch only confirmed the block form, not the standalone one, and fixed before shipping. A standalone (non-block) `Declare` can only ever produce `"C"` linkage in real FB too (`Cdecl` alone, no linkage string) - `"C++"` linkage is only reachable via an `Extern "C++" ... End Extern` block, which supplies the linkage/`Lib` default for every `Declare` line inside it (each line can still specify its own `Alias`, since case-sensitivity mismatches are per-function).
- `Stmt` gained `isExtern`/`externLinkage`/`externAlias`/`externLib` - a `Declare`d signature is exactly a `SubDecl`/`FunctionDecl` with an empty `body` (no eBasic-side definition at all, unlike M3e's "declared in TYPE, defined out-of-line in this same file" methods, which still get a body eventually) - reusing the entire existing signature-parsing/param-list machinery unchanged. An `Extern "C"|"C++" [Lib "name"] ... End Extern` block yields *multiple* top-level `Stmt`s (one per `Declare` line inside), which doesn't fit `parseStatement()`'s one-`Stmt`-per-call contract - handled by special-casing `EXTERN` directly in `parseModule()`'s own top-level loop (appending straight into `module.stmts`) rather than inventing a container `StmtKind` that Sema/Codegen would then need to unpack.
- New `ZStringT`: unlike real FB (which needs `ZSTRING PTR` even for a single C string parameter, since bare `ZSTRING` is a fixed-length embedded buffer type - deliberately not implemented, out of scope), eBasic's bare `ZSTRING` *is* the pointer form directly (`cppType` maps it straight to `const char*`); `ZSTRING PTR` still works too, via the existing generic Pointer-with-ZStringT-pointee path, meaning "pointer to a ZSTRING" (a `char**`-shaped parameter, e.g. for `argv`-style APIs) rather than needing a second, redundant pointer indirection for the common single-string case.
- **The marshaling story needed zero Codegen changes**, only two small, targeted `BString` (`runtime/include/ebasic/runtime/bstring.hpp`) changes: a null-safe `BString(const char*)` constructor (constructing `std::string` from `nullptr` is UB, and a null C string is a real, expected value at this boundary - e.g. a C function documented to return NULL on failure, exactly what the fixture's `eb_fixture_maybe_null` exercises) and a new implicit `operator const char*() const` conversion. Once both exist, C++'s own copy-initialization rules handle *both* directions automatically wherever a `BString` meets a `const char*`-typed context (an extern parameter, an extern return value assigned into a `STRING` variable) - Sema only needed to make `StringT`/`ZStringT` mutually `isAssignCompatible` (extending, not duplicating, the existing pointer-compatibility-style branch), no per-call-site conversion logic anywhere in Codegen.
- New `Sema::collectExternSignatureChecks` (run after `collectTypes`, since it needs `structs_` populated): rejects a bare `STRING` (must be `ZSTRING`/`ZSTRING PTR` instead - `BString`'s ref-counted/COW representation isn't C-ABI-compatible at all) and a `UserDefined` TYPE with a constructor, destructor, or virtual method (breaks standard-layout/C-ABI compatibility) used as a parameter or return type on an `isExtern` signature - both would otherwise silently corrupt memory at runtime rather than fail to compile. A TYPE with no fields/methods at all (M4d's opaque "handle" types, not yet formally flagged as such) already passes this check for free, since it has no ctor/dtor/virtual method by construction - confirmed by hand that a plain, ctor-less TYPE is correctly *allowed* through, both by value and (implicitly, already supported since M3c) via `PTR`.
- Codegen's biggest structural addition: a new `externProcNames_` pre-scan map (canonical BASIC name -> real external symbol name, the `Alias` if given else the declared name verbatim) - **every** call-site codegen path that could reference a free procedure (`genExpr`'s no-`lhs` `Call` case, and `CallStmt`'s no-`target` case) now checks this map *before* falling back to `mangleName`, since an extern-bound procedure must be called by its real, unmangled, case-preserved external name - `mangleName`'s `eb_`-prefixing would rename it to a symbol the linker can never find in the external library. `genProcedure` gained an early-return branch for `isExtern`: emits only a prototype (no body, no `procOut_` entry at all), wrapped in `extern "C" { ... }` when linkage is `"C"` - `"C++"` linkage needs no wrapping at all, since it's already a normal, real C++ declaration once emitted (the concrete, recurring payoff of transpiling to genuine C++ rather than needing to emulate `g++`'s name mangling the way `fbc` itself has to).
- Smoke-tested end-to-end against the real, separately-compiled `tests/fixtures/c` static library (both the `Extern "C" Lib "..." ... End Extern` block form and the standalone `Declare ... Cdecl Lib "..." Alias "..." (...)` form, the latter specifically re-verifying the corrected clause ordering): a plain `int` function, a `ZSTRING`-returning function assigned into a `STRING` variable, a `STRING` variable passed directly to a `ZSTRING` parameter, and the null-return case (correctly producing an empty `STRING`, not a crash). Error paths verified: a bare `STRING` parameter, a constructor-having TYPE parameter, and an unrecognized `EXTERN` linkage string (e.g. `"Pascal"`) - all three rejected with clean diagnostics; a plain ctor-less TYPE parameter confirmed to compile cleanly (not falsely rejected).
- New e2e golden test (`tests/e2e/extern_c`) is the **first golden test to link against an external library** - `tests/CMakeLists.txt`'s `EBASIC_FIXTURE_LIB_DIR` (from M4a) is passed as `run_case.sh`'s third argument, forwarded to `ebc` as `-L <dir>`. Full ctest suite (18/18) and a clean `-Wall -Wextra` rebuild (covering `ebc` and both fixture libraries) both pass.
- Next per the M4 breakdown: C++ free-function/namespace/overload interop (M4c).

## M4c Implementation Notes (C++ free-function/namespace/overload interop, done)

- **Verified rather than assumed, and this reshaped the whole slice's design**: the plan's own starting assumption - that real FB's `Extern "C++"` has no namespace syntax of its own, so eBasic would need a bespoke clause - turned out to be wrong. Real FB *does* bind namespaced C++ functions, but by **reusing its own existing `NAMESPACE` construct**, nested directly inside the `Extern` block: `Extern "c++" Lib "mylib" \n Namespace mylib Alias "mylib" \n Declare Function test() As Integer \n End Namespace \n End Extern`, called as plain `mylib.test()`. Adopted this exact real-FB pattern instead of inventing a new one - which turned out to be a better fit anyway, since eBasic already had a complete `NAMESPACE` implementation (M3b) to reuse rather than needing new call-site syntax at all.
- `NamespaceDecl` nested inside an `Extern` block reuses the *exact same* `StmtKind::NamespaceDecl` a regular BASIC `NAMESPACE` produces - Sema's existing qualified-lookup machinery (`collectProcedures`'s namespace recursion, `namespaces_`, the qualified-then-bare lookup) needed **zero changes**: an `isExtern` member nested in a `NamespaceDecl` already registered into `procedures_` under its namespace-qualified key exactly like a regular one would, since `collectProcedures` never excluded `isExtern` procs in the first place (a detail double-checked directly in the code rather than assumed, since M4b's design notes could have suggested otherwise). `Namespace Name [Alias "realName"] ... End Namespace`'s own optional `Alias` (for when the BASIC-visible namespace name must differ from the real C++ one) is stored on the `NamespaceDecl`'s own `externAlias` field (reused from its `Stmt`-level meaning on `SubDecl`/`FunctionDecl` - a deliberate, documented reuse across different `Stmt` roles, consistent with this codebase's established flat-struct-reuse convention).
- **Real bug caught by the smoke test, not by inspection**: the first implementation emitted a namespace-qualified extern declaration as a standalone, fully-qualified statement (`RetType realNs::Member(...);`) - which is only legal C++ if `realNs` is *already* an open namespace somewhere earlier in the translation unit. It never is; this is the only place it's ever mentioned. Fixed by having Codegen wrap a "purely extern" `NamespaceDecl`'s members in a **real, unmangled** `namespace realNs { ... }` block instead (using the namespace's real/aliased name, never `mangleName`) - after which the *unqualified* name inside naturally resolves, and a call site can validly write the qualified form. Each member's own prototype emission (`genProcedure`'s `isExtern` branch, unchanged since M4b) didn't need to know about this at all.
- Since eBasic has no general cross-FFI overload resolution (only the narrower, same-name operator-overload table from M3h), **each declared binding names one concrete C++ overload** - calling multiple real overloads of the same C++ function (verified with the fixture's `Add(int,int)`/`Add(double,double)` pair) means declaring multiple BASIC-visible names (e.g. `AddInts`/`AddDoubles`), each `Alias`ed to the *same* real name but with its own distinct declared parameter types - g++'s own normal overload resolution then picks the right one for each based purely on the declared prototypes' argument types, since two prototype declarations for different overloads of the same real name never conflict in C++.
- Codegen's `externProcNames_` pre-scan (introduced in M4b) is now genuinely recursive (`collectExternProcNames`), tracking **two parallel prefixes** as it descends into `NamespaceDecl` bodies: a BASIC-visible canonical-name prefix (what a `Call`/`CallStmt` node's namespace-qualified `lhs`/`target` actually spells, used as the lookup key) and a real-external-name prefix (what must actually be emitted, built from each `NamespaceDecl`'s own `externAlias`) - these can differ freely, which is the entire point of supporting a namespace `Alias`. `resolveCalleeName` (a new small helper factored out of what were separate inline checks in `genExpr`'s `Call` case and `CallStmt`'s codegen) is the single place both call-site paths now go through, checking this map before falling back to the ordinary `mangleName`-based rendering.
- Explicitly deferred, per the plan and the user's earlier scope decision: C++ template instantiation from a BASIC declaration, and calling methods on a real C++ object (a separate, later milestone).
- Smoke-tested end-to-end against the real `tests/fixtures/cpp` static library: a namespaced free function (`ebfixture::Square`) and both members of an overloaded pair (`ebfixture::Add(int,int)`/`ebfixture::Add(double,double)`), all called via plain `ebfixture.Name(...)` BASIC syntax. Error path verified: calling an undeclared member of an extern-bound namespace (`ebfixture.NotThere(...)`) is rejected with the same clean "namespace has no member" diagnostic a regular BASIC `NAMESPACE` already produces.
- New e2e golden test (`tests/e2e/extern_cpp`). Full ctest suite (19/19) and a clean `-Wall -Wextra` rebuild (covering `ebc` and both fixture libraries) both pass.
- Next per the M4 breakdown: opaque "handle" external types (M4d) - the last M4 slice.

## M4d Implementation Notes (opaque "handle" external types, done - M4 is now fully complete)

- **No new grammar at all**, as the plan anticipated: a `TYPE Name\nEND TYPE` with zero fields already parsed successfully before this slice (`parseRecordDecl`'s field loop just runs zero times) - this slice is entirely Sema recognition + Codegen emission + enforcement, no lexer/parser changes.
- New `RecordInfo::isOpaque` (`sema.hpp`), computed once collectTypes has fully resolved a TYPE's fields, methods (which already covers properties/ctor/dtor - all stored in the same `methods`/`hasCtor`/`hasDtor` fields), and `EXTENDS` base: `isOpaque = fields.empty() && methods.empty() && properties.empty() && !hasCtor && !hasDtor && baseName.empty()`. Deliberately scoped to `TypeDecl` only, not `UnionDecl` - an empty `UNION` is a degenerate edge case, not this feature's target, and real C libraries only ever expose opaque *structs*.
- Three enforcement points, all new: **by-value `DIM`** of an opaque TYPE (checked directly in `checkStmt`'s `Dim` case); **by-value embedding** as another TYPE/UNION's field, and **`EXTENDS` naming an opaque TYPE as the base** (both checked in a new pass run right after `collectTypes`'s per-type field/EXTENDS resolution, since checking a forward-referenced opaque TYPE during that same pass would see an as-yet-unpopulated `RecordInfo` - the identical forward-reference concern that already motivated the existing UNION/STRING check being its own later pass). The reverse EXTENDS direction (an opaque TYPE itself using `EXTENDS`) needed no separate check: assigning `baseName` non-empty already makes `isOpaque` false for that TYPE by construction, so there's nothing left to reject on that side. All three diagnostics point the user at the fix (`'Name PTR'` instead of by-value).
- **Codegen**: `genTypeDecl` recomputes the same opaque condition directly from the AST `Stmt` (fields/methods/baseTypeName all empty) - Codegen has no access to Sema's internal `structs_` table, so this is a small, deliberate, structurally-identical recomputation rather than a new cross-pass channel. When true, it emits a bare `struct eb_name;\n\n` and returns immediately, **never** emitting a `{ };` body - matching a real incomplete C struct exactly (an invented empty-but-complete struct would silently mismatch the real library's actual, unknown layout, even though both happen to compile). The existing unconditional "forward-declare every TYPE/UNION up front" pass (predating this slice, needed for self-referential/mutually-referential pointer fields) still also emits `struct eb_name;` once more before this - a harmless, pre-existing redundant forward declaration in C++, not a bug introduced here.
- Smoke-tested end-to-end against the real `tests/fixtures/c` static library's existing opaque-handle API (`eb_fixture_handle_create/get/add/destroy` around `struct eb_fixture_handle`, present since M4a but unused until now): bound as `TYPE Handle\nEND TYPE` plus four `Extern "C"`-bound, `Handle PTR`-taking/returning declarations; created a handle, read/mutated/read its value again, destroyed it - correct output (`10`, `15`) confirmed by both direct execution and inspecting the generated `.gen.cpp` (a bare `struct eb_handle;` forward declaration, no body, exactly as designed). Error paths verified: a by-value `DIM h AS Handle`, a by-value embedding (`Wrapper` containing an `inner AS Handle` field), and a TYPE `EXTENDS`ing an opaque TYPE - all three rejected with the intended, specific diagnostics.
- New e2e golden test (`tests/e2e/extern_handle`). Full ctest suite (20/20) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure with `-DCMAKE_CXX_FLAGS="-Wall -Wextra" -DCMAKE_C_FLAGS="-Wall -Wextra"`, covering `ebc` and both fixture libraries) both pass with zero warnings.
- **M4 is now fully complete**: C interop (`EXTERN`/`DECLARE`, `ZSTRING`), C++ free-function/namespace/overload interop, and opaque handle-style external types all shipped with golden e2e coverage, mirroring M3's own a-h completion pattern. Real C++ object/method interop remains explicitly deferred to a later milestone, per the plan's original scope decision. Next per the phased roadmap: M5 (`ebpm` package manager) - not yet planned in detail.

## M5 Plan Summary (`ebpm` package manager, in progress)

A detailed sub-slice plan (M5a-e) was written with the user before implementation started, mirroring M3/M4's own a-h/a-d breakdowns. **The core open design question**: how does a library package's code actually reach a dependent app's binary? Resolved explicitly with the user, who asked to verify what Cargo itself does before committing rather than assuming - and Cargo does *not* work by textual source inclusion: each crate compiles independently to an `.rlib` and downstream crates link against it. This plan adopts the same shape (**true separate compilation**, not a single shared compilation unit), refined into an approach that reuses M4's *existing* `EXTERN`/`DECLARE`/opaque-TYPE machinery wholesale rather than building new cross-package Sema/AST-importing infrastructure: a `[lib]` package compiles to a real static library via a new `ebc --lib` mode, plus an **auto-generated** `<name>.iface.bas` interface file (an ordinary `Extern "C++" Lib "<name>" ... End Extern` block, `Alias`ed to the library's real, already-mangled symbols) that a dependent package `#include`s and links against - 100% existing M4 machinery on the consuming side, zero new Sema logic. `toml++` (`marzer/tomlplusplus`, MIT, single-header, verified via its own repo/docs) was picked for `ebasic.toml` parsing, vendored directly rather than fetched at configure time (keeps every target, including Haiku/CI, fully offline-capable like the rest of the build). Full sub-slice detail lives in the plan file used to drive implementation; each sub-slice gets its own "M5x Implementation Notes" section below as it ships.

## M5a Implementation Notes (ebc --lib mode + manifest/TOML plumbing, done)

- New `ebc --lib` mode: `Codegen::generate` gained a `libMode` parameter that skips the `int main() { ... }` wrapper entirely (the types/protos/procs/globals streams are otherwise unchanged) - the driver structurally rejects any top-level `Stmt` that isn't a declaration (`Dim`/`Const`/`Enum`/`SubDecl`/`FunctionDecl`/`TypeDecl`/`UnionDecl`/`NamespaceDecl`) *before* calling Codegen at all, checked directly against `module.stmts` with no new Sema state threaded through - a library's object file must never define `main` itself, since it would collide with the consuming package's own `main` at final link time. The generated `.cpp` is compiled with `g++ -c` to an object file, then archived with `ar rcs` into `lib<name>.a` - mirroring the exact `tests/fixtures/{c,cpp}` pattern from M4a, just driven by `ebc` itself now.
- New `Codegen::generateLibraryInterface`: walks the same module's top-level `TypeDecl`/`UnionDecl` (re-emitted verbatim, real BASIC syntax, via a new `basicTypeName` helper - the inverse of `cppType`, needed only here) and free `SubDecl`/`FunctionDecl` (each `Declare`d with an `Alias` set to its real `mangleName`d symbol - ordinary mangling is completely unchanged). **A real bug caught by reasoning through the ABI before ever running it, not by a crash**: an early version would have exported *any* procedure regardless of signature, but a plain (non-extern) `FUNCTION`/`SUB` taking or returning `STRING` compiles that parameter to a real `BString` (a non-trivial class, passed BYREF by default), while the interface file's only C-ABI-safe string type is `ZSTRING` (a bare `const char*`) - the same `Alias`-matched symbol name would then be called with a completely different argument representation, a silent and dangerous ABI mismatch, not a merely cosmetic one. Fixed by having the generator skip (and comment-annotate) any procedure whose signature mentions `STRING`, documented as a deliberate scope cut - safely exporting one would need an auto-generated `ZSTRING`<->`BString` marshaling shim at the boundary, not just a re-declared prototype. A record (`TYPE`/`UNION`) with any method/ctor/dtor/`EXTENDS` is likewise skipped (not yet exportable - only plain-data/opaque records are, matching exactly what M4b/M4d's own extern-signature checks already allow).
- New `-I <dir>` flag (`preprocess()` gained an `includeDirs` parameter, consulted in `handleInclude` as a fallback *only* after the existing includer-relative lookup fails, for a non-absolute path) - lets a package's source `#include` a dependency's auto-generated interface file without knowing its exact relative filesystem path, exactly mirroring how a C/C++ compiler's own `-I` list only ever backs up the quote-form's primary, includer-relative search (never overriding it). Zero existing behavior changed for any program that passes no `-I` (the new parameter defaults to empty).
- New `pkg/` component (per the roadmap's own long-planned repo layout): vendored `third_party/tomlplusplus/toml++/toml.hpp` (v3.4.0, MIT, fetched once and checked in rather than pulled via `FetchContent` - keeps CMake configure fully offline); a `Manifest`/`Dependency`/`LibTarget`/`BinTarget` struct set + `loadManifest` (`pkg/src/manifest.{hpp,cpp}`) parsing `[package]`/`[lib]`/`[bin]`/`[dependencies]` with field defaults (`[lib].path` -> `"src/lib.bas"`, `[bin].name` -> the package name, `[bin].path` -> `"src/main.bas"`) and validation (a manifest needs a `[package].name` and at least one of `[lib]`/`[bin]`; a dependency needs exactly one of `path`/`git`); `ebpm new <name> [--lib]` and `ebpm init [--lib]` scaffold a manifest + stub `.bas` source, matching Cargo's own `new`-creates-a-directory vs. `init`-uses-the-current-one distinction exactly.
- `ebasic::runProcess` (`compiler/src/driver/process.{hpp,cpp}`) promoted into its own small `ebasic_process` static-lib CMake target, linked by both `ebc` and the new `ebpm` executable, rather than either duplicating the file or having `pkg/` reach across `compiler/`'s own source tree for it.
- Smoke-tested end-to-end by hand, proving the whole linking mechanism *before* any ebpm build/run orchestration exists (mirrors how M4a hand-verified `-L`/`-l` with raw `gcc` first): a `lib.bas` with a plain-data `TYPE Point`, a `Point`-returning function, an `INTEGER` function, and a `STRING`-returning function; `ebc --lib` produced a working `libmylib.a` and `mylib.iface.bas` (confirmed the `STRING` function was correctly omitted, with an explanatory comment); a hand-written consumer `#include`d the interface (once via a same-directory `#include`, once via `-I` pointing at a separate directory) and linked the archive using the *already-existing* `-L`/`-l` flags from M4a, producing correct output both times. Error path verified: a `--lib` source with a stray top-level `PRINT` is rejected with a clean driver-level diagnostic naming the offending line.
- No golden e2e test of its own yet (pure plumbing, like M4a) - verified instead via the hand-run smoke tests above, a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, covering `ebc`, `ebpm`, and both fixture libraries), and the full prior e2e suite (20/20) still passing unchanged.
- Next per the M5 breakdown: `ebpm build`/`run` for a single, dependency-free package (M5b).

## M5b Implementation Notes (ebpm build/run for a single, dependency-free package, done)

- New `pkg/src/build.{hpp,cpp}`: `buildPackage(manifest, packageDir, extraIncludeDirs, extraLibDirs, err)` invokes `ebcCommand()` once per target a package declares (`[lib]` via `ebc --lib`, `[bin]` normally) into `<packageDir>/target/`, threading `-I`/`-L` parameters through now (unused - empty - until M5c's dependency resolution needs them) so that slice won't need to reshape this signature. `ebcCommand()` resolves the real `ebc` binary from the `EBC` environment variable, falling back to the bare name `"ebc"` for the OS's own `PATH` search - deliberately mirrors `ebc`'s own `CXX`-environment-variable-else-`"g++"` convention (`main.cpp`) exactly, and matters concretely here since a dev build tree puts `ebc`/`ebpm` in separate CMake target directories (`compiler/`/`pkg/`), not side by side, so a same-directory lookup would have failed.
- `ebpm build`/`ebpm run [-- args...]` (`pkg/src/main.cpp`) both operate on `./ebasic.toml` only for now (no package-path argument, no dependency graph - that's M5c). `run` skips a redundant rebuild via a new `isStale(srcPath, outPath)` helper (`build.cpp`) - a deliberately simple single-file mtime comparison, no `#include` dependency-chain tracking or content hashing, documented as good enough for this slice. Arguments after a literal `--` are forwarded verbatim to the built binary; anything given *before* `--` is currently rejected (no `run`-side flags exist yet).
- **Two real bugs caught by hand-testing, not by inspection**: (1) an initial version printed `ebpm`'s own "Compiling ..." progress line to `stdout` via plain `"\n"` - under output redirection (piping to a file, or ctest's own capture) this is fully buffered rather than line-buffered, so it could appear *after* a subsequently-invoked child process's own directly-written output despite executing first, an incorrect apparent ordering. Fixed by switching to `std::endl` (explicit flush) *and*, more importantly, moving these lines to `stderr` entirely - which also fixes a second, more serious issue: printing build progress to `stdout` would otherwise interleave with (and corrupt) the actual program's real output whenever both are captured together, e.g. by `ebpm run > output.txt` or by this slice's own golden-test harness. This now deliberately matches Cargo's own real convention (build noise on `stderr`, program output only on `stdout`) rather than being an arbitrary choice. (2) A test-authoring mistake of my own - running `ebpm new` from the wrong working directory (a copy-paste command chain that set a `$SCRATCH` variable but never actually `cd`'d into it) created a stray package directory inside the real project checkout - caught immediately via `git status` before it could be committed, and removed; a reminder of why `git status` before staging is a standing project discipline, not just for destructive commands.
- New parallel golden-test harness, `tests/e2e_pkg/` (mirrors `tests/e2e/` but drives `ebpm` rather than `ebc` directly): `run_case.sh` copies a fixture *package directory* (`ebasic.toml` + `src/`) into an isolated temp workdir (so `target/` build output never pollutes the checked-in fixture), sets `EBC` to the just-built `ebc` binary, runs `ebpm run`, and diffs its `stdout` against `expected.stdout` - clean now that build progress lives on `stderr`. First case: `tests/e2e_pkg/simple_bin`, a plain no-dependency `[bin]` package.
- Full ctest suite (21/21, the new `e2e_pkg_simple_bin` alongside all prior tests) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, covering `ebc`, `ebpm`, and both fixture libraries) both pass with zero warnings.
- Next per the M5 breakdown: path dependencies, dependency graph, and lockfile (M5c) - this is the slice that hits M5's actual "lib + app" done-when bar.

## M5c Implementation Notes (path dependencies, dependency graph, lockfile - M5's "lib + app" bar hit, done)

- New `pkg/src/resolve.{hpp,cpp}`: `resolveDependencyGraph(rootDir, order, err)` is a memoized DFS over `[dependencies]` edges (`path` only - a `git` dependency is rejected with a clear "not supported yet (M5d)" message rather than silently ignored), returning every reachable package - the root always last - in dependency-first order, so building `order` front-to-back satisfies every ordering requirement with no separate topological sort. A diamond dependency (reached through more than one path) is resolved and appears exactly once. Cycle detection is a plain "currently on the DFS stack" check, independent of cycle length - verified by hand with a direct 2-cycle (`app -> lib1 -> lib2 -> lib1`) and a 3-cycle (`app -> lib1 -> lib2 -> lib3 -> lib1`), both rejected identically to the simplest 1-hop case. A dependency with no `[lib]` target is also rejected (nothing for a dependent package to link against).
- New `pkg/src/lockfile.{hpp,cpp}`: `writeLockfile` records every non-root package in the resolved graph as a `name`/`path` pair in `<root>/ebasic.lock` (plain TOML, hand-formatted text - consistent with how `ebpm new`'s manifest scaffolding already writes TOML directly rather than through `toml++`'s serializer). Regenerated on every `ebpm build`. **Deliberately not read back yet** to skip re-resolution on a repeat build - path resolution is cheap enough (filesystem + TOML parsing, no network) that this optimization has no real payoff until M5d's git dependencies make resolution genuinely expensive (a network fetch); documented as an explicit, motivated scope cut rather than an oversight.
- **A real, non-obvious bug found by hand-testing a three-package chain (`app -> mid -> base`), not by inspection**: an initial version only passed `-I`/`-L` for each package's *direct* dependencies. `mid` compiled fine (it directly depends on `base`), but linking `app` failed with an undefined reference to `base`'s function - because `app`'s own source only `#include`s `mid`'s interface file, never `base`'s, so `app`'s compiled module has no `Lib "base"` clause anywhere in its own AST for `ebc` to auto-derive a `-lbase` flag from (the existing M4-era auto-derivation is purely per-translation-unit). Fixed two ways together: (1) `buildPackageWithDeps` now computes each package's *entire transitive* dependency closure (`collectTransitiveDeps`, a memoized recursive walk, dedup'd by canonical directory so a diamond dependency contributes only one `-I`/`-L`/`-l` set) rather than just its direct dependencies; (2) added a genuinely new `ebc` flag, `-l <name>` (mirroring the existing `-L <dir>` exactly), since no amount of `-I`/`-L` alone fixes a *missing* `-l` flag for a library whose `Lib` clause is never textually present in the module being compiled. Verified fixed with the same three-package chain, and additionally with a diamond (`app` depending on both `left` and `right`, each independently depending on `base`) - `base` is correctly linked exactly once into `app`'s final binary.
- `isStale` (M5b) extended from a single source path to a list - `ebpm run`'s staleness check now walks every package in the resolved graph (each `[lib]`'s and/or `[bin]`'s declared source path), not just the root's own, so a dependency's source changing correctly triggers a rebuild of whatever (transitively) depends on it. A missing source file anywhere in the list is treated as stale (triggering a rebuild attempt, which then surfaces `ebc`'s own clear "cannot open input file" error) rather than silently skipped - skipping it could otherwise mask a real problem (e.g. a dependency deleted out from under an otherwise still-newer binary) behind a false "nothing changed".
- New golden e2e test, `tests/e2e_pkg/lib_and_app` - **the actual M5 milestone bar**: a `mathlib` `[lib]` package and a `myapp` `[bin]` package depending on it via `path`, both built and run end-to-end via `ebpm build && ebpm run` from `myapp`'s own directory. `tests/e2e_pkg/run_case.sh` gained an optional fourth argument (which subdirectory of a multi-package fixture is the one to actually `ebpm run` from) - backward compatible, defaulting to the case directory itself for a single-package fixture like `simple_bin`. Error paths verified by hand (not yet as permanent golden tests, since asserting on exact diagnostic text this early is brittle): a 2-cycle, a 3-cycle, a dependency with no `[lib]` target, and a `git`-typed dependency - all four rejected with clean, specific messages.
- Full ctest suite (22/22, the new `e2e_pkg_lib_and_app` alongside every prior test) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, covering `ebc`, `ebpm`, and both fixture libraries) both pass with zero warnings.
- Next per the M5 breakdown: git dependencies (M5d).

## M5d Implementation Notes (git dependencies, done)

- New `pkg/src/gitdep.{hpp,cpp}`: `resolveGitDependency` clones a `git`-typed dependency into a per-URL subdirectory of a global cache (`~/.ebpm/cache/git/<sanitized-url>` - every non-alnum/`-`/`_`/`.` character replaced with `_`, deliberately a readable name rather than an opaque hash, so the cache directory stays inspectable by hand) on first use, or `git fetch`es to refresh an already-cloned one - both shelled out via the existing `ebasic::runProcess`, consistent with this project's established "shell out to an external tool rather than link a library" pattern (already used for `g++`/`clang++`). `Manifest`/`Dependency` gained no new fields beyond M5c's already-present `branch`/`tag`/`rev` - `loadManifest` gained one new check (at most one of the three may be set per dependency).
- **Real reproducibility, not just a recorded number**: `ebasic.lock`'s `commit` field (written per dependency since M5c's `ResolvedPackage::gitCommit`, but unused until now) is read back by `resolveDependencyGraph` *before* resolving any git dependency, via a new `readLockfilePins` (`lockfile.cpp`, using `toml++` to parse the lockfile this time - `writeLockfile`'s own hand-formatted `[[package]]` text turns out to already be exactly what `toml++`'s array-of-tables reader expects, needing no format change at all). If a pin exists, that exact commit is checked out - `branch`/`tag`/`rev` are only consulted on a package's *first* resolution, before any pin exists. Verified end-to-end, not just by code reading: cloned a dependency, built successfully, then pushed a *new*, deliberately-breaking commit to the same "remote" and rebuilt *without* deleting the lockfile - output stayed pinned to the original commit's result, confirming a moving branch doesn't silently drift a repeat build. Deleting `ebasic.lock` and rebuilding then correctly picked up the new commit, confirming the pin (not something else) was what held it in place.
- Checking out a remote branch needs the `origin/<branch>` form (a fresh clone has no local tracking branch yet), but a tag, a `rev`, or a pinned commit SHA are never spelled that way - an early version tried the `origin/` form unconditionally before falling back to the bare ref, which "worked" (the fallback always caught it) but printed a confusing, guaranteed-to-fail `git checkout origin/<40-char-sha>` error on every single pinned rebuild. Caught by hand-testing the pinned-rebuild path specifically (not by code review) and fixed by only attempting the `origin/` form when actually resolving `dep.branch` with no pin in play.
- **A real bug caught by hand-testing a git dependency specifically, not a git-specific design flaw**: `buildPackageWithDeps`'s transitive-dependency-closure walk (`collectTransitiveDeps`, from M5c) re-derived each dependency's directory by joining `pkg.dir` with `Dependency::path` - which is empty for a `git` dependency (only `Dependency::git` is set), so this silently collapsed to `pkg.dir` itself, meaning a package depending on a git library would (silently, no error) treat *itself* as its own transitive dependency instead of finding the real one, leaving the actual library never passed to `ebc` at all (surfaced as "cannot open included file", easy to misdiagnose as an interface-generation bug rather than a path-resolution one). Fixed by having `collectTransitiveDeps` look up each dependency edge by *name* against the already-fully-resolved `order` (a `name -> ResolvedPackage*` map) instead of re-deriving a path a second time - simpler code, and correct for both dependency kinds uniformly, since it needed no dedicated "which dependency kind is this" branch.
- New `ebasic::runProcessCaptureOutput` (`compiler/src/driver/process.{hpp,cpp}`, the `ebasic_process` shared target from M5a) - a pipe-based sibling of the existing `runProcess`, needed to read back `git rev-parse HEAD`'s stdout (its own stderr still goes to this process's, unchanged) rather than just an exit code.
- New golden e2e test, `tests/e2e_pkg/lib_and_app_git`, with a dedicated `run_git_case.sh` (distinct from `run_case.sh`, since a git-dependency fixture needs a *real* remote URL substituted in at test-run time - the bare repo's path only exists once the script creates it): creates a bare "remote" repo, seeds it from a `lib_seed/` fixture via a throwaway clone/commit/push (explicitly forcing the `master` branch name, since a bare `git init`'s default branch name depends on the machine's own git config), substitutes the real repo path into the app fixture's `ebasic.toml.in`, and runs `ebpm run` with an isolated `HOME` (so the git cache never touches the real user's `~/.ebpm` or a previous test run's) - no real network access needed anywhere in the test.
- Full ctest suite (23/23, the new `e2e_pkg_lib_and_app_git` alongside every prior test) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, covering `ebc`, `ebpm`, and both fixture libraries) both pass with zero warnings.
- Next per the M5 breakdown: `ebpm test` + final roadmap notes (M5e) - the last M5 slice.

## M5e Implementation Notes (ebpm test, done - M5 is now fully complete)

- `ebpm test` builds the package (and its dependencies) first via the existing `buildPackageWithDeps`, then compiles and runs every `tests/*.bas` file as its own standalone program, judged purely by exit code (0 = pass) - printing a per-file `test <name> ... ok`/`FAILED (exit code N)` line plus a final `test result: ok/FAILED. N passed; M failed` summary, and returning nonzero overall if anything failed. **Deliberately the simplest meaningful interpretation, not a placeholder**: the eBasic *language* itself has no assertion or explicit-exit-code primitive yet (confirmed while writing the fixture - there's no `END <code>`-style statement, unlike real FreeBASIC's legacy one), so a test file can only meaningfully "fail" by genuinely crashing (verified by hand with a deliberate integer divide-by-zero, correctly reported as `FAILED (exit code -1)` - a signal death, not a clean exit) or by failing to compile at all (also verified by hand) - no golden-output diffing, no assertion library. Both are real, motivated scope cuts (there's no language feature to build them on top of yet), not oversights, and this is easy to extend once/if the language grows real test syntax.
- New `computeConsumerDirs` (`build.{hpp,cpp}`): each `tests/*.bas` file is compiled as a **consumer** of the package under test, not as another build target of the package itself - it gets the same `-I`/`-L`/`-l` a real external dependent package would (the package's entire transitive dependency closure, reusing the existing `collectTransitiveDeps`), *plus*, if the package under test has a `[lib]` target, the package's **own** `target/` directory and name too - letting a test file `#include` and link its own package's auto-generated interface exactly like an outside consumer would, rather than needing special-cased "am I testing myself" logic anywhere.
- New golden e2e test, `tests/e2e_pkg/lib_with_tests` (a `[lib]` package with one passing `tests/*.bas` file) - `tests/e2e_pkg/run_case.sh` gained a second optional argument (which `ebpm` subcommand to run - `run` by default, `test` for this case), reusing the exact same fixture-copy/stdout-diff machinery rather than a separate script. The divide-by-zero "does a genuine test failure get reported correctly" case was verified by hand but deliberately **not** made a permanent golden test - a crash's exact exit code/signal representation isn't guaranteed portable across platforms, and asserting on it this early would be brittle (same judgment already applied to M5c's cycle-detection error text).
- **Two of my own test-authoring mistakes caught while hand-verifying this slice, neither an ebpm bug**: (1) tried to write a "failing" test using a `END 1`-style exit-code statement, discovering by the resulting parse error that this eBasic dialect has no such statement at all (confirming the "no explicit exit code primitive" scope note above is a real language gap, not an assumption); (2) a copy-pasted shell command chain set a `$SCRATCH`-style path variable without actually `cd`-ing into it (the same class of mistake flagged in M5c's own notes), twice creating stray package directories inside the real project checkout - both caught immediately via `git status` before being staged, and removed. Recorded here because it's now happened more than once in the same shape: any multi-line shell block that sets a working-directory variable must `cd` into it as its own explicit first command, never assume a prior `cd` "took."
- Full ctest suite (24/24, the new `e2e_pkg_lib_with_tests` alongside every prior test) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, covering `ebc`, `ebpm`, and both fixture libraries) both pass with zero warnings.
- **M5 is now fully complete**: TOML manifests (`[package]`/`[lib]`/`[bin]`/`[dependencies]`), `ebpm new`/`init`/`build`/`run`/`test`, path and git dependencies with a real dependency graph (cycle detection, transitive linking, diamond dependencies) and a reproducible lockfile, all shipped with golden e2e coverage - mirroring M3/M4's own completion pattern. The `[lib]`-linking design (a new `ebc --lib` mode producing a real static archive plus an auto-generated `Extern "C++"` interface file, reusing M4's interop machinery wholesale rather than building bespoke cross-package Sema/AST-importing infrastructure) was the milestone's central architectural decision, made explicitly with the user after verifying real Cargo's own separate-compilation behavior rather than assuming a simpler source-inclusion model would do. Explicitly deferred, matching the plan's own documented scope cuts: a registry, SemVer range resolution, `[workspace]` multi-package builds, multiple binaries per package, debug/release profiles, and shared (`.so`/`.dll`) libraries. Next per the phased roadmap: M6 (precompiled standard library) - not yet planned in detail.

## M6 Implementation Notes (precompiled runtime header, done)

- **Verified rather than assumed, and this reshaped the whole slice's design**: the current `ebasic-rt` runtime is tiny (83 lines across three headers, header-only) - there's no `stdlib/` of real size yet to precompile as a *linkable archive* (the "prebuilt static libs" half of the roadmap's own M6 wording). Benchmarked (not assumed) where today's compile time actually goes: an empty `g++ -c` compile takes ~0.01s; compiling a trivial program that `#include`s `runtime.hpp` plus `<cmath>`/`<cstdint>`/`<vector>` (exactly what every generated `.cpp` does) takes ~0.20-0.36s - the gap is almost entirely C++ standard-library header re-parsing on every single `ebc` invocation, not `ebasic-rt`'s own code. This pointed squarely at **PCH**, not static-lib precompilation, as the mechanism with a real, present-day payoff - confirmed with the user before implementing (prebuilt static-lib precompilation is deferred until the runtime/stdlib actually grows enough out-of-line code to benefit, avoiding speculative infrastructure for content that doesn't exist).
- Three more things verified empirically (via direct `g++`/`-H` experiments) before committing to the design, each one shaping it: (1) a plain `#include "ebasic/runtime/runtime.hpp"` with **zero special compiler flags** automatically uses a `runtime.hpp.gch` sitting next to the real header, if a matching one exists - meaning `ebc`'s own compile invocation needs no new flag at all for the *lookup* half of PCH; (2) the `.gch` can live in an entirely separate `-I` directory searched *before* the real header's directory, and the speedup still applies identically - meaning the precompiled artifact can be built into the CMake **build tree** and never touch the source tree; (3) GCC silently and safely falls back to a normal (slower, correct) compile when the `.gch` doesn't match the compiler's version/flags (confirmed with a mismatched `-std` and a mismatched `-O` level, both exit code 0) - satisfying the roadmap's "versioning checks against compiler" requirement **for free**, with no custom fingerprinting/staleness system written at all. **Confirmed with the user**: GCC only for this milestone (matches every prior milestone's Linux/GCC-first sequencing; Clang needs an entirely different mechanism - a `.pch` format plus an explicit `-include-pch` flag - and simply never looks at a stray `.gch`, confirmed harmless under `-cxx clang++`, just no speedup) - a clean, documented M8 fast-follow rather than built now.
- New CMake step (`runtime/CMakeLists.txt`): `find_program(GXX_EXECUTABLE g++)` (not `REQUIRED` - gracefully skipped with a status message, not a build failure, if no system `g++` exists, e.g. a Clang-only machine) precompiles `runtime.hpp` into `${CMAKE_BINARY_DIR}/runtime_pch/ebasic/runtime/runtime.hpp.gch`, `DEPENDS` on all three runtime headers for correct incremental rebuilds, wired as a build dependency of the `ebc` target. Deliberately invokes **literal `g++`**, not `${CMAKE_CXX_COMPILER}` - `ebc`'s own driver defaults to invoking literal `"g++"` at runtime (`main.cpp`'s `cxx = envCxx ? envCxx : "g++"`) regardless of which compiler happened to build `ebc` itself (e.g. via the `linux-clang` preset), so the PCH must match *that* invocation, a genuinely different concern from this build's own C++ toolchain - a subtle point caught by reading `main.cpp` before writing the CMake code, not after something broke.
- `ebc`'s driver gained exactly one new compile-time constant, `EBASIC_RUNTIME_PCH_DIR` (mirrors the existing `EBASIC_RUNTIME_INCLUDE_DIR`, empty string when no PCH was built), consulted by a single new helper (`runtimeIncludeArgs()`) that prepends it as an extra `-I` before the real runtime include dir - used identically in both the normal-executable and `--lib` compile paths. Deliberately never passes a bare `-I ""` when empty (which could otherwise resolve to searching the current directory) - checked explicitly rather than assumed harmless. `ebpm` needed **zero changes** - it already just shells out to `ebc`, so every `ebpm build`/`run`/`test` benefits transparently.
- Measured with a new hand-run benchmark script (`scripts/bench_pch.sh` - deliberately *not* a ctest case, since asserting on absolute wall-clock thresholds in CI is inherently flaky across machines/load, the same judgment already applied elsewhere): the isolated backend `g++ -c` step (what the PCH actually affects) drops from an average of ~0.27s to ~0.13s over 10 runs - a **60% reduction**, reproducible across repeated runs of the script. The full end-to-end `ebc <file> -o <out>` invocation (preprocessing/lexing/parsing/codegen are unaffected, fixed costs) averages ~0.18s with the PCH in effect (down from a full pipeline that previously spent much of its own time inside the now-shortened backend step).
- Verified via `g++ -H` directly (the `!` marker in its verbose header-trace output confirms a header was used *as a precompiled header*, not reparsed) that the PCH is genuinely being consulted, not just present-but-ignored.
- Full ctest suite (24/24, unchanged - this is a pure optimization, no behavior or golden-output changes anywhere) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure) both pass with zero warnings. The existing suite's own total wall-clock time dropped noticeably too (an unplanned but expected side confirmation of the same win, outside the dedicated benchmark script).
- **M6 is now complete**, scoped as a single-pass milestone (no M6a-x breakdown, unlike M3/M4/M5) since the empirical grounding narrowed the design to one clear, low-risk mechanism before any code was written. Explicitly deferred, documented rather than silently dropped: Clang PCH support (a clean M8 fast-follow), prebuilt static-library precompilation (once the runtime/stdlib has real out-of-line code worth it), and any new stdlib *content* (file I/O, extended math/string, GUI/graphics - a separate, much larger content project with no concrete request behind it yet; M6 was about the precompilation *mechanism*, not growing the library's feature surface). Next per the phased roadmap: M7 (doc-comment syntax + `docgen`) - not yet planned in detail.

## M7 Plan Summary (doc-comment syntax + docgen, in progress)

A two-slice plan (M7a-b) was written with the user before implementation started. **FreeBASIC itself has no doc-comment syntax at all** (confirmed by the roadmap's own wording), so eBasic has to define its own - grounded in real prior art rather than invented from nothing: **VB.NET's own XML documentation comments use `'''` (three apostrophes)**, a genuine precedent from a real BASIC-family dialect. This plan adopts the same *marker* but not VB.NET's heavyweight XML-tag *content* convention - doc-comment content is freeform prose (closer to Rust's `///` in spirit), since re-stating parameter types in structured tags would just duplicate what the parser already knows structurally from the real signature; structured `@param`-style tags are a natural, deliberately deferred future extension. **Architecture decided with high confidence rather than gated behind a question**, since the project's own established pattern - reusing real, already-verified compiler machinery rather than building a second, shakier one (M4's interface generator reuses `Codegen`; M5's `ebpm` reuses `ebc` wholesale) - is unusually strong and consistent precedent here: `docgen` reuses the real `Lexer`/`Preprocessor`/`Parser` (not a standalone text scanner), needing neither `Sema` nor `Codegen` since the parser already resolves every declared type structurally. Each sub-slice gets its own "M7x Implementation Notes" section below as it ships.

## M7a Implementation Notes (doc-comment syntax + shared frontend library, done)

- **Comments were previously discarded with no retention mechanism anywhere** - confirmed by reading `lexer.cpp`'s `skipSpacesAndComments()` before writing anything: a `'` comment is skipped character-by-character to end of line and never becomes a token at all (no `REM` support exists either, so there was no second comment-like construct to worry about). `preprocessor.cpp` already passes comment text through untouched during macro expansion (it stops expanding at a `'`, per `expandMacros`'s own doc comment) - so the change is entirely confined to the lexer/parser, no preprocessor changes needed at all.
- New `TokenKind::DocComment`: `skipSpacesAndComments()` now stops (returns without consuming) the moment it sees `'''` (three or more consecutive apostrophes) instead of silently skipping it like a plain `'`/`''` comment; the main `tokenize()` loop then lexes it via a new `lexDocComment()` (consumes every leading apostrophe, then the rest of the line as `text`, trimming exactly one leading separating space - matching `/// text`'s convention). A plain comment's existing behavior is completely untouched - verified by hand that a `'` line directly above a documented declaration does *not* get attached to it (proving the marker distinction actually works, not just "any comment near a declaration").
- `Stmt` (`ast.hpp`) gains `docComment` - the `"\n"`-joined text of every consecutive `'''` line immediately preceding a statement. New `Parser::collectDocComment()` (called at the top of `parseModule()`'s loop, before dispatching to `parseExternBlock`/`parseStatement`) consumes the run of `DocComment` tokens (each followed by exactly one `Newline` - a blank line inside the block breaks the run, matching "immediately preceding" rather than "somewhere above with gaps") and attaches the joined text only when the following statement's `kind` is one of `SubDecl`/`FunctionDecl`/`TypeDecl`/`UnionDecl`/`NamespaceDecl`/`Const`/`Enum` (`Parser::isDocumentableKind`) - the same "public API surface" set `Codegen::generateLibraryInterface` (M5a) already treats as exportable, for one consistent notion of "documentable" across the codebase. A doc comment before an `Extern` block, or anything else, is deliberately dropped rather than attached anywhere.
- **A real bug caught by hand-testing, not by inspection**: a doc comment with nothing left to attach to (the very last thing in a file, or immediately followed by end-of-input) fell through into `parseStatement()`, which - having no case for `TokenKind::End` - reported a spurious `"expected a statement"` for content that was never meant to be one. Fixed with an explicit check right after `collectDocComment()`: if the token stream has already reached `End`, the loop simply stops rather than dispatching. Verified with a fixture ending in an orphaned `'''` line - previously would have errored, now produces no diagnostic and no extra `Stmt` at all.
- New `ebasic_frontend` STATIC CMake target (`compiler/CMakeLists.txt`) - `diagnostics.cpp`/`lexer.cpp`/`preprocessor.cpp`/`parser.cpp`, mirroring the exact precedent M5a already set with `ebasic_process`. `ebc` links it instead of compiling those files directly into its own executable target; `docgen` (M7b) will link the same target rather than duplicating these files or reaching across `compiler/`'s own source tree. Deliberately excludes `sema.cpp`/`codegen.cpp` - `docgen` only ever needs a parsed `Module`, not a type-checked or lowered one.
- Verified by hand with a small throwaway driver (parses a fixture, prints every `Stmt::docComment`) rather than through any real product code, since no consumer exists yet in this slice: a two-line doc comment above a `FUNCTION` joins correctly with `\n`; a plain `'` comment above a different `FUNCTION` correctly produces an empty `docComment`; a `'''`-documented `TYPE` captures correctly; the orphaned trailing doc comment produces neither a diagnostic nor a stray `Stmt`.
- Full ctest suite (24/24, unchanged - this slice is purely additive, a new token kind nothing yet produces and a new empty-by-default `Stmt` field) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, now also covering the new `ebasic_frontend` target) both pass with zero warnings.
- Next per the M7 breakdown: the `docgen` tool itself (M7b) - Markdown + HTML rendering, a sample documented fixture, and the golden test that actually hits M7's "produces browsable docs" done-when bar.

## M7b Implementation Notes (docgen tool - M7 is now fully complete)

- New `docgen/` component (per the roadmap's own long-planned repo layout): `docgen/src/main.cpp` (CLI: `docgen <input.bas> -o <output-dir>`, mirroring `ebc`'s own `-o` convention) runs `Preprocessor`+`Lexer`+`Parser` only - no `Sema`, no `Codegen` - to get a real, parsed `Module`, exactly as planned; `docgen/src/render.{hpp,cpp}` turns it into `<output-dir>/index.md` and `<output-dir>/index.html`. Links the new `ebasic_frontend` target from M7a rather than duplicating any front-end source file.
- `render.cpp` groups `module.stmts` into five sections in a fixed order (Types, Constants, Enums, Namespaces, Functions/Subs), each entry rendering its name, its `docComment` (or a literal `*(undocumented)*`/`<em>(undocumented)</em>` fallback - gaps in documentation are shown, not silently hidden), and a mechanically-derived signature via a small local `basicTypeName` - a deliberate, acknowledged duplicate of `Codegen::basicTypeName` (M5a) rather than a shared extraction, since the original is a private static method on a class `docgen` has no other reason to link, and the whole function is a handful of lines either way. A free-standing helper, `isFreeProc`, excludes a `SubDecl`/`FunctionDecl` with a non-empty `ownerType` (an out-of-line `TYPE` method definition) from the Functions/Subs section - the parser happens to capture a doc comment placed above one of these just like any other top-level statement (`Parser::isDocumentableKind` only checks `kind`, not `ownerType`), so `docgen` is the layer that decides not to *surface* it, matching the plan's "v1 doesn't document TYPE members" scope cut without needing the parser itself to know about it.
- **ENUM member values are deliberately never shown** - a real, considered omission caught while writing the renderer, not an oversight: an enum member's auto-incremented/expression value is `Sema`'s job (`EnumMember::resolvedValue`), and `docgen` never runs `Sema` (by design - see M7a's plan), so it would only ever have the unresolved default for most members. Showing a plausible-looking but *wrong* value would be worse than not showing one at all.
- **HTML output is a deliberately minimal renderer, not a general CommonMark engine**, exactly as scoped: paragraph splitting (blank-line-separated blocks -> `<p>`) plus HTML-escaping for doc-comment prose, a `<pre><code>` block per signature, wrapped in a small static page with minimal inline CSS - no bold/italic/links/lists. Verified by rendering the sample fixture and visually inspecting the actual page (sent to the user directly as a demonstration of "produces browsable docs" - the concrete M7 done-when bar), not just by reading the generated markup.
- **Sample target for the done-when bar**: new `examples/documented_mathlib.bas` - a small, clearly-labeled hand-written library (a `TYPE`, a `CONST`, and three `FUNCTION`s, one left deliberately undocumented to exercise the fallback) standing in for the real stdlib that doesn't exist yet, exactly as scoped in the plan (matching M6's own precedent for the identical gap). Verified it's real, valid eBasic - not just docgen fodder - by compiling it through `ebc` directly (it has no top-level executable statements, so this only proves it parses/type-checks/lowers cleanly, which is exactly what's being claimed of it).
- New golden e2e test, `tests/e2e_docgen/simple` (own small `run_case.sh`, since `docgen`'s CLI/output shape differs from both `ebc`'s and `ebpm`'s existing harnesses) - a fixture exercising a documented `CONST`, a documented `FUNCTION`, an undocumented `FUNCTION`, and a plain `'` comment directly above the undocumented one (proving *again*, this time through the real end-to-end tool rather than the M7a throwaway driver, that a plain comment is never mistaken for a doc comment). Only the Markdown output is golden-diffed; the HTML output is hand-verified instead - exactly the plan's own reasoning (text-level HTML is more prone to trivial, non-meaningful formatting differences that would make a golden diff brittle), the same judgment already applied to other fragile-to-assert cases (M5c's cycle-detection error text, M5e's crash exit code).
- Full ctest suite (25/25, the new `e2e_docgen_simple` alongside every prior test) and a clean `-Wall -Wextra` rebuild (fresh out-of-tree configure, now also covering the new `docgen` target) both pass with zero warnings.
- **M7 is now fully complete**: a `'''` doc-comment marker grounded in VB.NET's own real prior art (M7a), and a `docgen` tool that extracts signatures + doc comments from real eBasic source - reusing the genuine compiler front end rather than a second, shakier parser - into both Markdown and a minimally-styled, genuinely browsable HTML page (M7b). Explicitly deferred, matching the plan's own documented scope cuts: structured per-parameter doc tags, full CommonMark rendering in HTML, documenting `NAMESPACE` members/`TYPE` methods/`Extern` bindings, multi-page cross-linked output, and writing a real, substantial stdlib to document (a separate, much larger content project - M7 was about the extraction/rendering *mechanism*). Next per the phased roadmap: M8 (Windows/macOS/Haiku ports) - not yet planned in detail.

## M8 Plan Summary (Windows/macOS/Haiku ports, in progress)

A five-slice plan (M8a-e) was written with the user before implementation started - the first milestone with a **fundamentally different verification posture** than M1-M7, which were always verified by directly building and running the real thing on this (Linux) machine. There's no local Windows or macOS machine, and the Haiku box (reachable only by SSH at `192.168.1.30`) has no working access yet - three real attempts (direct SSH, `BatchMode` key auth, `ssh-copy-id`) all failed (no key registered on that host, the local `ssh-agent` refuses to sign non-interactively, no `ssh-askpass` available in this environment). **Per the user's explicit direction, the plan proceeds without live Haiku verification**, tracked as an open gap (M8e) rather than worked around. The codebase's only genuinely POSIX-specific code, found by grepping rather than assumed, is `compiler/src/driver/process.cpp` (`fork`/`execvp`/`waitpid`/`pipe`) and one `std::getenv("HOME")` call in `pkg/src/gitdep.cpp` - everything else (`std::filesystem::path` composition throughout, the `CXX`/`EBC` env-var conventions, the M6 PCH mechanism) is already portable. **Confirmed with the user**: the Windows port targets MinGW first, not MSVC, keeping the amount of untestable new driver code small (MSVC needs a genuinely different toolchain-abstraction layer, deferred as a separate follow-on). The plan leans on GitHub Actions CI (already authenticated via `gh`) as the real verification mechanism for Windows/macOS once pushed (M8d), with Haiku's real path being registration as a self-hosted runner once SSH access exists (M8e).

## M8a Implementation Notes (cross-platform process abstraction + path/env portability, done)

- `compiler/src/driver/process.cpp` gained a `#ifdef _WIN32` implementation of both `runProcess` and `runProcessCaptureOutput` using `CreateProcessA`/`WaitForSingleObject`/`GetExitCodeProcess` (plus `CreatePipe`+`STARTUPINFO.hStdOutput` for capture), alongside the existing POSIX `fork`/`execvp`/`waitpid`/`pipe` implementation unchanged in an `#else` branch. Windows has no `execvp`-style argv-array API - `CreateProcess` takes a single command-line string that the child's own CRT re-splits - so a `quoteArgvArgument` helper builds it by hand, following the documented Windows argument-quoting algorithm exactly (every run of backslashes doubled when it immediately precedes a literal quote or the argument's end, so the closing quote added around the whole argument is never itself escaped) - a well-known source of subtle bugs if done casually.
- `pkg/src/gitdep.cpp`'s `gitCacheRoot()` now falls back to `USERPROFILE` when `HOME` isn't set (Windows doesn't guarantee `HOME`, unlike POSIX) before falling back further to `"."`, unchanged. No other file needed any change at all - confirmed by grepping the whole codebase for `fork`/`execvp`/`waitpid`/`sys/wait.h`/`unistd.h`/other `getenv` calls/hardcoded `/tmp`-style paths, rather than assumed clean.
- **A real, unplanned upgrade to this slice's verification**: the plan expected the Windows branch to be entirely unverifiable until CI (M8d). This sandbox turned out to already have `g++-mingw-w64-x86-64` (a real MinGW-w64 cross-compiler) *and* Wine installed - discovered by checking rather than assuming there was nothing to work with. This made it possible to cross-compile the new Windows branch and actually **run it under Wine**, right now, rather than waiting for a CI round-trip: verified `runProcess` correctly launches a real child process and returns its real exit code (`cmd.exe /c exit 42` -> `42`); verified `runProcessCaptureOutput` correctly pipes a child's stdout back; and - the highest-risk part of this whole slice - verified the argument-quoting algorithm with a genuine argv round-trip test (a small MinGW-compiled `argc`/`argv`-printing helper program, invoked with a plain word, a word containing a space, a word containing embedded quotes, an empty string, a trailing backslash run, and a backslash-immediately-before-a-quote combination) - **every one of the 7 arguments came back byte-for-byte identical** to what was passed in. This is substantially stronger evidence than "reasoned about carefully" for the one piece of this slice that genuinely worried me going in.
- Full existing e2e suite (25/25) still passing on Linux (the `#ifdef _WIN32` branch is inert there, so this the pure regression check the plan called for) and a clean `-Wall -Wextra` rebuild, zero warnings.
- Next per the M8 breakdown: MinGW toolchain scoping in `ebc`'s own driver logic (M8b).

## M8b Implementation Notes (MinGW toolchain scoping, done - no code changes needed)

- The plan expected this slice to need "little or no code change," with its real job being to confirm that rather than assume it - it turned out to need **zero** code changes, but only after a genuine, worthwhile scare along the way.
- **A real bug was nearly shipped here, caught by re-verifying rather than trusting a first result**: `ebpm`'s `binaryPath()`/`ebc`'s own `-o` output path are bare names with no extension (e.g. `"myapp"`), and MinGW's `g++` always produces `myapp.exe`. An initial test - cross-compiling a small program and running `ebasic::runProcess({"hello_noext"})` under Wine, where only `hello_noext.exe` existed on disk - returned `-1` (failed to launch), seemingly confirming that `CreateProcess` (unlike a shell) doesn't resolve a bare name to its `.exe` file automatically. This looked like a real, load-bearing bug: every `ebpm run`/`ebpm test` invocation would fail on Windows the moment it tried to execute what it just built. A fix (`executableSuffix()`/`withExecutableSuffix()` in the shared `process` module, applied to `ebc`'s own `-o` argument and to `binaryPath()`) was written and was about to be committed.
- Before committing, the failing test was **re-run from scratch in one clean, uncontaminated shell invocation** (the original had touched several directories across separate tool calls in this environment, where the working directory resets between calls - the same class of mistake already flagged twice in this project's own M5c/M5e notes) - and it **passed**: `runProcess("hello_noext")` correctly found and ran `hello_noext.exe` in the same directory. A follow-up test with a binary reachable only via `PATH` (not the current directory) also passed. Real Windows documentation confirms this is expected `CreateProcess` behavior after all: when resolving via the command line's first token (rather than a separate `lpApplicationName`), `.exe` is appended automatically if the name has no extension, and the same directory/PATH search order a shell would use is followed.
- **The now-proven-unnecessary fix was reverted rather than kept "just in case"** - `git diff --stat` confirms the tree is back to exactly M8a's committed state, zero net change. Shipping unused, speculative code because it's "harmless" would have contradicted this project's own standing discipline against unneeded complexity; better to revert cleanly once the premise it was built on turned out to be false.
- With that settled, the actual audit this slice set out to do has nothing left to find: MinGW's `g++`/`ar`/`ar rcs` accept the exact same flags and tool names `ebc`/`ebpm`/`gitdep.cpp` already invoke for Linux, `CreateProcess`'s automatic `.exe`-resolution covers every bare tool/output name already hardcoded in the codebase (`g++`, `ar`, `git`, the `ebc` fallback name), and no `.exe`-suffix bookkeeping needs to exist anywhere. Confirmed, not assumed, via real cross-compiled-and-Wine-run tests rather than documentation alone.
- Full existing e2e suite (25/25) still passing and a clean `-Wall -Wextra` rebuild, zero warnings - unaffected either way, since there was ultimately no change to verify against.
- Next per the M8 breakdown: `CMakePresets.json` expansion (M8c).

## M8c Implementation Notes (CMakePresets expansion, done)

- Three new presets in `CMakePresets.json`, mirroring the existing `linux-gcc`/`linux-clang` shape exactly: `windows-mingw` (`g++`/`gcc`, `Unix Makefiles` - deliberately *not* the `MinGW Makefiles` generator, since the real target environment is an MSYS2 `mingw64` bash shell in CI, which behaves like any other POSIX-ish `Unix Makefiles` build, not a native Windows `cmd`/PowerShell + `mingw32-make.exe` setup), `macos` (`clang++`, matching `linux-clang`'s own reasoning - macOS's system compiler *is* Clang), `haiku` (`g++`, matching `linux-gcc`'s shape - Haiku's traditional system compiler). No new CMake *logic* needed anywhere, confirmed - every existing step (including the M6 PCH mechanism's own graceful `g++`-not-found fallback) already generalizes across presets by construction.
- **Verified far beyond what this slice's own plan called for** (which expected only a `cmake --preset <name> -N` structural dry-run): this sandbox's MinGW-w64 cross-compiler and Wine (found during M8a) made it possible to actually cross-compile the **entire project** - `ebc`, `ebpm`, and `docgen` all built cleanly with zero errors, including the M6 PCH mechanism (a `.gch` built successfully via the cross `g++` too). Going further, the resulting Windows binaries were **run under Wine**, not just compiled: `docgen.exe` correctly parsed a real fixture and produced correct Markdown output; `ebpm.exe new` correctly scaffolded a package on a real (emulated) Windows filesystem; `ebc.exe`'s entire front end (preprocessing/lexing/parsing/Sema/Codegen) produced byte-identical `.gen.cpp` output to the Linux build, and correctly, gracefully reported a clean error when its final backend-compile step had no native Windows `g++` available to invoke (expected in this sandbox - MSYS2 in CI, M8d, provides a real one). This is genuine behavioral verification of nearly the entire Windows port, not just "it compiles" - done with a throwaway, uncommitted CMake configuration (cross `-DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++`), separate from the checked-in `windows-mingw` preset itself (which targets the real, native MSYS2 environment CI actually uses, not this sandbox's cross-compilation shortcut).
- The `macos` and `haiku` presets could only be checked structurally on this machine (`clang++` isn't installed here at all - the same pre-existing gap `linux-clang` already had, not something newly introduced; there's obviously no native Haiku toolchain either) - real verification for both is deferred to CI (`macos`, M8d) and, eventually, a self-hosted runner (`haiku`, M8e).
- `README.md` updated to mention the new presets and to fix its own stale "current milestone (M0)" status line, noticed while touching the build instructions - a small, low-risk drive-by correction alongside the actual scope of this slice.
- Full existing e2e suite (25/25, `linux-gcc`) still passing and a clean `-Wall -Wextra` rebuild, zero warnings.
- Next per the M8 breakdown: the GitHub Actions CI matrix (M8d) - the real verification mechanism for `macos`/`windows-mingw` going forward.

## M8d Implementation Notes (GitHub Actions CI matrix, done)

- New `.github/workflows/ci.yml`: four jobs - `linux-gcc`/`linux-clang` (`ubuntu-latest`), `macos` (`macos-latest`), `windows-mingw` (`windows-latest`, via `msys2/setup-msys2` in a `MINGW64` environment - matches the `windows-mingw` CMake preset's own design, `Unix Makefiles` + plain `g++`/`gcc`, not a native `cmd`/PowerShell setup). Haiku deliberately excluded - no GitHub-hosted image exists (see M8e). Validated locally with `actionlint` (a real, authoritative GitHub Actions linter - installed via `go install` since it wasn't already present, rather than eyeballing the YAML) before ever pushing - zero issues reported both times the workflow changed.
- **Two real, distinct bugs were caught by checking actual CI run results, exactly as this milestone's whole approach was designed to do** - the first real end-to-end test of "does CI verification actually work" for this project:
  1. The first real run: `linux-gcc`, `linux-clang`, and `macos` **all passed cleanly on the very first try** - a strong, welcome confirmation that the portability work in M8a-c was sound. `windows-mingw` failed all 25 tests with `[inappropriate file type or format]`/`BAD_COMMAND`/`Process not started`. Cause: every `add_test(COMMAND <script>.sh ...)` relied on the script's shebang line (`#!/usr/bin/env bash`) plus its executable bit for the OS to know how to run it - real, standard behavior on POSIX (the kernel itself parses the shebang), but native Windows process creation has no shebang support at all and simply can't interpret a text file as directly executable. Fixed by changing every `add_test` in `tests/CMakeLists.txt` to invoke `bash <script>.sh` explicitly instead of relying on the bare script path - a single `replace_all` edit across all 17 identical `COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/` prefixes, confirmed harmless on Linux (25/25 still passed locally before pushing the fix).
  2. The second run: `windows-mingw` progressed much further (each test script now genuinely executed, ~0.5s each, real compile+run attempts) but every one failed with `diff: command not found`. Cause: MSYS2's `mingw64` environment doesn't include GNU `diffutils` by default, and every `run_case.sh` uses `diff -u` to compare actual vs. expected output. Fixed by adding `diffutils` to the `msys2/setup-msys2` install list.
  3. The third run: **all four jobs passed** - `linux-gcc`, `linux-clang`, `macos`, and `windows-mingw` all green, the full 25-test e2e golden suite passing on every platform this slice targets. This is the actual, literal "full e2e golden suite passes in CI" bar from M8's own done-when criterion, for three of the four platforms (Haiku remains the explicitly-tracked M8e gap).
- Neither bug was specific to anything unique about *this* project's own code - both were generic "a Linux-first test harness assumed a POSIX-only convention (shebang execution, GNU coreutils availability) that doesn't hold on Windows" gaps, exactly the class of issue this milestone's CI-based verification approach exists to surface cheaply, in a few round-trips, rather than discover much later from a real Windows user's bug report.
- Full local ctest suite (25/25, `linux-gcc`) still passing throughout, confirming neither fix changed any POSIX-side behavior.
- Next per the M8 breakdown: Haiku - self-hosted CI runner registration + a best-effort packaging draft (M8e), blocked on SSH access per the user's own explicit direction.

## M8e Implementation Notes (Haiku: live verification, relocatable install, packaging draft - M8 is now fully complete)

- **The SSH-access blocker named in M8's plan got resolved mid-milestone**, once the user offered to place a password in a file outside the repo (the scratchpad directory was used instead of a new gitignored secret file, avoiding touching the repo at all). Getting from "a password in a file" to real, working passwordless key auth took several genuine wrong turns, each one real and worth recording rather than glossing over: no `ssh-askpass` in this environment blocked both `ssh-copy-id` and any interactive password prompt (worked around with a Python venv + `paramiko`, password auth, `allow_agent=False`/`look_for_keys=False` - the local `ssh-agent`'s own RSA key caused failures even through paramiko otherwise); a copied private key had insecure `0444` permissions (fixed: `chmod 600`); RSA auth failed against a newer OpenSSH `publickey-hostbound-v00@openssh.com` extension; a fresh ED25519 key *still* didn't fix it (a different symptom - the server didn't even offer to accept it). **The actual root cause, found only after the user supplied the exact `sshd_config` path** (`/boot/system/settings/ssh/sshd_config`): Haiku's sshd sets `AuthorizedKeysFile config/settings/ssh/authorized_keys` - a non-standard location relative to home (`~/config/settings/ssh/authorized_keys`, not `~/.ssh/authorized_keys`). Writing the public key there fixed it immediately.
- With real SSH access established, `scripts/haiku_verify.sh` was written: transfers the current commit via `git archive HEAD | ssh haiku tar -x`, configures/builds/tests via the `haiku` CMake preset, reports pass/fail, cleans up its remote scratch directory either way. **Ran successfully end-to-end on genuine Haiku hardware (hrev59901, GCC 13.3.0): 25/25 e2e tests passed** - the actual, literal M8 done-when bar, now met for real on the fourth platform, not assumed from documentation. (`git archive`'s use over a plain `git clone` was itself a real workaround: the GitHub mirror was still private at the time, so cloning would have needed credentials on the remote box; the user made the repo public shortly after, but the archive approach had already worked and needed no revisiting.)
- **The plan's assumption that Haiku could join the CI matrix as a self-hosted GitHub Actions runner turned out to be false**, discovered by checking rather than assuming: `gh api /repos/.../actions/runners/downloads` lists only osx/linux/win x64/arm runner binaries (no Haiku build exists), and Haiku has neither dotnet nor mono to run one manually either. Surfaced to the user directly rather than silently downgrading scope or declaring the gap unfixable; offered a choice between building a from-scratch custom runner/status-reporting alternative or simply keeping `scripts/haiku_verify.sh` as a **rerunnable, manually-invoked script** (no permanent CI integration). The user chose the script-only path - it already does everything a CI job would (build + full e2e suite + clear pass/fail), just without GitHub's own UI showing it, which is an acceptable and honest trade-off for a platform with no first-party runner support.
- **A second, more consequential gap surfaced while drafting the HaikuPorts recipe, not anticipated in the original plan**: the project had no CMake `install()` rules anywhere, and even if it did, `ebc`'s runtime header/PCH `-I` paths were hardcoded absolute build-tree paths (`EBASIC_RUNTIME_INCLUDE_DIR`) that wouldn't exist post-install - a `haikuporter` `INSTALL()` step would have nothing real to do, and the installed `ebc` wouldn't be able to compile anything even if files were copied by hand. This was real, cross-cutting scope beyond "draft a packaging recipe," surfaced to the user rather than worked around quietly or left as a known-broken recipe; the user chose "fix it now."
- **Fix**: `install(TARGETS ebc/ebpm/docgen ...)` into `${CMAKE_INSTALL_BINDIR}`, and `install(DIRECTORY ...)` for the runtime headers/PCH into `${CMAKE_INSTALL_DATADIR}/ebasic/runtime/{include,pch}` (`runtime/CMakeLists.txt`). `ebc` (`main.cpp`) gained `resolveOwnExecutablePath()` (resolves its own path from `argv[0]`, handling both a path-with-separator via `fs::canonical` and a bare PATH-searched name via a manual `PATH` scan - needed since introspecting *this* process's own launch path isn't something `argv[0]` alone reliably gives) and a rewritten `runtimeIncludeArgs()` that checks for an installed runtime relative to its own executable directory (`EBASIC_RUNTIME_INSTALL_RELDIR`, a `../<datadir>/ebasic/runtime` relative path computed at CMake configure time) before falling back to the original compile-time-baked build-tree path - so every existing dev/test workflow (running `ebc` straight from the build tree) is entirely unaffected.
- **Executable-relative resolution was chosen deliberately over Haiku's own native `find_directory()`/XDG Base Directory APIs** (raised by the user mid-turn) specifically because it needs zero platform-specific runtime code and works uniformly across all four target platforms, while still fully honoring Haiku's own real directory convention at the CMake level: `GNUInstallDirs`'s generic `CMAKE_INSTALL_DATADIR` default (`share`) is wrong for Haiku (confirmed on real hardware - `/boot/system/share` exists but is empty, while `/boot/system/data` holds every other installed package's own data, e.g. CMake's own `CMAKE_ROOT`; `GNUInstallDirs.cmake` itself has zero Haiku-specific handling, confirmed by grepping it directly on the box), so the `haiku` CMakePresets.json preset now overrides `CMAKE_INSTALL_DATADIR` to `data` explicitly.
- **Verified at every layer, not assumed**: locally on Linux, the existing build-tree workflow still passes 25/25 unchanged, and a real `cmake --install <builddir> --prefix <tmp>` was performed, followed by running the *installed* `ebc` with the source `runtime/include/` tree renamed away entirely - it still worked, proving the installed layout is genuinely used rather than silently falling back. The exact same install+run test was then repeated live over SSH on the real Haiku box: `cmake --install` produced `data/ebasic/runtime/{include,pch}/...` (confirming the Haiku-specific override took effect for real, not just in a preset file), and the installed `ebc` compiled and ran a real program correctly from that installed prefix.
- A draft HaikuPorts recipe (`packaging/haiku/ebasic-0.1.0.recipe`) was written against the real recipe-format conventions (cross-checked against the actual, current `app-arch/brotli/brotli-1.1.0.recipe` in the box's own populated haikuports tree) and **checked with `haikuporter --lint`** (temporarily copied into a scratch `dev-lang/ebasic/` location inside the real haikuports tree, linted, then removed - non-destructive) rather than trusted by eye - it passed cleanly. `SOURCE_URI`/`CHECKSUM_SHA256` are left as explicit placeholders since no tagged release exists yet; the recipe is otherwise real and buildable once one does.
- Full existing e2e suite (25/25, `linux-gcc`) still passing throughout, and a clean `-Wall -Wextra` rebuild with zero warnings on the new `main.cpp` code.
- **M8 is now fully complete**: the full e2e golden suite passes on all four targeted platforms - `linux-gcc`/`linux-clang`/`macos`/`windows-mingw` in GitHub Actions CI (M8d), and Haiku verified live on real hardware via `scripts/haiku_verify.sh` (M8e) - the exact done-when bar stated in the original phased roadmap. No further milestone is currently planned past M8.

## Version reporting + -h/--help Implementation Notes (post-M8, done)

A standalone CLI-ergonomics addition (not part of the phased roadmap table above - that table tracks the original FreeBASIC-compatibility arc, not every follow-on feature): `ebc`, `ebpm`, and `docgen` previously had no `-v`/`--version` at all, and `docgen` had no real flag parsing (`argc != 4`), just `ebc`/`ebpm`'s existing usage-on-error path.

- `project(ebasic VERSION 0.1.0 ...)` (top-level `CMakeLists.txt`) is now the single canonical version, plus a best-effort short git commit hash detected at configure time (`execute_process(... git rev-parse --short HEAD ...)`, guarded on `GIT_FOUND` and a real `.git` directory existing) - **confirmed with the user**: report version+hash (e.g. `ebc 0.1.0 (a37c8bf)`), not a bare version, since this project's own verification discipline spans a 4-platform CI matrix plus live Haiku hardware, where knowing exactly which commit a given binary corresponds to is genuinely useful. Both are baked into a generated `ebasic/version.hpp` (`configure_file` from a new `compiler/src/driver/version.hpp.in` template into `${CMAKE_BINARY_DIR}/generated/ebasic/version.hpp`, mirroring the existing `ebasic/runtime/*.hpp` include-namespace convention) - header-only, included directly by all three binaries, no new library target needed.
- `ebc` (`compiler/src/driver/main.cpp`): `-v`/`--version`/`-h`/`--help` added as new flag-position branches inside the existing `parseArgs` loop, not a separate blind scan of argv - checked, live: `ebc -l -h` still correctly consumes `-h` as `-l`'s value, not a help request.
- `ebpm` (`pkg/src/main.cpp`): checked on the *leading* `args[0]` token only, before the `new`/`init`/`build`/`run`/`test` dispatch - deliberately not a scan of the whole argv, since `ebpm run -- --help` must keep forwarding `--help` to the user's own program via ebpm's existing `-- args...` passthrough, not be swallowed as ebpm's own help.
- `docgen` (`docgen/src/main.cpp`): the one genuine small refactor in this slice - its previous `argc != 4` rigid check had no flag parsing at all, replaced with a small `parseArgs` loop mirroring `ebc`'s shape (`-o <dir>`, `-v`/`--version`, `-h`/`--help`, one positional input path). Verified its existing real functionality (compiling `examples/documented_mathlib.bas` to Markdown/HTML) still works unchanged after the refactor.
- All three print version/help to **stdout** and exit 0, distinct from the pre-existing error path (still stderr, exit 1).
- New `tests/cli/version_help.sh`, wired into `tests/CMakeLists.txt` as `cli_version_help`, checks all 8 flag/tool combinations (both spellings x 3 tools) - full suite is now 26/26.
- **Per the user's explicit request, verified on real Haiku hardware for this change too, not just Linux**: `scripts/haiku_verify.sh` ran 26/26 passing on the real box. One thing specifically worth checking live rather than assuming: `scripts/haiku_verify.sh` deploys via `git archive HEAD | ssh haiku tar -x`, whose extracted tree has no `.git` directory at all - confirmed live that this degrades gracefully exactly as designed: the generated `version.hpp` on the Haiku box has an empty `kGitHash`, and `ebc --version` there correctly prints `ebc 0.1.0` with no parenthetical, rather than failing or fabricating a hash.
- Full existing e2e suite (26/26, `linux-gcc`) passing and a clean `-Wall -Wextra` rebuild, zero warnings.

## Documentation Plan Summary (developer/reference/guide/README, in progress)

A six-slice plan (Doc-1 through Doc-6), following the exact same slice-per-milestone workflow as every prior phase. Prompted by the project being functionally complete (M0-M8 plus post-M8 CLI ergonomics) while documentation stayed thin: `README.md` was stale (claimed M8 still in progress), `docs/` had only this chronological planning log plus Doxygen config (no narrative developer overview, no language reference, no end-user guide), and `examples/` had only 2 files. **Confirmed with the user**: the language reference is an exhaustive, dictionary-style per-keyword entry (not a lighter topic-only narrative) - chosen for completeness at the cost of more writing/upkeep. Scope: `docs/developer/` (current-state architecture, linking to Doxygen rather than duplicating it - this log stays as the historical record, untouched), `docs/reference/` (one file per feature area, each a dictionary of that area's keywords, plus an alphabetical keyword index), `docs/guide/` (getting-started + one page per tool + substantially expanded `examples/`), and a refactored `README.md`. Every reference/guide code example is actually compiled and run while being written, not written from memory.

## Doc-1 Implementation Notes (developer docs, done)

- New `docs/developer/architecture.md`: the compile pipeline (Preprocessor -> Lexer -> Parser -> Sema -> Codegen -> backend), a directory/component map, why `ebasic_process`/`ebasic_frontend` exist as shared static libs (reused by `ebpm`/`docgen` respectively, avoiding duplicated subprocess-handling/parsing code), the M6 PCH mechanism (GCC-only `.gch`, PCH-shadow `-I` trick, graceful Clang/mismatch fallback), the M8e relocatable-install design (`resolveOwnExecutablePath`/`runtimeIncludeArgs`, `EBASIC_RUNTIME_INSTALL_RELDIR`, Haiku's `CMAKE_INSTALL_DATADIR` override), and how `ebpm` reuses `ebc --lib` mode rather than a separate cross-package pipeline.
- Every factual claim (file paths, function names, exact CMake variable names) checked directly against the current source (`compiler/CMakeLists.txt`, `runtime/CMakeLists.txt`, `pkg/src/build.cpp`) rather than written from memory of earlier milestones' notes, which could have drifted.
- Deliberately does not duplicate per-symbol API detail - links to the Doxygen `docs` target instead (`cmake --build build/linux-gcc --target docs`), and to this roadmap log for historical/decision context.
- Verified: `docs` Doxygen target still builds clean (unaffected, as expected for a pure Markdown addition); full e2e suite still 26/26.
- Next: `docs/reference/` - types/literals, operators, control-flow (Doc-2).

## Doc-2 Implementation Notes (language reference: types/literals, operators, control-flow, done)

- New `docs/reference/types-and-literals.md`, `docs/reference/operators.md`, `docs/reference/control-flow.md` - dictionary-style entries per the user's confirmed exhaustive scope, each keyword/construct with exact syntax, semantics, and a runnable example.
- **Every example actually compiled and run** (not written from memory) - caught two real inaccuracies before they made it into the docs: (1) an assumed `1 & 2` numeric-concat example fails a real Sema check (`&` requires both operands already be `STRING` - no implicit numeric-to-string conversion, unlike some BASIC dialects); (2) a bare `Greet()` call-as-statement doesn't parse at all - `CALL Greet()` is required. Both corrected in the docs to match real behavior rather than assumed FreeBASIC-style convenience.
- The full 14-level operator precedence table was derived from the parser's actual recursive-descent call chain (`compiler/src/parser/parser.hpp`'s `parseXor -> parseOr -> parseAnd -> parseNot -> parseRelational -> parseConcat -> parseAdditive -> parseShift -> parseMod -> parseIDiv -> parseMulDiv -> parseNegate -> parsePow -> parseUnaryPtrOps -> parsePrimary`), then spot-verified live rather than trusted from the chain alone: `-2 ^ 2` = `-4` (confirms `^` binds tighter than unary `-`), `2 ^ -1` = `0.5` (confirms `^`'s own right operand can start with unary `-`), `2 ^ 2 ^ 3` = `256` not `64` (confirms `^` is right-associative), `7 MOD 2 + 1` = `2` (confirms `MOD` binds tighter than `+`), `1 OR 0 AND 0` = `1` (confirms `AND` binds tighter than `OR`), `3 > 2 AND 1 > 5` = `0` (confirms relational binds tighter than `AND`).
- `DO`/`LOOP`'s four independent pre-test/post-test x WHILE/UNTIL combinations were all individually compiled and run (`DO WHILE`/`DO UNTIL` pre-test, `LOOP WHILE`/`LOOP UNTIL` post-test) rather than assuming symmetry from the two already-e2e-tested forms.
- Verified: `docs` Doxygen target unaffected; full e2e suite still 26/26.
- Next: `docs/reference/` - procedures, arrays, TYPE/OOP, namespaces/pointers/unions (Doc-3).
- **Post-commit fix**: writing Doc-3's operator-overload example (grounded in `tests/e2e/operators/input.bas`, which uses `v1.x = 1 : v1.y = 2`) surfaced a real gap in Doc-2's `control-flow.md` - the `:` statement separator had no entry at all. Verified live (`:` is lexed as a Newline-kind token, so it separates statements anywhere a newline would) and added as its own small entry, in a follow-up commit rather than amending the already-pushed one.

## Doc-3 Implementation Notes (language reference: procedures/arrays, TYPE/OOP, namespaces/pointers/unions, done)

- New `docs/reference/procedures-and-arrays.md` (`SUB`/`FUNCTION`, `BYVAL`/`BYREF`, `EXIT SUB`/`EXIT FUNCTION`, scoping/shadowing, `REDIM`/`REDIM PRESERVE`), `docs/reference/type-oop.md` (`TYPE` fields, memberwise-copy assignment, `Constructor`/`Destructor`, methods + `This`, `EXTENDS` + `Virtual`/`Override` dispatch + `Base.Method()`, `PROPERTY`, free-standing operator overloading), `docs/reference/namespaces-pointers-unions.md` (`NAMESPACE` incl. reopening, `PTR`/`@`/`*`/`->` incl. pointer arithmetic, `UNION`).
- Every example - including several combined/adapted from `tests/e2e/{sub_function,redim,type_records,classes,inheritance,properties,operators,namespace,pointers,unions}/input.bas` rather than copied verbatim - was compiled and run as a whole (not just visually cross-checked against the original test), catching nothing further wrong this time but confirming the adaptations (renamed variables, trimmed-down snippets, recombined into single files) stayed correct.
- Explicit, Sema-verified restrictions stated precisely rather than assumed: `REDIM` on a fixed-size array is a real error (`'<name>' is a fixed-size array and cannot be REDIM'd`); operator overloads are free-standing only (no member-declared form yet) and require at least one operand to be a user-defined `TYPE`; `PROPERTY` requires both a getter and setter of the same type; `UNION` rejects `STRING` (directly or nested) and any field with a constructor/destructor; `NAMESPACE` nesting is not supported.
- Verified: `docs` Doxygen target unaffected; full e2e suite still 26/26.
- Next: `docs/reference/` - `EXTERN`/C-C++ interop, doc-comments, and the keyword index (Doc-4).
- **Caught two more real gaps while grounding Doc-4's examples, both fixed as small additions rather than left out**: `PRINT` itself had no dictionary entry anywhere in Doc-2/Doc-3 despite appearing in literally every example - added to `control-flow.md`, and live verification turned up a genuinely worth-stating detail (`PRINT "a", "b", 1, 2` -> `ab12`, no separator inserted between comma-separated arguments at all, unlike some BASIC dialects' space/tab/column-align convention). Also added a dedicated `CALL` entry to `procedures-and-arrays.md` (previously only mentioned inline under `SUB`/`FUNCTION`) - both real, exhaustiveness gaps the user's chosen dictionary-style scope calls for filling in wherever found, not just in the slice originally planned to cover them.

## Doc-4 Implementation Notes (language reference: EXTERN interop, doc-comments, keyword index, done)

- New `docs/reference/extern-interop.md` (`Extern "C"`/`Extern "C++"` blocks, standalone `Declare` with the verified real clause order `Name [Cdecl] [Lib] [Alias] (params) As type` - params come *after*, not before - `Alias`, nested `Namespace` inside `Extern "C++"`, `ZSTRING` auto-marshaling, opaque "handle" `TYPE`s), `docs/reference/doc-comments.md` (`'''`, precisely what "no blank line tolerated" actually means - verified live that a blank line anywhere from the first `'''` line through the documented declaration is a hard parse error, not a silent split into two comments), and `docs/reference/index.md` (a full alphabetical keyword/symbol index).
- **A real grammar-order bug caught by testing**: an initial standalone-`Declare` example placed `Lib "name"` after the parameter list (`Declare Function f(...) AS INTEGER Lib "name"`) - a real parse error (`expected end of statement`). Real FreeBASIC's (and this compiler's) actual grammar requires `Cdecl`/`Lib`/`Alias` *before* the parameter list; fixed and re-verified.
- The keyword index's anchor links were generated programmatically (a small script reimplementing GitHub's actual heading-slug algorithm: lowercase, strip non-word/space/hyphen characters, collapse whitespace to single hyphens) rather than hand-guessed - a link-check pass across every `docs/reference/*.md` file (verifying every relative link's target file exists and every `#anchor` matches a real computed heading slug) then caught and fixed 9 real hand-written anchor typos elsewhere in Doc-2/Doc-3/Doc-4 (mostly doubled hyphens, e.g. `#redim--redim-preserve` instead of the real `#redim-redim-preserve`) that had gone unnoticed until this systematic check - exactly the kind of thing "looks right, isn't" that's cheap to verify and expensive to leave wrong in a reference meant for lookup.
- Two links to Doc-5's not-yet-written guide pages (`../guide/ebpm.md`, `../guide/docgen.md`) are the only remaining "broken" links reported by the checker - expected forward references, to be verified for real once Doc-5 lands (Doc-6's final link-check pass covers this).
- Verified: `docs` Doxygen target unaffected; full e2e suite still 26/26.
- **Next: `docs/guide/` - getting-started, one page per tool, expanded `examples/` (Doc-5)**, then the `README.md` refactor + final full link-check pass (Doc-6).

## Doc-5 Implementation Notes (end-user guide: getting-started, tool pages, expanded examples, done)

- New `docs/guide/getting-started.md`, `docs/guide/ebc.md`, `docs/guide/ebpm.md`, `docs/guide/docgen.md`. Every command shown was actually run, not written from memory - `ebpm new`/`build`/`run`/`test` were run live end-to-end against copies of real test packages (`tests/e2e_pkg/lib_and_app`, `tests/e2e_pkg/lib_with_tests`) to capture genuine output, including the actual `ebasic.lock` format produced by a real build (`[[package]] name/path` entries).
- `examples/` grew from 2 files (`hello.bas`, `documented_mathlib.bas`) to 9 - one small, runnable program per reference topic area (`types_and_literals`, `operators`, `control_flow`, `procedures_and_arrays`, `type_oop`, `namespaces_pointers_unions`, `extern_interop`), each compiled and run to confirm correctness, and each additionally run through `docgen` to confirm it also parses cleanly there.
- `extern_interop.bas` deliberately binds to real C standard library functions (`abs`, `atoi`) rather than the project's own internal test fixtures (`ebfixturec`/`ebfixturecpp`), since a public-facing example shouldn't depend on build-tree-only libraries a real user would never have. One candidate function (`strlen`) was tried first and rejected after it produced a real compiler warning (`declaration ... conflicts with built-in declaration` - `strlen` actually returns `size_t`, not `INTEGER`, so the `Declare`d signature technically mismatches GCC's own builtin knowledge of it) - swapped for `atoi` (a real, clean `const char* -> int` signature) instead of shipping an example that warns on its own recommended usage.
- A small link-checker script (reused from Doc-4, extended to walk the whole `docs/` tree) confirmed every relative link across all `docs/**/*.md` files now resolves, including the two forward-references to this slice's own guide pages that were expected-broken as of Doc-4.
- Verified: `docs` Doxygen target unaffected; full e2e suite still 26/26.
- Next: `README.md` refactor + a final full link-check pass (Doc-6).

## Doc-6 Implementation Notes (README refactor + final link-check pass, done - documentation plan complete)

- `README.md` rewritten as a short, accurate front door: fixed the stale status line (was still "M0 through M7 are complete... M8 is in progress" - both M8 and the post-M8 CLI-ergonomics work have been done for a while), fixed three pre-existing typos noticed while touching the file ("extented", "programmming", "sythax" x2), and replaced most of the inline detail with links to the five new documentation surfaces (developer docs, language reference, end-user guide, examples, Doxygen API docs) plus this roadmap log - a good README points to detailed docs rather than containing all of it inline.
- **Final link-check pass**: the link-checker script (built in Doc-4, extended through Doc-5) run across the entire project (`README.md` plus all of `docs/**/*.md`) - 17 markdown files, every relative link and `#anchor` verified to resolve to a real file/heading. Zero real breaks found; the only two remaining "broken" reports are a limitation of the checker itself (it only checks files, not directories) against two legitimate links to the `examples/` directory.
- Verified: `docs` Doxygen target unaffected; full e2e suite still 26/26.
- **The documentation plan (Doc-1 through Doc-6) is now complete**: `docs/developer/` (current-state architecture), `docs/reference/` (8 files - exhaustive, dictionary-style keyword reference plus an alphabetical index), `docs/guide/` (getting-started + one page per tool), `examples/` (9 runnable programs, one per feature area), and a refactored `README.md`. Every single code example across the entire effort was compiled and run rather than written from memory - this caught a genuine handful of real bugs/gaps along the way (documented in each slice's own notes above) that would otherwise have shipped as wrong documentation.

## Linux Packaging Plan Summary (deb, rpm, Flatpak, in progress)

A three-slice plan (Pkg-1 through Pkg-3). Made tractable by M8e's already-relocatable CMake `install()` rules and executable-relative runtime-path resolution - every format reuses `cmake --install` unchanged (`/usr` for deb/rpm, `/app` for Flatpak), so this is almost entirely packaging *metadata*, not new source. A cross-cutting design point true for all three: `ebc`/`ebpm` invoke a real backend C++ compiler as a subprocess at **runtime**, not just build time, so each format must declare that dependency explicitly rather than assume it. **Confirmed with the user**: Flatpak uses `--filesystem=host` and the host's own `g++`/`clang++` (not a bundled toolchain - a much bigger, more novel undertaking); one combined `ebasic` package per format (matching the M8e HaikuPorts recipe precedent); verification builds and extracts/runs each real package in a scratch location, never installing onto the real system via `dpkg -i`/`rpm -i`/a system-wide Flatpak install.

## Pkg-1 Implementation Notes (`.deb`, done)

- New top-level `debian/` directory (`control`, `rules`, `changelog`, `copyright`, `source/format`, `docs`) - a minimal `dh $@ --buildsystem=cmake` in `rules` needed no custom logic at all, since the existing CMake `install()` rules already do the real work. Native package format (`3.0 (native)`, version `0.1.0` with no debian-revision suffix) rather than a quilt/upstream-tarball split, matching the honest "no tagged release exists yet" gap already documented in the M8e Haiku recipe.
- `Depends: ${shlibs:Depends}, ${misc:Depends}, g++ | clang` makes the runtime-compiler dependency explicit rather than silently assumed - `dh_shlibdeps` auto-resolved the real shared-library deps (`libc6`, `libgcc-s1`, `libstdc++6`) on top of it.
- **Real, live build**: `dpkg-buildpackage -us -uc -b` succeeded on the first real attempt - debhelper's own `dh_auto_test` step ran the *entire* 26-test e2e suite automatically as part of the package build (a genuine bonus verification layer, not something this plan's own scripts had to arrange). Inspected via `dpkg-deb -c`/`-I`: correct layout (`/usr/bin/{ebc,ebpm,docgen}`, `/usr/share/ebasic/runtime/{include,pch}`, `/usr/share/doc/ebasic/`), correct auto-resolved + declared dependencies.
- **Verified exactly like M8e's own install test**: extracted the built `.deb` (`dpkg-deb -x`, never `dpkg -i` onto this real system) into a scratch prefix, ran the extracted `ebc` to compile and run a real program from there, then repeated with the source tree's `runtime/include/` temporarily renamed away entirely to confirm the installed layout is genuinely self-contained, not silently falling back to the build tree.
- **A real environment limitation surfaced and worked around by the user, not silently skipped**: installing `lintian`/`rpm`/`rpmlint`/`flatpak-builder` needs `sudo`, and this session initially had no interactive sudo access at all - even the user's own attempt via the chat's `!`-prefixed passthrough hit the same "a terminal is required to authenticate" error, confirming it's a real session-level TTY limitation, not something either side could route around from inside the chat. The user ran the install through a different, real terminal instead; `rpm`/`rpm2cpio`/`rpmlint`/`flatpak-builder` all confirmed working afterward. `lintian` initially landed in a broken/incomplete `dpkg` state (`iHR` flags, binary not actually on `PATH`) - the user repaired it shortly after.
- **`lintian` then found a real, legitimate issue, fixed rather than just noted**: `no-manual-page` for all three binaries - real Debian packaging convention expects a man page per binary. Wrote `debian/man/{ebc,ebpm,docgen}.1` by hand (standard troff/man macros, grounded in each tool's own real `--help` text and the `docs/guide/*.md` pages already written) rather than pulling in another build dependency (`help2man` isn't installed either) - each validated with `man --warnings` (zero warnings) and visually rendered before use. Wired in via `debian/ebasic.manpages` + `dh_installman` (automatic, no `rules` changes needed). Rebuilt: `lintian` now reports **zero warnings**; re-verified the man pages render correctly from the actual installed/extracted layout, not just the source tree.
- `.gitignore` gained entries for `dpkg-buildpackage`'s own build artifacts (`obj-*/`, `debian/.debhelper/`, `debian/ebasic/`, etc.) - `debian/` itself is tracked, its generated build output is not.
- Verified: full e2e suite (26/26, `linux-gcc` preset) still passing outside the packaging build too; a clean `-Wall -Wextra` rebuild.
- Next: `.rpm` packaging (Pkg-2), now that `rpmbuild`/`rpmlint` are confirmed available for real local verification.

## Pkg-2 Implementation Notes (`.rpm`, done)

- New `packaging/rpm/ebasic.spec`. **A real environment finding changed the design mid-slice**: the spec was first written using Fedora's convenience `%cmake`/`%cmake_build` macros, but this Ubuntu-based `rpm`/`rpmbuild` install has no `cmake-rpm-macros`-equivalent package at all (confirmed: no such package exists in Ubuntu's own repos, and the macros expand as literal unexpanded text here) - switched to plain `cmake -S . -B build`/`cmake --build build` invocations instead, which is both more portable (works identically on a minimal/older RPM-based system without that macro package) and directly locally-verifiable, rather than assuming the fancier macros would "just work" everywhere.
- Reuses `debian/man/*.1` (written in Pkg-1) directly rather than duplicating them under `packaging/rpm/` - both packaging formats build from the same source tree, so the same hand-written man pages serve both.
- **Real, live build**: generated a real source tarball via `git archive` (the same honest "no tagged release yet" placeholder as `Source0`'s URL, and the same technique `scripts/haiku_verify.sh` already uses) into `~/rpmbuild/SOURCES/`, then `rpmbuild -bb` - succeeded, and its own `%check` section ran the full 26-test e2e suite automatically (the rpm equivalent of Pkg-1's `dh_auto_test` bonus). Verified via `rpm2cpio | cpio -idmv` extraction (this sandbox has no `dnf`/`yum` to do a real install, and wouldn't do one here regardless per the plan's own safety note) plus the exact M8e-style isolation test (hiding `runtime/include/` and re-running the extracted `ebc`) - confirmed genuinely self-contained.
- `rpmlint` found 9 errors/7 warnings; two were real and trivially fixed (`no-packager-tag`, `no-group-tag` - added `Packager`/`Group` fields). The rest were investigated rather than blindly suppressed or blindly "fixed": the 6 `spelling-error`s are false positives on correct technical terms (transpiler, backend, interop, ...) not in the spell-checker's dictionary; `no-signature` is expected for an unsigned local test build; `manpage-not-compressed bz2` reflects this rpmlint config's own assumed compression convention, not a real defect (`.gz` is standard and portable); `invalid-license MIT` was traced to this rpmlint installation's own `ValidLicenses` config list being **completely empty** (confirmed by reading `configdefaults.toml` directly) - every license string would trigger this here, not something specific to "MIT" (which is the correct, standard SPDX identifier, already used consistently in `LICENSE`/`debian/copyright`); `devel-file-in-non-devel-package` flags the runtime headers as if they were optional library-development headers, but they're actually the compiler's own required runtime data (`ebc` cannot compile anything without them present) - splitting them into a `-devel` subpackage would be actively wrong here (a user installing `ebasic` to just run `ebc` would need a second package for it to function at all), and splitting packages was already explicitly out of this plan's scope.
- Verified: full e2e suite (26/26, `linux-gcc` preset) still passing outside the packaging build too; a clean `-Wall -Wextra` rebuild; no stray build artifacts leaked into the tracked working tree (`rpmbuild`'s own tree lives entirely under `~/rpmbuild/`, outside the repo).
- Next: Flatpak (Pkg-3), now that `flatpak-builder` is confirmed available too.

## Pkg-3 Implementation Notes (Flatpak, done - three real, escalating design corrections found only by actually installing and running it)

- New `packaging/flatpak/io.github.yann64.ebasic.json` (app-id follows Flathub's own GitHub-hosted-project convention) and `packaging/flatpak/io.github.yann64.ebasic.metainfo.xml` (validated with `appstreamcli validate` - zero findings after adding a `<developer>` element for the one informational note it first raised). `org.freedesktop.Platform`/`Sdk` `25.08` (already installed locally, both runtime and SDK, avoiding a network-dependent branch choice); `buildsystem: cmake` (plain, matching Pkg-2's own portability reasoning) building straight from a local `dir` source (no tagged release exists yet, same recurring honest gap).
- **This slice's live verification found three real, escalating problems that structural review alone would never have caught** - each one only visible by actually installing (`--user`-scoped, never system-wide) and running the packaged app for real, not just `flatpak-builder --run`ning it inside the build sandbox:
  1. **`--filesystem=host` alone doesn't let the sandboxed app *execute* a host binary** - only read/write its files. `ebc`'s own `runProcess({"g++", ...})` failed with "not found" (exit 127) even though the host clearly has `g++`. The real, standard fix is `flatpak-spawn --host <cmd>` (needs `--talk-name=org.freedesktop.Flatpak`) - added as a new `hostExecArgs()` helper in `compiler/src/driver/process.cpp`/`process.hpp`, detecting the sandbox via the standard `/.flatpak-info` marker file.
  2. **Applying it automatically inside `runProcess`/`runProcessCaptureOutput` was wrong** - it broke `ebpm` invoking its own bundled `ebc` (`flatpak-spawn --host ebc` fails, since `ebc` only exists inside the sandbox, never on the host). Corrected: `hostExecArgs()` is opt-in per call site, not a blanket transform - applied explicitly at the three genuine host-tool call sites (`ebc`'s own `g++`/`ar` invocations in `main.cpp`, all five of `ebpm`'s `git` invocations in `gitdep.cpp`), left untouched at the one call site that must stay sandboxed (`ebpm`'s call to its own `ebc` in `build.cpp`).
  3. **Even with `g++` correctly found and run on the host, it couldn't see the sandbox-only `/app/share/ebasic/runtime/...` path** passed as `-I` - a host process runs in the host's own mount namespace, where Flatpak's `/app` doesn't exist at all. Fixed by adding `stageRuntimeDirForFlatpak()` (`main.cpp`): since `--filesystem=host` was already confirmed (live) to share `$HOME` identically between the sandbox and the host, the runtime include/PCH directories are copied once into a host-visible, version-keyed cache (`~/.cache/ebasic/runtime-<version>[-<githash>]/`) the first time they're needed, then that path is handed to the host compiler instead - confirmed idempotent (a second invocation's cache-directory mtime didn't change) rather than assumed.
- Each of these three findings was surfaced to the user rather than worked around silently, since each one further corrected the plan's own original "almost entirely packaging metadata" premise - all three were confirmed necessary, not hypothetical, before being implemented.
- `flatpak-builder`'s own build-sandbox test execution (`run-tests`/`test-rule: test`, the Flatpak equivalent of Pkg-1/Pkg-2's automatic `dh_auto_test`/`%check` bonus) was tried, but failed for a different, also-real reason: `flatpak-spawn --host` needs a D-Bus session, and `flatpak-builder`'s own internal build sandbox doesn't proxy one in (confirmed: a real session bus *is* running on the actual host, just not forwarded into that specific, more-isolated build-time sandbox) - removed rather than worked around further, since the same source is already verified via `ctest` three other ways this session (plain build, `.deb`, `.rpm`).
- **Full real, live end-to-end verification** (after all three fixes): a real `--user`-scoped install (never system-wide) from a local repo, `flatpak run io.github.yann64.ebasic <file>.bas -o <out> && <out>` producing correct output, `flatpak run --command=ebpm ... new`/`run` successfully scaffolding and building a real package (proving the sandboxed-`ebc`-calls-sandboxed-`ebc` path), `flatpak run --command=docgen ... --version`, and cache-reuse confirmed on a second invocation - then fully uninstalled and the temporary local remote removed, leaving no trace on the real system.
- Verified: full e2e suite (26/26, `linux-gcc` preset) and a clean `-Wall -Wextra` rebuild unaffected by the `process.cpp`/`main.cpp`/`gitdep.cpp` changes; no stray Flatpak build/install state left in the tracked working tree or the user's real Flatpak installation.
- **The Linux packaging plan (Pkg-1 through Pkg-3) is now complete**: `.deb` (`debian/`), `.rpm` (`packaging/rpm/ebasic.spec`), and Flatpak (`packaging/flatpak/`) all build and run for real from this exact source tree, reusing M8e's relocatable `install()` design unchanged for the first two and with three now-real, generically-useful `process.cpp`/`main.cpp` accommodations for the third's genuinely different sandboxing model.

## Post-Packaging Fix: `tests/cli/version_help.sh` drive-letter bug (found via real Windows CI)

While the packaging work above was wrapping up, real `windows-mingw` CI (unrelated to it - this script predates the packaging plan) failed: `cli_version_help` errored with `D: command not found` for all three tools. Root cause: the script combined a tool's path and name into one string (`"$EBC:ebc"`) and split it back apart with `${entry%%:*}`/`${entry##*:}` - which breaks the moment the path itself contains a colon, exactly what every Windows path does (a drive letter, e.g. `D:/a/ebasic/.../ebc.exe`). `%%:*` strips from the *first* colon onward, so the extracted "path" was just `"D"`.

**Reproduced locally without a Windows machine**, by exploiting the fact that Linux filesystems (unlike Windows) allow a literal `:` in a path - created a copy of `ebc` at a path containing `D:/` and confirmed the exact same failure (`.../D: Aucun fichier ou dossier de ce nom`) with the old script, then confirmed it's gone with the fix. Fixed by removing the combined-string/split trick entirely - a small `run_checks(path, name)` helper called three times explicitly (`tests/cli/version_help.sh`), never joining a path and a name into one delimited string at all.

Verified: full e2e suite (26/26) still passing locally; pushed to trigger real CI on all four platforms to confirm the actual Windows fix, per this project's standing "CI is the real verification for Windows" discipline (M8's own approach) rather than trusting the local-repro alone.

## OS-Conditional Dependencies Plan Summary (in progress)

Prompted by the user asking whether `ebpm` can do what Cargo does with `[target.'cfg(windows)'.dependencies]` (e.g. Win32 bindings on Windows, GTK4 on Linux). **Confirmed by direct inspection it could not, on two counts**: `pkg/src/manifest.hpp`'s `Dependency` has no platform field and nothing in `resolve.cpp`/`manifest.cpp` has any OS concept; and `compiler/src/preprocessor/preprocessor.cpp` auto-defines no platform macros at all, so even if `ebpm` picked the right *package*, the consuming `.bas` source had no way to conditionally compile against a different platform API. A two-slice plan: (1) auto-defined platform macros in the preprocessor, grounded in real FreeBASIC precedent (`__FB_WIN32__`/`__FB_LINUX__`/`__FB_DARWIN__`, confirmed via web search rather than invented) plus a new `__FB_HAIKU__` for eBasic's own Haiku support (no FreeBASIC precedent there); (2) `[target.<os>.dependencies]` in `ebasic.toml`, using this project's own established platform-naming convention (`windows`/`linux`/`macos`/`haiku`, matching `CMakePresets.json`) rather than FreeBASIC's macro spelling - a deliberate two-vocabulary split, since the manifest is human-typed TOML and the macros are about real FreeBASIC source compatibility, two different concerns.

## Slice 1 Implementation Notes (auto-defined platform macros, done)

- `preprocess()` (`compiler/src/preprocessor/preprocessor.cpp`) now seeds exactly one of `__FB_WIN32__`/`__FB_LINUX__`/`__FB_DARWIN__`/`__FB_HAIKU__` into `PPState::macros` at startup, via the same `#ifdef _WIN32`/`__APPLE__`/`__HAIKU__`/`else` pattern already established in `process.cpp`/`gitdep.cpp` - no new platform-detection logic invented. No API change to `preprocess()` itself, so `ebc` and `docgen` (both callers) get it automatically.
- New `docs/reference/preprocessor.md` - and, while writing it, found the preprocessor (`#define`/`#ifdef`/`#ifndef`/`#else`/`#endif`/`#include`/`#include once`) had **no reference page at all** despite the exhaustive-dictionary scope the user chose for the earlier documentation plan - filled in as a natural, directly-adjacent gap rather than left for later, since the new platform macros would otherwise have had nowhere sensible to document. Added to the keyword index (`docs/reference/index.md`) with a new "Preprocessor directives" table (anchors re-verified with the same slug-checker script from the documentation plan - caught and fixed 2 more double-hyphen typos before they shipped).
- **Every example genuinely compiled and run, not written from memory** - including one that links against a real installed system library: this sandbox happens to have GTK4 dev libraries installed, so the doc's own `#ifdef __FB_LINUX__ ... Extern "C" Lib "gtk-4" ...` example was verified to actually compile *and link* against the real `libgtk-4.so`, not just parse.
- New e2e test `tests/e2e/platform_macros` - deliberately platform-*independent* expected output (checks "exactly one of the four is ever defined" and "#ifdef/#ifndef agree", not which specific one) so the same `expected.stdout` passes unchanged on all four CI platforms, each exercising a different branch of the same source file.
- Verified: full e2e suite (27/27, `linux-gcc`) passing; a clean `-Wall -Wextra` rebuild; `docs` Doxygen target unaffected.
- Next: `[target.<os>.dependencies]` in `ebasic.toml` (Slice 2).

## Slice 2 Implementation Notes (`[target.<os>.dependencies]` - OS-conditional dependencies complete)

- `pkg/src/manifest.hpp`/`manifest.cpp`: `Manifest` gained `targetDependencies` (keyed by `"windows"`/`"linux"`/`"macos"`/`"haiku"` - this project's own established platform vocabulary, deliberately not FreeBASIC's macro spelling, per the plan's own two-vocabulary reasoning), a new `currentTargetOS()` (same `#ifdef _WIN32`/`__APPLE__`/`__HAIKU__`/`else` pattern as Slice 1/`process.cpp`/`gitdep.cpp`), and `effectiveDependencies(manifest)` (unconditional + current-target deps merged). TOML parsing for `[dependencies]` and each `[target.X.dependencies]` now shares one `parseDependencyEntry()` helper (same `path`/`git` exactly-one-of and `branch`/`tag`/`rev` at-most-one-of validation as before, no duplicated logic) - an unrecognized `X` is a real, clear build error (`'[target.freebsd]' is not a supported target (windows, linux, macos, haiku)`), verified live rather than assumed.
- **Two real call sites needed the same fix, not one** - confirmed by grepping rather than assuming: `resolve.cpp`'s `resolveDependencyGraph` (the dependency-graph walk) and `build.cpp`'s `collectTransitiveDeps` (the `-I`/`-L`/`-l` flag computation) each independently iterated the raw `manifest.dependencies` vector - both switched to the shared `effectiveDependencies()` helper, so "which dependencies actually apply" is computed in exactly one place.
- New e2e test `tests/e2e_pkg/target_deps`: a 6-package fixture (a common lib built everywhere, one lib per platform, and an app) - verified live that only the *current* platform's lib actually gets compiled (`Compiling linux_lib (lib)` appears, `windows_lib`/`macos_lib`/`haiku_lib` never do, confirmed by checking which `.a` archives actually exist afterward), and that the app's own `#ifdef __FB_<OS>__`-guarded `#include`s correctly pick the matching interface. Deliberately platform-independent expected output (asserts the resolved lib's own reported code matches whichever macro fired, not a hardcoded platform-specific value) - same lesson as Slice 1's test, applied again.
- **A real, useful bug caught while building the test, unrelated to the new feature itself**: a lib function named `Double` failed to parse - `DOUBLE` is a reserved primitive-type keyword, not available as an identifier. Renamed to `Doubled`, not a real defect in Slice 2's own code, just a naming collision in the test fixture.
- **A real, pre-existing limitation discovered along the way, not a Slice 2 regression**: a lib function returning `STRING` is silently *not* exported into the auto-generated `.iface.bas` interface at all (`' (not exported: 'PlatformName' uses STRING, which needs a marshaling shim not yet implemented)`) - a known M5a gap, not something this slice introduced. Worked around in the test by returning an `INTEGER` platform code instead of a name string; the gap itself is out of scope for this plan (a real, separate follow-on: STRING marshaling across the `Extern "C++"` library-interface boundary).
- `docs/guide/ebpm.md` gained a "Platform-specific dependencies" section (with a real, worked GTK4/Win32 example matching Slice 1's own reference doc), cross-linked both ways with `docs/reference/preprocessor.md#platform-macros`. Full link-check (all 18 project markdown files) passes clean.
- Verified: full e2e suite (28/28, `linux-gcc`) passing; a clean `-Wall -Wextra` rebuild; `docs` Doxygen target unaffected; manual checks of both real error paths (unrecognized target name, `path`+`git` both given inside a `[target.*]` section) produce clear messages.
- **This completes the OS-conditional-dependencies plan**: `ebpm` can now do what the user asked whether it could do - resolve a genuinely different dependency per target platform, with the auto-defined platform macros (Slice 1) letting the consuming `.bas` source conditionally compile against whichever platform API that dependency actually provides.
- **Re-verified across all four real platforms**, not just locally: pushed and checked real CI (`gh run watch`) - `linux-gcc`/`linux-clang`/`macos`/`windows-mingw` all green - then re-ran `scripts/haiku_verify.sh` on the real Haiku box: 28/28, confirming `__FB_HAIKU__`/`target.haiku.dependencies` genuinely work on real Haiku hardware, not just inferred from the other three.

## v1.0.0: First Tagged Release

With every phased milestone (M0-M8), post-M8 CLI ergonomics, Linux packaging (`.deb`/`.rpm`/Flatpak), and OS-conditional dependencies complete and verified, the user asked to bump/tag `1.0.0` and bring documentation/`README.md` up to date with everything shipped since the last "no tagged release yet" placeholders were written (M8e's Haiku recipe, Pkg-1/2/3's packaging metadata).

- `project(ebasic VERSION 1.0.0 ...)` (top-level `CMakeLists.txt`) - confirmed live: `ebc`/`ebpm`/`docgen --version` all report `1.0.0 (<hash>)` correctly after rebuilding.
- `packaging/rpm/ebasic.spec` (`Version:` + a new `%changelog` entry - the *existing* `0.1.0-1` entry is left alone, a fresh entry added on top, matching RPM/Debian's own append-only changelog convention rather than rewriting history) and `debian/changelog` (same convention: a new `ebasic (1.0.0)` entry stacked above the existing `0.1.0` one). `packaging/flatpak/io.github.yann64.ebasic.metainfo.xml`'s `<release>` updated to `1.0.0` (replaced rather than appended - unlike the deb/rpm changelogs, AppStream's `<releases>` is meant to list actual shipped versions, and `0.1.0` never really was one).
- **A real .deb/.rpm rebuild caught a genuine sequencing mistake, not a code bug**: rebuilding the `.rpm` via `git archive HEAD` *before* committing the version bump grabbed the still-`0.1.0` committed state (`ebc --version` inside that `.rpm` showed `0.1.0`, not `1.0.0`) - `dpkg-buildpackage` builds directly from the working tree (uncommitted changes included), which is why the `.deb` built moments earlier correctly showed `1.0.0` while the `.rpm` didn't. Not a defect in either packaging setup - just confirmed live, rather than assumed, that the version bump needed to be committed before an archive-based build (rpm, Haiku) would pick it up; the `.deb` was re-verified again after committing.
- `README.md`: Status bumped to `1.0.0`, Features gained a line about OS-conditional dependencies (paired with the platform macros), and a new "Installing (Linux)" section documents the three real packaging formats built and verified in the Linux Packaging plan above - a real, previously-missing piece of end-user-facing documentation now that those packages genuinely exist.
- Verified: full e2e suite (28/28) passing after the bump; a real `.deb` rebuild (extracted, version-checked, re-linted clean) confirming the whole packaging pipeline still works correctly at the new version number.
- **The `v1.0.0` tag itself**: created as a real, annotated tag on the version-bump commit and pushed to both remotes (`origin`, `github`).
- **The real tag unblocked finishing the Haiku recipe for real**: `packaging/haiku/ebasic-0.1.0.recipe` (M8e's draft, with an explicit `SOURCE_URI`/`CHECKSUM_SHA256` placeholder since no tag existed yet) renamed to `ebasic-1.0.0.recipe`, with the real tarball actually downloaded (`https://github.com/yann64/ebasic/archive/refs/tags/v1.0.0.tar.gz`) and its genuine sha256 computed - not a `git archive`-derived approximation, since GitHub's own tag-archive generation isn't guaranteed byte-identical to a local one.
- **Going well beyond a lint check this time - a real `haikuporter` build in its own isolated chroot, on the real Haiku box, caught two further real bugs the earlier M8e draft (verified only via `haikuporter --lint` and manual `cmake`/`ctest` runs on the *live*, already-fully-configured system) never could have surfaced**:
  1. The isolated build chroot has no C++ startup objects (`crti.o`, `start_dyn.o`, `init_term_dyn.o`) unless `haiku_devel` is explicitly listed - my `BUILD_PREREQUIRES` (`cmd:cmake`/`cmd:g++`/`cmd:gcc`/`cmd:make`) had no equivalent `BUILD_REQUIRES` at all. Fixed by adding `BUILD_REQUIRES="haiku_devel"`, matching the real, working `app-arch/brotli` recipe's own convention (a `BUILD_REQUIRES`/`BUILD_PREREQUIRES` split this recipe had gotten subtly wrong from the start).
  2. `cmake`'s `CMAKE_INSTALL_PREFIX` was never set, so it defaulted to Haiku's real `/boot/system` - inside the isolated chroot (unlike the *live* system `haiku_verify.sh` runs against, where `/boot/system` genuinely exists and is writable) that path doesn't exist as a staging location at all, so `make install` failed outright. Fixed by adding `-DCMAKE_INSTALL_PREFIX=$prefix` to the `BUILD()` step's `cmake` invocation, matching the real, working `app-admin/conky` recipe's own idiom (confirmed by grepping the live haikuports tree for other real CMake-based recipes, not guessed).
  3. Both fixes verified by re-running the real `haikuporter` build from scratch (`rm -rf work-1.0.0` first, so nothing stale carried over) - it succeeded completely: a real `ebasic-1.0.0-1-x86_64.hpkg` was produced, with `haikuporter` itself auto-detecting the correct runtime dependency (`requires { haiku >= ..., lib:libstdc++ >= 6.0.32 }`) purely from the built binaries.
  4. **Installed and run for real** (`pkgman install`, then uninstalled again afterward to restore the box's prior state): `ebc`/`ebpm`/`docgen --version` all correctly reported `1.0.0` from `/boot/system/bin/`, and the installed `ebc` compiled and ran a real program correctly. This is real, complete, installed-package verification - not just a structural draft, and a genuine step up from every prior Haiku-packaging checkpoint in this project's history.
- `scripts/haiku_verify.sh` re-run once more (28/28) as the standard full-regression check, on top of the dedicated `haikuporter` verification above.

## clang++ as Backend Compiler: Verified and Permanently CI-Covered

The user asked for a plan to "implement compilation using clang++". **Direct inspection showed the mechanism already existed** - `ebc`'s `-cxx <compiler>`/`CXX` env var (`compiler/src/driver/main.cpp`) has never hardcoded anything GCC-specific, and `runtime/include/ebasic/runtime/*.hpp` uses no GCC-only extensions (confirmed: zero `__builtin`/`__attribute__`/`__GNUC__` hits). `ebpm` needs no `-cxx` of its own either - it invokes `ebc` as a child process, which inherits `CXX` from the environment. **But the combination had never actually been exercised anywhere**: `.github/workflows/ci.yml`'s existing `linux-clang` job only ever used Clang to build `ebc`/`ebpm`/`docgen` *themselves* - its `ctest` step never set `CXX=clang++`, so the e2e suite it ran still transpiled through plain `g++` regardless. These are two independent choices (which compiler builds the toolchain vs. which compiler the toolchain uses as its own backend) that had only ever been tested in the "both `g++`" combination. This reframed the task as verify-for-real + permanent CI coverage, not new implementation - consistent with M8's own discipline.

- **A real, comprehensive verification battery**, once the user installed `clang++` locally (blocked on the same no-interactive-`sudo` limitation as the Linux-packaging work; the user ran the install themselves): every existing `tests/e2e/*` case (the entire core language, `EXTERN` C/C++ interop, platform macros) recompiled and rerun with `-cxx clang++`, all passing with byte-identical output to their `g++`-backed originals. `--lib` mode tested in **both mixed-compiler directions** - a library built with `clang++` linked into a `g++`-built program, and the reverse - both producing correct results, proving no ABI/mangling mismatch between the two toolchains for what `ebc` actually generates.
- **`ebpm` verified with `CXX=clang++` under a `PATH` deliberately stripped of every `g++`/`gcc`/`cc`/`c++` binary** (only `clang++`/`ar`/`as`/`ld` symlinked in) - a real, conclusive proof that the whole `[lib]`+`[bin]` pipeline never silently falls back to `g++`, not just "the output looked right." (An earlier, less careful version of this same test used too minimal a `PATH`, genuinely missing `ld` - clang's own `posix_spawn failed` error was itself proof clang++ was really being invoked, just missing a linker helper; fixed by including `ld` too.)
- M6's PCH graceful-degradation claim re-confirmed live rather than trusted from the M6-era notes: `-cxx clang++` compiles cleanly with the `.gch` shadow directory still on the include path, no error or warning about the file it silently ignores.
- **CI**: `linux-clang`'s job gained a second `Test` step - `CXX=clang++ ctest --preset linux-clang` - reusing the Clang already installed for building `ebc` itself, at no extra setup cost, to close the exact gap identified above permanently. Verified locally first with the literal command CI will run (`ebc` built by `clang++`, e2e suite also run with `CXX=clang++`) before ever pushing.
- **Re-verified on real Haiku hardware too** - `clang`/`clang++` turned out to already be installed there (`llvm12_clang`-family packages, confirmed via `pkgman search`) - a full build plus `CXX=clang++ ctest` run on the real box: 28/28.
- `docs/guide/ebc.md`'s `-cxx` entry and `docs/guide/ebpm.md` (which `CXX` env var it inherits, and why no `ebpm`-specific flag is needed) both updated with this now-verified guarantee, not just "any compiler in principle."
- **Explicitly out of scope, as decided in the plan**: extending M6's PCH speedup to Clang's own `-include-pch` mechanism (a distinct performance feature, not required for correctness - M6's original GCC-only decision stands) and a MinGW+Clang toolchain on Windows (a materially bigger, more novel addition than reusing an already-available compiler on an already-supported platform).
- Verified: full e2e suite (28/28) with both the default `g++` backend and `CXX=clang++`; a clean `-Wall -Wextra` rebuild of eBasic's own compiler under Clang.

## LSP Plan Summary (complete - see the per-slice Implementation Notes below)

The user asked for a detailed plan to implement a Language Server Protocol
server for eBasic, explicitly required to be aware of `ebpm` dependencies
(a `.bas` file `#include`-ing another package's auto-generated
`<name>.iface.bas`). Guiding principle, carried over directly from this
project's own established anti-duplication discipline (`docgen` reusing
`ebasic_frontend` instead of writing a second parser): the LSP reuses the
*real* compiler/package-manager components rather than re-implementing name
resolution or dependency resolution. Six slices: LSP-1 transport/document
sync, LSP-2 diagnostics, LSP-3 Sema symbol locations/outline/hover, LSP-4
go-to-definition/references, LSP-5 `ebpm`-awareness, LSP-6 completion. Full
plan: see the `lsp/` component design below as each slice lands.

## LSP CMake Refactor Implementation Notes (`ebasic_sema` + `ebasic_pkg` shared libs, done)

Two small, purely additive CMake refactors, both mirroring the exact
`ebasic_process`/`ebasic_frontend` precedent (a shared STATIC lib split out
of an executable so a second real consumer can reuse it, rather than
duplicating the source or reaching across component boundaries):

- `compiler/CMakeLists.txt`: `sema.cpp` moved out of `ebc`'s own executable
  sources into a new `ebasic_sema` STATIC lib (public-links `ebasic_frontend`,
  since `Sema::check()` takes a `Module&`). `ebc` now links `ebasic_process`
  + `ebasic_frontend` + `ebasic_sema` instead of compiling `sema.cpp`
  directly. `codegen.cpp` stays compiled directly into `ebc` only - the LSP
  never needs codegen, so it wasn't worth sharing.
- `pkg/CMakeLists.txt`: `manifest.cpp`/`build.cpp`/`resolve.cpp`/
  `lockfile.cpp`/`gitdep.cpp` moved out of `ebpm`'s own executable sources
  into a new `ebasic_pkg` STATIC lib (public-links `tomlplusplus` +
  `ebasic_process`). `ebpm`'s executable target shrinks to just `main.cpp`,
  linking `ebasic_pkg`. This is what will let the upcoming LSP-5 slice call
  `resolveDependencyGraph`/`computeConsumerDirs` in-process for real,
  identical-to-`ebpm` dependency resolution.
- No behavior change to `ebc`/`ebpm` themselves - purely a build-graph
  reshape. Verified: a full rebuild from scratch (`linux-gcc` preset) is
  clean with no new warnings, and the full e2e suite (28/28) passes
  unchanged.
- `third_party/nlohmann_json/` vendored (`nlohmann/json.hpp` single header,
  pinned to the `v3.11.3` tag, plus its `LICENSE`) the same way
  `third_party/tomlplusplus/` already is - needed for the LSP's own
  JSON-RPC transport (LSP-1).

## LSP-1 Implementation Notes (transport + document sync, done)

New `lsp/` component, new `ebasic_lsp` executable (`lsp/CMakeLists.txt`,
linking `ebasic_frontend`/`ebasic_sema`/`ebasic_pkg`/`nlohmann_json` - the
latter two unused until LSP-5/LSP-3 respectively, linked now so later
slices don't need to reshape the CMake target).

- `lsp/src/rpc.{hpp,cpp}`: `Content-Length`-framed JSON-RPC read/write over
  `std::istream`/`std::ostream` - tolerant of a bare `\n` line ending in
  headers (the spec mandates `\r\n`, but tolerating both costs nothing and
  matches how forgiving real LSP servers tend to be).
- `lsp/src/documents.{hpp,cpp}`: `DocumentStore` - an in-memory
  `uri -> text` map, replaced wholesale on every `didChange` (Full sync,
  not incremental - `.bas` files are small enough that this is a
  deliberate simplicity choice, not a stopgap).
- `lsp/src/server.{hpp,cpp}`: the method dispatch table -
  `initialize`/`initialized`/`shutdown`/`exit` (advertising
  `textDocumentSync: 1` only so far; later slices add
  `hoverProvider`/`definitionProvider`/etc. to the same capabilities object
  as each lands) and `textDocument/didOpen`/`didChange`/`didClose`. An
  unrecognized *request* gets a real JSON-RPC `MethodNotFound` (`-32601`);
  an unrecognized *notification* is silently ignored, matching the spec.
  Exit code is spec-mandated: `0` if `shutdown` preceded `exit` (or a
  clean end-of-stream after one), `1` on an unclean disconnect (the client
  process died, or exited without ever calling `shutdown`).
- **No editor available in this sandbox to verify interactively** (neither
  `nvim` nor `vim` installed) - `tests/lsp/smoke_test.sh` substitutes a
  scripted, real protocol-level conversation (constructs real
  `Content-Length` frames with `printf`, pipes a fixed request/notification
  sequence into one `ebasic_lsp` invocation, greps the compact-JSON
  response stream for what each step must produce) as this session's own
  verification, registered as a permanent CTest (`lsp_smoke`) alongside the
  rest of the suite - not a one-off throwaway check. `docs/guide/lsp.md`
  documents a real Neovim `vim.lsp.start` snippet for the user's own,
  genuinely interactive verification once they have a moment to try it by
  hand.
- Verified: full e2e suite (29/29, including the new `lsp_smoke`), a clean
  `-Wall -Wextra` rebuild.

## LSP-2 Implementation Notes (diagnostics, done)

- `DiagnosticEngine` gained one small, additive public accessor -
  `fileName(int fileId)` (`compiler/src/diagnostics/diagnostics.hpp`/`.cpp`)
  - `printAll` itself now calls it too, rather than duplicating the same
  bounds-checked lookup inline.
- `lsp/src/uri.{hpp,cpp}`: real `file://` URI <-> filesystem path
  conversion (percent-encode/decode, and the RFC 8089 Windows drive-letter
  form `file:///C:/...`) - needed both to turn a `didOpen`/`didChange`'s
  URI into the path `preprocess()` resolves `#include`s relative to, and to
  turn a diagnostic's real originating file (via the new `fileName()`)
  back into a URI to publish against.
- `lsp/src/diagnostics.{hpp,cpp}`: `computeDiagnostics(path, text)` runs
  the exact same Preprocessor -> Lexer -> Parser -> Sema pipeline `ebc`'s
  `main.cpp` does (now via `ebasic_frontend` + `ebasic_sema`) against a
  document's *current*, possibly-unsaved text, grouping the resulting
  diagnostics by their real originating file (not just the edited
  document) - an error inside an `#include`d file is published against
  that file's own URI. An `#include`d file itself is still read fresh from
  disk (an unsaved edit to one that's *also* open in the editor isn't
  picked up until saved) - a deliberate simplification, not a bug.
- `Server::publishDiagnostics` (`lsp/src/server.cpp`) always publishes an
  (possibly empty) array for the edited document itself, and tracks which
  other files it published a non-empty array for last time
  (`lastDiagnosticUris_`) so a fix (or an `#include` that's no longer
  reached) still clears a stale diagnostic instead of leaving it stuck.
  Wired into both `didOpen` and `didChange`.
- Diagnostic `range`s are one character wide (`SourceLoc` is a single
  point everywhere - no node stores an end position, confirmed during
  planning) - the closest honest approximation an editor can still usefully
  underline; real per-token spans aren't added until/unless a later slice
  needs them.
- `tests/lsp/diagnostics_test.sh` (new `lsp_diagnostics` CTest): a real
  Sema error surfacing with the right message/severity, a `didChange` that
  fixes it clearing the diagnostic, and a diagnostic inside an `#include`d
  file landing on that file's own URI, not the including document's.
- Verified: full e2e suite (30/30, including the new `lsp_diagnostics`), a
  clean `-Wall -Wextra` rebuild, manual protocol-level checks against a
  real Sema type error and a real cross-file `#include` error before
  writing the permanent test (both matched the test's own expectations
  exactly).

## LSP-3 Implementation Notes (Sema symbol locations, outline, hover, done)

- `Sema` (`compiler/src/sema/sema.hpp`) gains a `declLoc` field on
  `SymbolInfo`/`ProcedureInfo`/`RecordInfo`/`PropertyInfo`, populated at
  every existing registration site (`collectProcedures`, `collectTypes`'s
  TYPE/UNION/method/property registration, the `Dim`/`Const`/`Enum`/
  parameter handling in `checkStmt`) - purely additive, no behavior change
  to compilation itself. A new `SemaIndex` struct (a plain copy of
  `symbols_`/`procedures_`/`structs_`/`namespaces_`) and a public
  `Sema::index()` accessor expose this to tooling without making Sema's own
  private maps part of its public surface.
- `lsp/src/symbols.{hpp,cpp}`: `checkDocument(path, text)` runs the same
  pipeline as `computeDiagnostics`, but returns the checked `Module` +
  `SemaIndex` whenever Preprocessor/Lexer/Parser succeed - even if Sema
  itself found body-level errors, since `collectProcedures`/`collectTypes`
  (module-level registration) always complete *before* any statement body
  is checked (confirmed by reading `Sema::check`'s own pass ordering) - so
  hover/outline stay useful during in-progress edits elsewhere in the file.
- `documentSymbols(module)` walks the AST with the same switch-over-
  `stmt.kind` shape as `docgen/src/render.cpp`'s own `collectSections` (not
  literally shared - it's private to docgen's own translation unit - but
  deliberately mirrored rather than independently re-derived), emitting LSP
  `DocumentSymbol` JSON instead of Markdown sections.
- `identifierAt(text, line, character)`: re-lexes just the requested line
  in isolation to find the identifier token under the cursor - eBasic's
  `SourceLoc` is a single point everywhere (confirmed during planning), so
  there's no stored range to hit-test a position against directly; this
  sidesteps needing one.
- `hoverFor(index, rawName)` looks up the identifier's *canonical* form in
  `SemaIndex` but renders the signature using the identifier's *original*
  on-screen casing (BASIC is case-insensitive; `ProcedureInfo`/etc. don't
  store the declared spelling, only the canonical map key) - caught by
  manual testing showing `FUNCTION square(...)` instead of the real
  `FUNCTION Square(...)` before this was fixed.
- **A real crash bug, caught by the existing `lsp_smoke` test breaking**:
  once `textDocument/hover` was wired up, `lsp_smoke`'s old "unimplemented
  method" check (which happened to *be* a bare `textDocument/hover` request
  with empty `params`) now reached the real handler, which did
  `params.at("textDocument")` with no exception boundary anywhere in
  `dispatch()` - an uncaught `nlohmann::json::out_of_range` crashed the
  whole server. Fixed by wrapping `dispatch()`'s method handling in a
  single `try`/`catch (const json::exception&)`, replying `InvalidParams`
  (`-32602`) for a malformed *request* rather than dying - one boundary
  covers every handler instead of repeating it in each. `lsp_smoke` updated
  to test a genuinely still-unimplemented method
  (`textDocument/completion`, LSP-6) for `MethodNotFound`, plus a new,
  permanent check that a malformed `hover` request gets `InvalidParams`,
  not a crash.
- `tests/lsp/symbols_test.sh` (new `lsp_symbols` CTest): `documentSymbol`
  finds a top-level `FUNCTION`; `hover` over a call site and over a
  variable reference both return the right resolved signature; hovering
  somewhere with no identifier returns a null result, not an error.
- A second `-Wall -Wextra` pass caught 3 new `-Wmissing-field-initializers`
  warnings from aggregate-initializing `SymbolInfo` with only its first 3
  (of now 5) members at 3 call sites in `sema.cpp` - fixed by switching
  those to explicit field assignment instead of brace-init.
- Verified: full e2e suite (31/31, including the new `lsp_symbols`), a
  clean `-Wall -Wextra` rebuild (after the missing-field-initializer fix
  above), manual protocol-level `documentSymbol`/`hover` checks before
  writing the permanent test.

## Post-LSP-3 Fix: `lsp_diagnostics`'s cross-file scenario, caught by real CI on macOS and Windows

Real CI (not caught locally - Linux's `/tmp` isn't a symlink, and this
sandbox has no Windows/macOS to test on directly) found `lsp_diagnostics`'s
`#include` scenario failing on **both** other platforms, for two distinct,
genuine reasons - neither a defect in `ebasic_lsp`/the preprocessor itself,
both a wrong assumption baked into the *test*:

- **macOS**: the test expected `lib.bas`'s diagnostic to be published
  against `file://$WORKDIR/lib.bas` (the raw `mktemp -d` path), but got
  `file:///private/var/folders/.../lib.bas` instead. Traced to
  `preprocessor.cpp`'s own, pre-existing, deliberate `fs::canonical()` call
  on every `#include` target (needed for circular-`#include` detection to
  recognize two different relative paths to the same real file) -
  `fs::canonical` resolves symlinks, and macOS's `/var` is itself a symlink
  to `/private/var`, so any `mktemp -d` result canonicalizes to a different
  string. The *including* document (never written to disk - its content
  only ever exists as the didOpen "text") keeps whatever raw path
  string was registered for it, since canonicalizing a path that doesn't
  exist on disk fails and falls back to the plain string - only a real,
  on-disk `#include` *target* goes through the full canonicalization.
  Confirmed by reproducing the exact asymmetry locally via a real symlinked
  temp directory (`ln -sfn`) before touching the test.
- **Windows**: the test's hand-built `file://$WORKDIR/...` URI embedded
  MSYS2's own POSIX-style temp path (e.g. `/tmp/tmp.XXXX` or
  `/d/a/_temp/...`) directly - a form `ebasic_lsp` (a plain, non-msys-
  runtime-aware native `mingw-w64-x86_64-toolchain` build) cannot resolve
  via real Win32 file APIs, so `fs::canonical()` on the `#include` target
  failed outright ("cannot open included file"). A real Windows LSP client
  would never send such a URI - it sends a real `file:///C:/Users/...`
  path.
- **Fix, in the test only** (`tests/lsp/diagnostics_test.sh`): a new
  `to_file_uri` helper resolves symlinks via `cd "$dir" && pwd -P` (mirrors
  `fs::canonical`'s own resolution) and, when `cygpath` is available
  (MSYS2/Windows), converts to the real native path first - matching
  exactly what the production pipeline actually does/expects, rather than
  loosening the check. Verified locally by reproducing both failure modes
  directly: a real symlinked temp dir (macOS's exact scenario) and a raw
  manual protocol conversation confirming the asymmetric canonicalization
  behavior first-hand, before writing the fix.
- No production code changed - `ebasic_lsp`/the preprocessor's behavior was
  correct throughout; only the test's own path assumptions were wrong. A
  follow-up push fixed a second instance of the same root cause: the fix
  above only converted `lib.bas`'s *expected* URI (the assertion), not
  `MAIN2_URI` (the URI actually *sent* to the server), which was still the
  raw MSYS2-style path - so Windows still failed to resolve the `#include`
  at all until that URI was fixed the same way.

## LSP-4 Implementation Notes (go-to-definition, find-references, done)

- `CheckedDocument` (`lsp/src/symbols.hpp`) now also carries the
  `DiagnosticEngine` used to check it (moved in, not just its
  diagnostics) - needed so a `declLoc`'s `fileId` (e.g. one landing inside
  an `#include`d file) can be mapped back to a real path via
  `diags.fileName(fileId)`, then to a URI via `pathToUri`.
- `declLocFor(index, rawName)`: the same procedure/TYPE/variable lookup
  order as `hoverFor`, returning just the `SourceLoc`. `pointRange` (used
  by `documentSymbols` since LSP-3) is now a shared, public function in
  `symbols.hpp` rather than private to `documentSymbols`'s own translation
  unit, so `definition`/`references` build LSP ranges the same way.
- `findReferences(module, targetKey)`: a full recursive walk over every
  `Stmt`/`Expr` field that can hold a nested statement or expression
  (`If`/`SelectCase`/`ForNext`/`WhileWend`/`DoLoop` bodies and conditions,
  `CaseArm` matches, `EnumMember` values, `NamespaceDecl`'s body via the
  shared `body` field, TYPE method prototypes) - collecting every
  `Ident`/`Call`/`Member` expression matching `targetKey`, **plus** each
  `Dim`/`Const`/`Assign`/`ForNext`/`Goto`/`Label`/`GoSub` statement's own
  `name` field, since a plain `x = 5` assignment's target has no `Expr`
  node of its own at all (it's a bare `Stmt::name`, unlike every other
  statement kind) - confirmed by reading `ast.hpp`'s full `Stmt`/`Expr`
  shape before writing the walker, not assumed.
- `Server::handleReferences` reconciles `findReferences`' own output
  against `includeDeclaration` explicitly rather than trusting the walker
  to have gotten every symbol kind's declaration-inclusion right: a
  variable's own declaration already appears in `findReferences`' results
  (its `Dim` statement's `name` is itself walked as a reference), but a
  `SUB`/`FUNCTION`/`TYPE`'s declaring statement is never walked that way -
  so the handler adds/removes the `declLocFor` result explicitly, comparing
  by `(fileId, line, column)`.
- Verified manually against a real fixture (a `FUNCTION` called twice, a
  variable assigned once and read once) before writing
  `tests/lsp/definition_references_test.sh` (new `lsp_definition_references`
  CTest): go-to-definition on a call site lands on the `FUNCTION`'s own
  declaration; find-references-with-declaration returns both call sites
  plus the declaration; find-references-without-declaration on the
  variable returns the assignment target and the read, never the `DIM`.
- Verified: full e2e suite (32/32, including the new
  `lsp_definition_references`), a clean `-Wall -Wextra` rebuild, real CI
  green on all 4 platforms.

## LSP-5 Implementation Notes (`ebpm`-awareness, done)

The user's explicit requirement for this whole LSP effort - "the LSP must
also consider libraries included by ebpm". New `lsp/src/pkgaware.{hpp,cpp}`:

- `findPackageRoot(startDir)`: walks up from a document's own directory
  looking for `ebasic.toml`, mirroring how `ebpm` itself finds "the package
  rooted at the current directory" - just anchored at the file's directory
  instead of a process's CWD.
- `resolvePackageContext(packageDir)`: calls `ebasic_pkg`'s real
  `computeConsumerDirs` (for the `-I` set) and `resolveDependencyGraph`
  (to check each dependency's own `interfacePath()` existence, and parse
  every *existing* one via `checkDocument` into its own `SemaIndex`) -
  reusing the real dependency-graph resolution rather than a second,
  potentially-drifting implementation, exactly per the plan.
- **A real, deliberate design constraint, confirmed by reading
  `gitdep.cpp` before writing any caching logic**: `resolveDependencyGraph`
  runs a genuine `git clone`/`git fetch` for every `git` dependency edge,
  every single call, with no "skip if recently resolved" caching of its
  own. Recomputing this on every `didChange` keystroke would mean a
  network fetch per keystroke - a correctness/performance problem, not
  just a slowness one. `Server::packageContextFor` therefore caches by
  package root directory (`packageCache_`) and only force-refreshes from
  `didOpen` (`handleDidOpen` calls it with `forceRefresh=true` before
  `publishDiagnostics`); every other call site (`didChange`'s own
  `publishDiagnostics`, `documentSymbol`, `hover`, `definition`,
  `references`) reuses whatever's cached.
- `computeDiagnostics`/`checkDocument` both gained an `includeDirs`
  parameter (default empty, so every pre-existing call site/test is
  unaffected), threaded through to `preprocess()` so `#include
  "dep.iface.bas"` resolves via the package's own dependency target
  directories, exactly like a real `ebc -I ...` invocation.
- **Graceful degradation for an unbuilt dependency**: `computeDiagnostics`
  gained `missingInterfaces` (dependency names whose own
  `target/<name>.iface.bas` doesn't exist yet) and a small
  `annotateMissingInterface` rewrite - a raw preprocessor "cannot open
  included file 'mathlib.iface.bas'" becomes "...- dependency 'mathlib'
  hasn't been built yet; run `ebpm build`" whenever the missing file's
  name matches a real, known-but-unbuilt dependency (an unrelated/
  misspelled `#include` target still gets the honest, unannotated error).
- **Cross-package hover/go-to-definition**: `handleHover`/`handleDefinition`
  fall back to each of the package's resolved dependencies' own parsed
  `SemaIndex` (`PackageContext::dependencies`) when a symbol isn't found in
  the current document's own index - a go-to-definition landing in a
  dependency lands in its real, on-disk generated `.iface.bas` (assumed to
  be the interface's only file, since an auto-generated interface never
  itself contains an `#include` - true by construction, not just
  observed).
- **`workspace/didChangeWatchedFiles`**: a handler exists
  (`handleDidChangeWatchedFiles`, dropping any cached package whose root is
  an ancestor of a changed file's path) but this server doesn't yet
  dynamically register interest in it (`client/registerCapability` - a
  server-initiated request/response exchange this server has no
  infrastructure for yet, a real, deliberate scope cut rather than a silent
  gap) - documented honestly in `docs/guide/lsp.md` as a known limitation:
  reopening the file (or restarting the server) after `ebpm build` is
  today's reliable path to fresh dependency info.
- Verified against a real fixture, not just unit-style checks: reused
  `tests/e2e_pkg/lib_and_app` (a real `[lib]` + `[bin]` pair) - manually
  first (confirmed the exact "not built yet" hint, then a real `ebpm
  build`, then real hover/go-to-definition results), then as the new
  `tests/lsp/pkgaware_test.sh` (`lsp_pkgaware` CTest), which runs a real
  `ebpm build` itself rather than pre-building the fixture.
- Verified: full e2e suite (33/33, including the new `lsp_pkgaware`), a
  clean `-Wall -Wextra` rebuild.

## Post-LSP-5 Fix: `lsp_pkgaware`'s Windows CI failure - the same root cause, again

Real Windows CI failed `lsp_pkgaware` (`cannot open included file
'mathlib.iface.bas'` with **no** hint appended, meaning package detection
itself silently failed) - the exact same root cause as the earlier
`lsp_diagnostics` fix (a hand-built `file://$WORKDIR/...` URI embedding
MSYS2's own POSIX-style path directly, which `findPackageRoot`'s
`fs::exists` check can't resolve via real Win32 file APIs), just not yet
applied to this brand-new test file - a real reminder that this fix needs
to be a standing habit for any future LSP test touching real files, not a
one-off patch. Fixed the same way: `tests/lsp/pkgaware_test.sh` gained its
own `to_file_uri` helper (`cd "$dir" && pwd -P` then `cygpath -w` when
available), applied to `MAIN_URI`. Verified locally (including a real
symlinked temp dir) before pushing.

## LSP-6 Implementation Notes (completion, done - all six LSP slices now complete)

- `completionItems(index, dependencyIndexes)` (`lsp/src/symbols.cpp`):
  every reserved keyword (a small, hand-maintained copy of `lexer.cpp`'s
  own keyword table - presentation-only data, not parsing/resolution
  logic, the same reasoning `docgen`'s own `basicTypeName` copy already
  relies on) plus every name in the current document's own `SemaIndex`
  and each of the package's resolved dependencies' own indexes
  (`PackageContext::dependencies`, already built by LSP-5). Not
  context-sensitive (no attempt to filter by grammatical position) -
  labels for symbols are their canonical (lowercased) form, since
  `SemaIndex` doesn't retain a declaration's original casing and
  completion (unlike hover/go-to-definition) has no specific on-screen
  token to borrow casing from.
- **The "last good parse" fallback** (`Server::lastGoodIndex_`, keyed by
  document URI): `publishDiagnostics` (already run on every `didOpen`/
  `didChange`) now also runs `checkDocument` and caches its `SemaIndex`
  whenever it succeeds. `handleCompletion` uses the *current* text's own
  `checkDocument` result when available, falling back to this cache when
  the current text has a syntax error (mid-edit) - so completion doesn't
  go blank while still typing, per the plan's own explicit design.
- Verified manually first (a real fixture, a genuinely broken mid-edit
  version, confirming completion still returned the same 76 items -
  keywords + `Square`/`total` - after the syntax error), then as
  `tests/lsp/completion_test.sh` (new `lsp_completion` CTest).
- **`lsp_smoke` needed a second fix** (the same class of self-inflicted
  staleness as LSP-3's "hover became real, breaking the MethodNotFound
  test that happened to send a bare hover request"): its own
  "unimplemented method" check used `textDocument/completion` - now a
  *real* method, so the check started asserting `-32601` against a
  request that now legitimately succeeds. Fixed by switching to a
  deliberately-fake, never-real method name
  (`textDocument/notARealMethod`) instead of borrowing whichever real
  method happened to be unimplemented *this slice* - a test that won't go
  stale again the next time a new capability lands.
- Verified: full e2e suite (34/34, including the new `lsp_completion`), a
  clean `-Wall -Wextra` rebuild.

All six planned LSP slices (transport/sync, diagnostics, symbols/hover,
go-to-definition/references, `ebpm`-awareness, completion) are now
complete and CI-green on all 4 platforms - `linux-gcc`/`linux-clang`/
`macos`/`windows-mingw` via GitHub Actions, and re-confirmed on real Haiku
hardware via `scripts/haiku_verify.sh` (34/34, including all 6 `lsp_*`
tests). `docs/guide/lsp.md` updated to reflect this (no longer "work in
progress").

## Post-1.0: Function Pointers for C Callback APIs (`@ProcName`)

Prompted by a downstream project (`eb-gtk4`, a GTK4 binding library) that
needed to bind `g_signal_connect`-style APIs: GTK/GLib's whole signal
system is callback-driven, and every `Extern`/`Declare` before this point
could only let eBasic *call into* external code, never let external code
*call back into* eBasic. Confirmed by direct inspection (not assumed) that
this was a genuine, complete gap: `TypeKind` had no function-pointer kind,
no `ADDRESSOF`/`CALLBACK` keyword existed, and `@` (AddressOf) only
accepted an lvalue operand (`compiler/src/sema/sema.cpp`'s `isLvalue`) -
matching the M3 roadmap note above that explicitly deferred "function
pointers" with no later milestone ever reintroducing them.

**Deliberately narrow scope, decided before implementation**: rather than
building a full structurally-typed function-pointer *type* (its own
`TypeKind`, parsed `SUB(...)`/`FUNCTION(...) AS T` syntax, and matching
Sema/Codegen machinery throughout), `@ProcName` produces plain `ANY PTR` -
reusing the pointer type this language already has, and the "ANY PTR is
universally compatible with any pointer type" Sema rule it already
enforces (`isAssignCompatible`, unchanged). This matches how the real C
callback APIs motivating this feature actually work in practice (GLib's
own `GCallback` is itself just a generic function-pointer typedef, always
cast at the call site) and how eBasic already treats `Extern` bindings in
general (it never parses a real header - only the *value* needs to line
up at the ABI level, not a matching declared type on the eBasic side).
Verified concretely: converting a function pointer to `void*` is *not* an
implicit conversion in C++ (confirmed by directly compiling a minimal
reproduction with `g++ -std=c++17 -Wpedantic`, which hard-errors
`-fpermissive` without a cast) - so Codegen must insert an explicit
`reinterpret_cast<void*>` itself; a new `Expr::isProcAddress` flag (set by
Sema, read by Codegen) marks exactly this case.

**What's addressable**: only a plain, bodied, top-level `SUB`/`FUNCTION` -
`Extern`-declared procedures are rejected (no eBasic-compiled body to take
the address of) and so are `TYPE` methods (implicit `This` has no room in
a plain C function pointer), both with clear diagnostics. Every
parameter's type, and the return type for a `FUNCTION`, must be
C-ABI-compatible - `STRING` (a C++ class, not a C-layout value) is
rejected the same way M4b's `Extern` signature checks already reject it
elsewhere, pointing the user at `ZSTRING` instead.

Implementation: `ProcedureInfo` gained `isExtern` (`sema.hpp`/`.cpp`, set
from the existing `Stmt::isExtern`, needed to reject `@`-of-`Extern`); the
`AddressOf` case in `Sema::checkExpr` gained an early branch recognizing
an `Ident` operand that names a known top-level procedure, before falling
through to the ordinary lvalue path unchanged; Codegen's `AddressOf` case
gained a matching `isProcAddress` branch emitting the explicit cast. No
`ast.hpp` `TypeKind`/`Type` changes at all - the entire feature fits
inside `Expr::isProcAddress` (new) plus `ProcedureInfo::isExtern` (new).

New e2e test `tests/e2e/function_pointers` mirrors M4's own real-library
pattern: the C fixture library (`tests/fixtures/c/fixture.c`) gained
`eb_fixture_invoke_callback`, a real separately-compiled C function taking
a `void(*)(int,void*)`-shaped callback and invoking it - genuinely
verifies a real, external C call *back into* eBasic-compiled code, not
just a parse/compile check. Verified: this new test plus the full existing
suite (35/35) passing, and a clean `-Wall -Wextra` rebuild from scratch
with zero new warnings.

`docs/reference/extern-interop.md` gained a `@ProcName` section
documenting the feature, cross-linked from the existing `Pointers`
reference page.

## Post-1.0: `ebpm` Forwards a Dependency's Own `Lib` Clauses Transitively

Found while building `eb-gtk4` (the same downstream project that motivated
`@ProcName` above): a `[lib]` package wrapping a native C library (e.g.
`Extern "C" Lib "gtk-4"`) built and linked *itself* just fine, but any
package depending on it failed the final link with undefined references to
every raw C symbol - confirmed live with a minimal two-package reproduction
(a `mygtk` lib wrapping one real `gtk_get_major_version()` call, consumed
by a `consumer` app via a path dependency) before writing any fix, not
assumed. Root cause, confirmed by reading `<output>.iface.bas`'s real
generated content: `ebc --lib`'s auto-generated interface only ever names
*that package's own* archive (`Extern "C++" Lib "mygtk" ... End Extern`) -
a raw system library the package's own `Extern` declarations need
contributes no new symbol to the archive for the interface generator to
notice, so that information had nowhere to go at all. The existing
`-l <name>` / `Options::extraLibNames` plumbing in `ebc` (added for M5c's
transitive-linking case) was already exactly the right mechanism - it
just had no way to learn a dependency's raw lib names, only its package
name.

**Also found, while probing why this hadn't already broken every existing
test**: `ebpm test`'s own fixtures happened to dodge this gap entirely by
`#include`-ing a package's *raw source* directly rather than its generated
interface (this project's own `eb-gtk4` test suite did the same, by
choice) - a real, working pattern, but not the one `ebpm`'s documented
dependency mechanism (`[dependencies]` + the generated `.iface.bas`) is
built around, so the gap was real for any genuine cross-package consumer
despite every existing golden test passing.

Fix: `ebc --lib` now also writes `<output>.libs` (plain text, one `Lib
"name"` per line - see `ebc`'s own updated usage text and
`docs/guide/ebc.md`) alongside the archive and interface file. `ebpm`
(`pkg/src/build.cpp`) gained `appendLibsSidecar`, called from both
`buildPackageWithDeps` (a real downstream package's own build) and
`computeConsumerDirs` (`ebpm run`/`test`'s dependency-graph resolution) for
every transitive dependency (and, in `computeConsumerDirs`, the root
package's own sidecar too, for a test that includes the root's generated
interface rather than its raw source) - each dependency's raw lib names
are merged in, deduplicated, alongside its package name, in the same
`extraLibNames` list `ebc` already consumes.

New e2e test `tests/e2e_pkg/lib_and_app_extern_lib` (a `wraplib` package
wrapping the same real `ebfixturec` C library the plain `e2e/extern_c`
test already links against, consumed by an app via a path dependency) -
deliberately reuses existing, already-CI-available fixture infrastructure
rather than requiring GTK4 (not installed on this project's own CI
runners) just to prove the general mechanism. `tests/e2e_pkg/run_case.sh`
gained an optional 6th argument (a fixture lib directory), exported as
`LIBRARY_PATH` (a real `g++`/`clang++` environment variable adding extra
`-L` search directories) before invoking `ebpm`, since `ebpm` itself has
no `-L`-equivalent of its own to thread through.

Verified: the original two-package reproduction now succeeds end-to-end
(`consumer` prints `4`, GTK4's real installed major version, through two
package boundaries); the new e2e test plus the full existing suite
(36/36) passing; a clean `-Wall -Wextra` rebuild from scratch with zero
new warnings.

**Still open, deliberately deferred** (per the plan's own Linux-first
platform scoping): this closes the `-l` gap on every platform, but not the
separate `-L` (search *path*) gap for a library installed somewhere other
than the linker's default search path (e.g. Homebrew's `/opt/homebrew/lib`
on macOS) - `ebpm` still has no manifest field for that, tracked as a
distinct follow-on if/when it actually blocks a real build.

## Post-1.0: `--lib`'s Interface Generator Drops a Derived (EXTENDS) TYPE

Found immediately after the `Lib`-forwarding fix above, building
`eb-gtk4`'s actual wrapper-type hierarchy (`TYPE Widget EXTENDS Obj`,
`TYPE Button EXTENDS Widget`, ...): `generateLibraryInterface` required
`stmt.baseTypeName.empty()` to export a TYPE at all, so *every* derived
TYPE - even one with zero fields of its own beyond what it inherits, the
exact shape a wrapper hierarchy has - was silently dropped from
`.iface.bas` entirely, while the free functions taking/returning it were
still exported and now referenced a TYPE the interface never declared.
Confirmed live before fixing: a real two-package reproduction
(`gtk4`/`gtk4consumer`) failed with `unknown TYPE 'Button'` the moment the
consumer tried to use it.

Fix: `generateLibraryInterface` (`compiler/src/codegen/codegen.cpp`) now
exports a derived TYPE too, as long as it (and, transitively, every TYPE
in its own `EXTENDS` chain) is still method/ctor/dtor-free -
`isExportablePlainData` checks the whole chain, and `emitType` emits a
base before the derived TYPE that extends it (mirroring `genTypeDecl`'s
own established "dependency first" pattern in the same file), each TYPE
emitted at most once regardless of how many derived types or plain field
references reach it. New e2e test `tests/e2e_pkg/lib_and_app_extends`
(a two-level `Shape` -> `Colored` -> `Circle` chain, the last link adding
no fields of its own) - verified: the original two-package reproduction
now succeeds; the new test plus the full suite (37/37) passing; a clean
rebuild.

## Post-1.0: `--lib`'s Interface Generator Drops top-level `CONST`/`ENUM`

Found alongside the `EXTENDS`-export fix above, building `eb-gtk4`'s
orientation/flag constants (`CONST GTK_ORIENTATION_VERTICAL = 1`, needed
by nearly every real GTK4 call): `generateLibraryInterface` had no branch
for `StmtKind::Const`/`StmtKind::Enum` at all - a downstream consumer got
`variable 'GTK_ORIENTATION_VERTICAL' is not declared` the moment it used
one, confirmed live before fixing.

Fix: a `CONST` whose initializer is a plain integer or double literal is
now re-emitted verbatim (`CONST Name = <literal>`) - deliberately
restricted to literals only (not any general constant expression, e.g. one
referencing another `CONST`/`ENUM` member or an operator), since safely
re-deriving those would need re-serializing arbitrary expression text back
into `.bas` syntax, a distinctly larger feature; skipped with a comment in
the same `skippedText` stream the existing STRING-signature skip already
uses, not silently dropped. An `ENUM`'s members need no such restriction -
Sema has already fully resolved every member's value (`resolvedValue`,
computed for auto-increment among other things) regardless of how it was
originally written, so the whole `ENUM` is always re-emitted from those
resolved values directly. New e2e test `tests/e2e_pkg/lib_and_app_consts`
(a `CONST` and an `ENUM`, both consumed downstream) - verified: the
original `eb-gtk4` build now succeeds past this point; the new test plus
the full suite (38/38) passing; a clean rebuild from scratch, zero new
warnings.

## Fixed: `ANY PTR` -> typed `PTR` doesn't compile

Previously logged just below as a confirmed-but-deferred known issue
(found while building `eb-gtk4`): assigning or passing an `ANY PTR`-typed
*value* where a specific typed `PTR` is expected (`DIM`/plain `Assign`, a
call argument, a `CONST` initializer, or a `RETURN`/implicit-return value)
failed backend compilation, even though `Sema::isAssignCompatible`
explicitly allows it and the docs describe `ANY PTR` as "implicitly
converted to and from other pointer types" - only the *other* direction
(a typed pointer into an `ANY PTR` slot) was actually implicit in the
generated C++ (`T*` -> `void*` is a real implicit C++ conversion; `void*`
-> `T*` is not, in C++, unlike C).

The previous attempt's stated blocker - "Codegen has no target-type
awareness at Sema's several `isAssignCompatible` call sites" - turned out
to be avoidable rather than fundamental: instead of threading target
types *into* Codegen, `Sema` now annotates the *value expression itself*
at the exact moment it decides the bridge is legal. A new free helper,
`annotatePointerBridge(target, value)` (sema.cpp, next to the existing
`pointeesIdentical`), is called once at all 7 real call sites (found by
grepping every `isAssignCompatible` invocation in sema.cpp: `checkCallArgs`
- the single funnel for every call/method-call argument; `CONST`
initializers; both `Assign` forms of return-assignment and the real
`RETURN expr`; `Assign` with an explicit Member/Call-chain target
(covering PROPERTY setters too, since codegen routes them through the
same value expression); the implicit `This.field = value` fallback; and
the plain variable/array-element `Assign` case). It sets a new
`Expr::pointerCastTo` field (`ast.hpp`) - a `shared_ptr<Type>` snapshot of
the target type - whenever the *value*'s own resolved type is a bare ANY
PTR (`Pointer` with a null pointee, i.e. genuinely `void*` in the
generated C++, at any nesting depth of the *target*). `Codegen::genExpr`
was split into a thin public wrapper plus the original body (renamed
`genExprBase`); the wrapper checks `pointerCastTo` and wraps the rendered
text in `static_cast<T*>(...)` when set. Because every one of the 7 Sema
sites' corresponding Codegen emission already called `genExpr(*stmt.expr)`
/ `genExpr(arg)` on exactly the annotated node, this one choke point fixed
all 7 call sites with zero further per-site Codegen changes.

`static_cast<T*>(voidExpr)` is always well-formed here regardless of how
nested `T` is (including `T` itself being a pointer, e.g. bridging into a
`Node PTR PTR` target) - the C++ standard permits `static_cast` from a
prvalue of type "pointer to void" to any object pointer type. The
annotation only ever fires when the *value*'s type is single-indirection
ANY PTR (a leaf case in `Codegen::cppType`, always exactly `void*`), so
the deeper `void**` vs `T**` case (a bare `ANY PTR PTR` value bridged
into a deeper typed target) never reaches this code path - correctly out
of scope, since that conversion was never valid in C either.

One confirmed, pre-existing, still-out-of-scope edge (found during design
review, not a regression from this fix): a BYREF pointer parameter is
codegen'd as a genuine C++ reference (`T*&`), and neither a `static_cast`
prvalue nor a bare `void*` lvalue can bind to it - BYREF pointer
parameters bridging from ANY PTR were, and remain, unsupported.

Verified: extended `tests/e2e/pointers` (previously only exercised the
*working* typed->ANY direction) to cover the fixed direction across a
plain variable assignment, a member/field assignment, a BYVAL
call-argument, and a return-assignment, all reusing the file's existing
`Node` linked-list fixture; full suite (38/38) passing; a clean
`-Wall -Wextra -Werror` rebuild, zero warnings; the original minimal
GTK-independent repro (`DIM np AS Node PTR : np = anyP`) recompiles and
runs correctly.

## Fixed: `ebpm`'s `LIBRARY_PATH` workaround broke every process on real Haiku hardware

Found while running the real Haiku verification for the fix above:
`e2e_pkg_lib_and_app_extern_lib` (added by the "forward a dependency's own
`Lib` clauses transitively" work, never previously checked on real Haiku
hardware) failed there, but nowhere else. Root cause had nothing to do with
that feature or with the `ANY PTR` fix - `tests/e2e_pkg/run_case.sh` passed
a fixture library's directory to `ebpm` via the real `LIBRARY_PATH`
environment variable (the one genuine mechanism available, since `ebpm`
itself had no manifest/CLI concept of an external system library's search
directory - see the two "Post-1.0" entries above). On Linux/macOS that's a
harmless, g++/clang++-only, link-time-only convention. On Haiku, the exact
same name is *also* consulted by the OS's own runtime_loader for dynamic
library resolution - pointing it at a directory holding only unrelated
static archives broke process startup entirely, confirmed by direct
reproduction on real Haiku hardware: even a bare `ls` crashed silently
(exit 3, zero output) with that `LIBRARY_PATH` set.

Fix: gave `ebpm` a real, first-class mechanism instead of relying on the
compiler's own environment-variable convention at all. A new
`EBASIC_LIBRARY_PATH` environment variable (`:`-separated, read once in
`build.cpp`'s new `externalLibraryDirs()`) is forwarded as `-L` to every
`ebc` invocation, in both `buildPackageWithDeps` (every package in the
graph) and `computeConsumerDirs` (`ebpm test`'s consumer-side compile) -
deliberately a distinct, ebasic-specific name so it can never collide with
anything OS- or toolchain-reserved, on any platform. `tests/e2e_pkg/
run_case.sh` now exports `EBASIC_LIBRARY_PATH` instead of `LIBRARY_PATH`;
documented in `docs/guide/ebpm.md`. This is also a genuine capability
improvement for real users, not just a test-only workaround - previously
there was no way at all for an `ebpm` package (e.g. a real GTK4 binding) to
tell `ebpm build` where its wrapped system library actually lives.

Verified: full suite (38/38) passing locally, a clean `-Wall -Wextra
-Werror` rebuild, and a second real Haiku hardware run - `ebpm build`/`ls`
both confirmed no longer crash with `EBASIC_LIBRARY_PATH` set to the same
directory that broke `LIBRARY_PATH`, and the full suite green on Haiku
including `e2e_pkg_lib_and_app_extern_lib`.

## `ebpm` central package index/registry (REG-0 through REG-9, in progress)

A Cargo-like `ebpm add <name>` experience: a central index of published
packages (each with real, multiple versions), real SemVer constraint
matching, and dependency-management commands (`add`/`remove`/`list`/
`search`/`update`) - un-deferring two design decisions this project made
early on ("central registry deferred indefinitely", "SemVer range
resolution deferred indefinitely"). See the approved plan for the full
design (resolver integration, lockfile reproducibility guarantee, index
format) - Implementation Notes land here per slice as each one completes.

### REG-0 Implementation Notes (gitdep.cpp cache-key fix, done)

Found and fixed *before* building anything registry-specific on top of it:
`gitdep.cpp`'s cache directory was keyed by git URL only
(`gitCacheRoot() / sanitizeForDirName(dep.git)`), with no ref/tag/commit
component. Two dependency edges naming the *same* URL at *different* refs
collided - the second `resolveGitDependency` call mutated the first edge's
already-checked-out working tree in place, and `resolve.cpp`'s
directory-keyed dedup meant the second edge's manifest was never even
re-read, silently leaving it pointed at stale, wrong-version content on
disk. Already a latent bug (no prior e2e case ever exercised two refs of
one URL), but the registry makes it *likely*: an index author naturally
publishes multiple versions of one package as multiple tags in one repo.

Fixed by widening the cache-directory key to include the manifest's
*declared* selector (`branch`/`tag`/`rev` - deliberately not the resolved
*pinned commit*, which differs between an unpinned first resolution and a
pinned repeat build of the exact same edge, and would otherwise force a
needless extra clone the moment a lockfile pin appears). Each distinct
`(url, declared ref)` pair now gets its own clone; the common "no ref at
all" case is untouched (same cache dir as before, fully backward
compatible).

Verified empirically before *and* after: temporarily reverted the fix and
confirmed the new `e2e_pkg_git_same_url_diff_refs` test (two path-adjacent
dependencies, `libv1`/`libv2`, both pointing at one fake bare repo tagged
`v1`/`v2`) fails exactly as predicted - the second library is never even
built, main.bas's `#include "libv2.iface.bas"` fails outright, because the
shared checkout thrashed between refs on every `git checkout` call. Restored
the fix, confirmed the same test passes; full suite (39/39) green, clean
`-Wall -Wextra -Werror` rebuild.

### REG-1 Implementation Notes (SemVer + VersionReq library, done)

New `pkg/src/semver.hpp/.cpp` - a strict `MAJOR.MINOR.PATCH` `SemVer` type
(comparable, no pre-release/build-metadata by deliberate scope cut) plus a
`VersionReq` type parsing a manifest requirement string's optional
`^`/`~`/`=` prefix and 1-3 dot-separated components, implementing the exact
Caret/Tilde/Exact table from the approved plan (including the `0.x.y`
caret edge cases - a `0.x` minor bump and a `0.0.x` patch bump are each
treated as breaking, matching real Cargo semantics) and `pickBestSatisfying`
(highest version in a list matching a requirement).

No existing unit-test-binary precedent exists in this codebase (every
other test drives a real compiled tool end-to-end via bash) - SemVer has no
CLI surface of its own until REG-4/REG-6 land, so a small, plain
assertion-based standalone binary (`pkg/src/semver_test.cpp`, registered as
ctest's `semver_unit`) was the simplest honest way to verify it in
isolation first, per the plan's "verify standalone before touching
anything else" instruction for this slice. Covers the full Caret/Tilde/
Exact table plus parse-error cases (partial exact requirement, 4-component
input, leading zeros, empty string) - all passed on the first run, no bugs
found needing a fix. Verified: full suite (40/40), clean `-Wall -Wextra
-Werror` rebuild.

### REG-2 Implementation Notes (manifest `version` field + shorthand, done)

`Dependency` (`manifest.hpp`) gains a `version` field; `parseDependencyEntry`
now accepts a bare-string shorthand (`dep = "^1.2.0"`, matching Cargo's own
`serde = "1.0"` convention) alongside the existing inline-table form (which
now also accepts `{ version = "..." }`), enforces a real three-way "exactly
one of `path`/`git`/`version`" exclusivity (loosened from the prior
two-way `path`/`git` check), rejects `version` combined with
`branch`/`tag`/`rev` (meaningless once an index has already picked a tag),
and syntax-validates the requirement string via REG-1's `parseVersionReq`
at load time.

Testable purely at the manifest layer (no resolver/index support exists
yet - that's REG-3/REG-4) via a new `tests/e2e_pkg/manifest_version_errors.
sh`, checked with a real `ebpm build` against nine synthetic, throwaway
manifests (no checked-in fixture directories - these are single-purpose,
inline-generated error cases): every rejected combination (`git`+`version`,
`path`+`version`, neither given, `version` with each of
`branch`/`tag`/`rev`, a malformed requirement in both shorthand and table
form, a partial `=` exact requirement) produces the expected error message
and a non-zero exit. The *valid* case (a well-formed registry dependency
actually resolving/building) is deliberately left untested until REG-4
lands real resolver support - asserting today's transient "falls through to
`resolveGitDependency` with an empty URL" behavior would just be testing
something about to change in the very next slice.

Verified: full suite (41/41) passing, clean `-Wall -Wextra -Werror`
rebuild - fully backward compatible (no existing manifest anywhere in this
repo's tests uses `version` in `[dependencies]`, so the three-way
exclusivity check never changes behavior for any of them).

### REG-3 Implementation Notes (index format + fetch/cache, done)

New `pkg/src/index.hpp/.cpp`: one TOML file per package
(`<indexDir>/<name>.toml`, a `[package]` name/description plus one
`[[versions]]` entry per published version, each a `SemVer` + a real git
source shaped exactly like a hand-written git dependency - `git` plus at
most one of `branch`/`tag`/`rev`). `indexUrl()` resolves in priority order:
the `EBASIC_INDEX_URL` env var (naming matches the `EBASIC_LIBRARY_PATH`
precedent from the earlier Haiku fix), then `~/.ebpm/config.toml`'s
`[registry] index = "..."`, then a hardcoded default pointing at this
project's own starter index repo (REG-8). `fetchIndexDir` clones/fetches
that repo into `~/.ebpm/cache/index/<sanitized-url>/`; `lookupPackage`/
`listAllPackages` parse it (the latter skips a malformed entry rather than
failing the whole listing, so `search` stays resilient to one bad file).

Also landed, as its own commit within this slice per the plan: factored
`gitdep.cpp`'s clone-or-fetch subprocess logic out into a shared
`cloneOrFetch()` (plus `homeDir()`/`sanitizeForDirName()`, both now
exported from `gitdep.hpp`) - a pure refactor with no behavior change,
verified by re-running the existing git-dependency e2e cases unchanged
before writing a single line of `index.cpp`.

Like SemVer (REG-1), the index has no CLI surface of its own yet (that's
REG-6/REG-7) - verified via a small standalone binary (`pkg/src/
index_test.cpp`) driven by a new `tests/e2e_pkg/run_index_case.sh`, which
stands up a real local bare "index" repo (the same technique
`run_git_case.sh` already uses for a fake git-dependency remote) seeded
with one well-formed package (two versions) and one deliberately malformed
one (missing its own `git` URL) - confirming `lookupPackage` succeeds/
fails exactly as expected for each, and `listAllPackages` finds the good
one while silently skipping the bad one.

Verified: full suite (42/42) passing, clean `-Wall -Wextra -Werror`
rebuild.

### REG-4 Implementation Notes (resolver integration, done - the crux slice)

`resolve.cpp`'s DFS gains a third per-dependency branch (alongside `path`
and `git`) for a `version`-only `Dependency`: look up the package in the
index (REG-3), pick the highest version satisfying the requirement
(REG-1's `pickBestSatisfying`), then resolve that version's own `git`/`tag`
via the *existing*, unmodified `resolveGitDependency` (a synthetic
`Dependency` built from the chosen `IndexVersionEntry` - identical shape to
a hand-written git dependency) - exactly the "zero changes to `gitdep.cpp`,
zero changes to `visit`'s own recursion" design the plan called for. A
registry-resolved package's own transitive dependencies (which might
themselves be path/git/further-registry) are walked by the exact same DFS
loop with no special-casing needed deeper in, since `visit` already reads
`ebasic.toml` fresh *after* a directory is concretely resolved, uniformly
for every dependency kind.

A new memo map, `resolvedRegistryVersions` (keyed by package **name**, not
directory - deliberately separate from `resolvedByDir`), makes this
deliberately non-backtracking: the first requirer of a given package name
picks its version; a later requirer elsewhere in the graph reuses that pick
if it's still compatible with their own requirement, or gets a hard,
clearly-messaged error naming both requirers and both requirements if not
("no version of 'regmathlib' satisfies both ^1.0 (required by app2) and
^2.0 (required by wrapper)"). `ResolvedPackage` gained `SourceKind` (`Root`/
`Path`/`Git`/`Registry`) and `version` fields, threaded through `visit`'s
signature itself (not derived after the fact), since a diamond dependency's
node is only ever pushed to `order` once, by whichever edge reaches it
first.

Verified via a new `tests/e2e_pkg/run_registry_case.sh`, standing up real
local bare "index" and "library" repos (the library tagged `v1.0.0`/
`v2.0.0` - a major bump) and exercising two scenarios against the same
seed: a happy path (`regmathlib = "^1.0"` correctly resolves to `1.0.0`,
excluding `2.0.0`, builds, and runs) and a genuine conflict (a diamond -
`app2` wants `regmathlib ^1.0` directly and `^2.0` transitively via
`wrapper` - rejected with the exact expected error). Both passed on the
first run. Full suite (43/43), clean `-Wall -Wextra -Werror` rebuild.

Known, deliberate gap left for REG-5: a registry dependency still consults
the index on *every* resolution, even when `ebasic.lock` already pins a
commit for it (unlike a plain `git` dependency's pinned rebuild, which
needs no live index/branch lookup at all) - because the lockfile doesn't
yet record which `git`/`tag` a registry pick resolved to, only `resolve.
cpp`'s in-memory synthetic `Dependency` knows that. REG-5 closes this gap.

### REG-5 Implementation Notes (lockfile extension, done)

`ResolvedPackage` gained `registryGit`/`registryRef` (alongside REG-4's
`sourceKind`/`version`); `writeLockfile` now emits `version =`/`git =`/
(if any) `ref =` for a `Registry`-sourced `[[package]]` entry, and
`readLockfilePins`'s return type grew from a bare `name -> commit` map to
`name -> Pin{commit, version, git, ref}`. `resolve.cpp`'s registry branch
now checks the pin *before* consulting the index: if a pinned version still
satisfies the *current* manifest requirement, it's reused directly (a
synthetic `Dependency` built straight from the pin, `resolveGitDependency`
called with zero index involvement) - if the requirement has since been
edited to something the pin no longer satisfies, it falls through to a
fresh index lookup instead, so a manifest edit always takes effect rather
than silently sticking to a stale pin.

**Real bug found and fixed while writing this slice's own test** (not a
regression from REG-0-4, but only reachable once something re-fetches an
already-cloned repo more than once - which none of REG-0/REG-3/REG-4's own
tests happened to exercise): `cloneOrFetch`'s `git fetch` branch only
updated the remote-tracking refs, never the actual checked-out working
tree - a git dependency's own caller (`resolveGitDependency`) always
performs its own explicit `git checkout <ref>` right after, masking this
completely, but the package index's caller (`fetchIndexDir`) does not, so
`lookupPackage` kept reading whatever content existed at the *first* clone
forever after, never seeing anything pushed later. Fixed by having
`cloneOrFetch` itself bring the working tree in line with `origin/HEAD`
after a fetch (`git reset -q --hard origin/HEAD` - the `-q` needed because
a first attempt leaked `reset`'s own "HEAD is now at ..." confirmation
message onto stdout, corrupting two unrelated git-dependency e2e tests'
captured output; caught immediately by the full suite and fixed before
moving on). Always safe for the git-dependency caller too, since its own
subsequent explicit checkout simply overrides it.

Verified via a new `tests/e2e_pkg/run_registry_lock_case.sh`, which
performs four builds against one evolving fake index/library: (1) a fresh
resolution picks the only available version; (2) after the library/index
both gain a newer, still-compatible release, a repeat build stays pinned
to the original version; (3) the same repeat build still succeeds with
`EBASIC_INDEX_URL` pointed at a nonexistent path, proving zero index
consultation happens once pinned; (4) editing the manifest's own
requirement to something the pin no longer satisfies re-resolves to the
newer version on the very next build, and the rebuilt program's real
output confirms it. All four passed after the `cloneOrFetch` fix above.
Full suite (44/44), clean `-Wall -Wextra -Werror` rebuild.

### REG-6 Implementation Notes (`ebpm add` / `ebpm remove`, done)

`main.cpp` gained `cmdAdd`/`cmdRemove` plus small text-editing helpers
(`trimSpaces`, `readLines`, `writeLines`, `dependencyNameExists`,
`insertDependencyLine`, `removeDependencyLine`). Both commands edit
`ebasic.toml` at the text level rather than via toml++'s (read-only, in
this codebase) parser+reserializer, so a user's existing formatting/
comments in the file survive untouched - `add` finds-or-creates a
`[dependencies]` section and inserts one `name = "^x.y.z"` line right
after its header; `remove` deletes exactly that single line, refusing (with
a clear message pointing at manual editing) if the named entry isn't in
that simple single-line shorthand form. `add` checks for a duplicate name
across `manifest.dependencies` *and every* `manifest.targetDependencies`
entry, not just `effectiveDependencies()` (which only reflects the current
platform) - a name already present in a different platform's
`[target.*.dependencies]` section is still rejected. With no `--version`,
the highest available index version is picked; with `--version <req>`, the
highest version satisfying that requirement is picked via REG-1's
`pickBestSatisfying`.

**Not a code bug, a wrong test expectation** (found while chasing what
looked like `--version` being silently ignored): the test asserted that
`ebpm add mylib --version "^1.0"` against an index offering `1.0.0` and
`1.2.0` should pick `1.0.0`. It doesn't, and correctly so - caret
requirements are deliberately broad (`^1.0` means `>=1.0.0, <2.0.0`, per
REG-1's own table), so `1.2.0` genuinely satisfies `^1.0` and is correctly
the highest satisfying pick. `pickBestSatisfying` was never wrong; the test
needed `~1.0.0` (`>=1.0.0, <1.1.0`) to actually exercise "picks a specific,
non-latest version." Fixed by changing the test's requirement string, not
the product code - a good reminder to double check a failing test's own
expectation against the spec before assuming the implementation is at
fault.

Verified via `tests/e2e_pkg/run_add_remove_case.sh` against a real fake
index + two-tag library repo: highest-version pick with no `--version`,
duplicate-add rejection, the added dependency actually building and
running, `remove` deleting the entry, re-remove rejection, a
`--version`-constrained pick landing on the older compatible version, and
an unknown package name failing clearly. Full suite (46/46), clean
`-Wall -Wextra -Werror` rebuild.

### REG-7 Implementation Notes (`ebpm list` / `ebpm search` / `ebpm update`, done)

`main.cpp` gained `cmdList`, `cmdSearch`, `cmdUpdate`. `list` reuses
`resolveDependencyGraph`'s already-computed `order` directly - one line per
resolved dependency (its own last entry, always the root package itself, is
skipped), annotated by `SourceKind`: a path shows its directory, a git or
registry dependency shows a short commit prefix, and a registry dependency
additionally shows its picked version. Deliberately a flat annotated list
rather than a nested ASCII tree, matching the plan's own stated scope cut.
`search <term>` calls `listAllPackages` and substring-matches name/
description, Cargo's own `cargo search` output shape (`name - description`).

`update [<name>]` is the one genuinely new mechanism: with no existing way
to make `resolveDependencyGraph` ignore a specific pin, a new
`removeLockfilePinBlocks` helper deletes just the named package(s)'
`[[package]]` block(s) (plus the blank line `writeLockfile` always emits
before one) straight out of `ebasic.lock` at the text level - the same
"never round-trip through toml++, since the file may need to keep some
entries verbatim" reasoning as `add`/`remove`'s manifest edits, applied to
the lockfile instead. With no pin left for that name,
`resolveDependencyGraph`'s existing registry branch falls through to a
fresh index lookup exactly as if the dependency had never been built
before - no resolver changes needed at all. `update` with a name validates
that name is actually a registry ("version") dependency somewhere in the
manifest first (a `path`/`git` dependency has no index-picked version to
re-pick); with no name, it targets every registry dependency in the
manifest. Reports each target's before/after version by reading
`readLockfilePins` both before removing the pin and after the
`buildPackageWithDeps` rebuild that follows - "Updating X vA -> vB",
"X is already up to date (vB)", or "Locked X vB" (no prior pin existed at
all, e.g. right after `ebpm add` but before any build).

Verified via a new `tests/e2e_pkg/run_list_search_update_case.sh`, against
a real fake index + two-tag library repo, with the app declaring
`mylib = "^1.0"` directly (independent of REG-6's own `add`/`remove`
coverage): `search` finds/doesn't-find; `list` shows the resolved version
before any build even exists; a real build pins v1.0.0; after the index
gains v1.2.0, `list` stays pinned to v1.0.0 (proving `list` itself honors
REG-5's reproducibility guarantee, not just `build`); `update mylib`
reports the v1.0.0 -> v1.2.0 transition and the rebuilt program's real
output confirms it; a second `update` (no name) reports "already up to
date"; `update` on a non-registry name fails clearly. Full suite (47/47),
clean `-Wall -Wextra -Werror` rebuild.

## Testing Strategy

- **Golden-file e2e tests** (primary): `tests/e2e/<case>/input.bas` + `expected.stdout` + `expected.exit`, run through the full `ebc → g++ → execute` pipeline and diffed.
- **Per-stage unit tests**: lexer (source → token list), parser (source → AST S-expression dump, golden), sema (source → expected diagnostics, golden), codegen (typed-AST fixture → generated C++, prefer compiling+running the snippet over pure text diff since text diffing generated C++ is brittle).
- **Differential testing against real `fbc`**: since the user maintains an `fbc` fork, run the same `.bas` corpus through both `fbc` and `ebc`, diff stdout/exit codes — highest-leverage fidelity check, add as an optional CI job (not required where `fbc` isn't installed) starting around M1.
- **Regression discipline**: every bug fix adds a minimal repro to the e2e golden corpus first.
- **Fuzzing** (post-M2/M3): grammar-aware parser fuzzing; later, differential fuzzing vs. `fbc` on small generated programs.
- **CI matrix**: Linux from M0; Windows/macOS added once M1/M2 stabilize; Haiku has no native GitHub Actions image — needs a self-hosted/VM runner, flagged as infra work to solve before M8.

## Key Open Design Questions to Revisit Before/During Implementation

- **String representation**: ref-counted/COW `BString` (recommended, closest to FB's actual runtime behavior) vs. always-copy value type vs. true tracing GC. Affects every generated line touching strings — confirm before M1.
- **`ON ERROR GOTO` mapping**: C++ exceptions internally vs. manual error-state/`longjmp` — needs an ADR before M1/M2 since FB's error model doesn't map cleanly onto C++ stack unwinding.
- **`GOTO` into scopes with non-trivial destructors**: C++ forbids this; needs a codegen strategy (hoisting/restructuring) decided before GOTO support lands in M1.
- **Precompiled stdlib mechanics (M6)**: prebuilt static/shared libs at link time, precompiled headers, or both — has real distribution/release-pipeline implications, scope concretely with the user when M6 starts.
- **`ebpm` manifest format/name**: TOML recommended (matches "cargo-like"); central registry deferred indefinitely (git/path deps sufficient for a long while).
- **Extended FFI ambition**: README says "C/C++ headers/libs" — plan targets C-only for M4 (assumption above); revisit whether arbitrary C++ class/template reuse is truly wanted as a later milestone, since that's substantially harder (mangling, templates, overload resolution across the FFI boundary).

## First Files to Create (Implementation Start)

- `CMakeLists.txt` (top-level)
- `compiler/src/driver/main.cpp`
- `compiler/src/lexer/lexer.{cpp,hpp}`
- `compiler/src/parser/parser.cpp`, `compiler/src/ast/ast.hpp`
- `compiler/src/codegen/codegen.cpp`
- `runtime/include/ebasic/runtime/bstring.hpp`
- `tests/e2e/hello_world/input.bas`

## Verification (once implementation begins)

- `cmake --preset linux-gcc && cmake --build --preset linux-gcc` builds `ebc` cleanly.
- `ebc tests/e2e/hello_world/input.bas -o /tmp/hello && /tmp/hello` prints `Hello, world!` with exit code 0.
- CI runs the full `tests/e2e` golden suite on every push; new constructs are not considered "done" until they have a corresponding golden test.
