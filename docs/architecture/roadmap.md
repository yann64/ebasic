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
| **M4** | C interop: extended `IMPORT`/`EXTERN`-style syntax binding to C headers/libs, linking against `.a`/`.so`/`.lib` | Golden test links a small real C library and calls it from `.bas` |
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
