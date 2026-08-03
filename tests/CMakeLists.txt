# Tiny fixture libraries for the M4 (C/C++ interop) e2e tests - real,
# separately-compiled static libs to link against, not just calls into
# ebc's own runtime. Placed in a fixed subdirectory of the build tree so
# run_case.sh can point ebc at it via -L without guessing CMake's default
# archive output location.
set(EBASIC_FIXTURE_LIB_DIR ${CMAKE_CURRENT_BINARY_DIR}/fixtures)

add_library(ebasic_fixture_c STATIC fixtures/c/fixture.c)
set_target_properties(ebasic_fixture_c PROPERTIES
    OUTPUT_NAME ebfixturec
    ARCHIVE_OUTPUT_DIRECTORY ${EBASIC_FIXTURE_LIB_DIR}
)

add_library(ebasic_fixture_cpp STATIC fixtures/cpp/fixture.cpp)
set_target_properties(ebasic_fixture_cpp PROPERTIES
    OUTPUT_NAME ebfixturecpp
    ARCHIVE_OUTPUT_DIRECTORY ${EBASIC_FIXTURE_LIB_DIR}
)

add_test(
    NAME e2e_hello_world
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/hello_world
)

add_test(
    NAME e2e_arithmetic
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/arithmetic
)

add_test(
    NAME e2e_types_and_operators
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/types_and_operators
)

add_test(
    NAME e2e_control_flow
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/control_flow
)

add_test(
    NAME e2e_const_enum_arrays_preprocessor
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/const_enum_arrays_preprocessor
)

add_test(
    NAME e2e_sub_function
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/sub_function
)

add_test(
    NAME e2e_redim
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/redim
)

add_test(
    NAME e2e_gosub
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/gosub
)

add_test(
    NAME e2e_include
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/include
)

add_test(
    NAME e2e_platform_macros
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/platform_macros
)

add_test(
    NAME e2e_type_records
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/type_records
)

add_test(
    NAME e2e_namespace
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/namespace
)

add_test(
    NAME e2e_pointers
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/pointers
)

add_test(
    NAME e2e_unions
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/unions
)

add_test(
    NAME e2e_classes
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/classes
)

add_test(
    NAME e2e_inheritance
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/inheritance
)

add_test(
    NAME e2e_properties
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/properties
)

add_test(
    NAME e2e_operators
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/operators
)

add_test(
    NAME e2e_extern_c
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/extern_c ${EBASIC_FIXTURE_LIB_DIR}
)

add_test(
    NAME e2e_extern_cpp
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/extern_cpp ${EBASIC_FIXTURE_LIB_DIR}
)

add_test(
    NAME e2e_extern_handle
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/extern_handle ${EBASIC_FIXTURE_LIB_DIR}
)

add_test(
    NAME e2e_function_pointers
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/function_pointers ${EBASIC_FIXTURE_LIB_DIR}
)

add_test(
    NAME e2e_default_params
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/default_params
)

add_test(
    NAME e2e_default_params_errors
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/default_params_errors.sh $<TARGET_FILE:ebc>
)

add_test(
    NAME e2e_string_library
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/string_library
)

add_test(
    NAME e2e_file_library
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_case.sh $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e/file_library
)

# M5: a parallel golden-test harness (tests/e2e_pkg/) driving ebpm rather
# than ebc directly - each case dir is itself a fixture package (ebasic.toml
# + src/) plus an expected.stdout, round-tripped through `ebpm run`.
add_test(
    NAME e2e_pkg_simple_bin
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/simple_bin
)

# The actual M5 milestone bar: a lib + an app depending on it (path
# dependency), built/run end-to-end via `ebpm build && ebpm run`.
add_test(
    NAME e2e_pkg_lib_and_app
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/lib_and_app myapp
)

# A [lib] package's own Extern "C" Lib clause (wrapping a real, separately-
# compiled C library) must reach a downstream *consumer*'s final link step
# too, even though the consumer only ever #includes the auto-generated
# wraplib.iface.bas (which never mentions "ebfixturec" itself - see ebc's
# own .libs sidecar file and build.cpp's appendLibsSidecar).
add_test(
    NAME e2e_pkg_lib_and_app_extern_lib
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/lib_and_app_extern_lib myapp run ${EBASIC_FIXTURE_LIB_DIR}
)

# A derived, plain-data TYPE (EXTENDS, no methods anywhere in its chain)
# must be exported across a --lib boundary along with its base(s) - see
# generateLibraryInterface's isExportablePlainData/emitType in codegen.cpp.
add_test(
    NAME e2e_pkg_lib_and_app_extends
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/lib_and_app_extends myapp
)

# A top-level CONST/ENUM must be exported across a --lib boundary too - see
# generateLibraryInterface's new CONST/ENUM branch in codegen.cpp.
add_test(
    NAME e2e_pkg_lib_and_app_consts
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/lib_and_app_consts myapp
)

# OS-conditional dependencies: [target.<os>.dependencies] resolves only the
# current platform's own section (proven by only its lib actually getting
# built - see collectTransitiveDeps/effectiveDependencies in build.cpp/
# manifest.cpp) alongside the always-built [dependencies]. Deliberately
# platform-independent expected output (see app/src/main.bas) so this same
# fixture passes unchanged on all four CI platforms.
add_test(
    NAME e2e_pkg_target_deps
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/target_deps app
)

# M5d: a git dependency, resolved against a real local bare repo the test
# itself creates (git init --bare + one commit) - exercises the real
# clone/fetch/checkout code path with no real-network dependency.
add_test(
    NAME e2e_pkg_lib_and_app_git
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_git_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/lib_and_app_git
)

# REG-0: two dependency edges naming the same git URL at different refs
# (tags) must resolve independently, not silently share one mutable
# checkout - see gitdep.cpp's resolveGitDependency cache-key comment.
add_test(
    NAME e2e_pkg_git_same_url_diff_refs
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_git_multiref_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/git_same_url_diff_refs
)

# REG-2: a [dependencies] entry's new `version` alternative (registry
# dependency) - every rejected path/git/version/branch/tag/rev combination
# and malformed requirement string, checked via a real `ebpm build`.
add_test(
    NAME e2e_pkg_manifest_version_errors
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/manifest_version_errors.sh $<TARGET_FILE:ebpm>
)

# REG-3: index fetch/cache/parse against a real local bare "index" repo.
add_test(
    NAME e2e_pkg_index_fetch
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_index_case.sh $<TARGET_FILE:index_test>
)

# Regression test for the Windows-only lockfile TOML-escaping bug found
# via REG-5's own testing - see lockfile_test.cpp.
add_test(
    NAME lockfile_toml_escaping
    COMMAND $<TARGET_FILE:lockfile_test>
)

# REG-4: a real registry dependency end to end (happy path + conflict
# detection), against real fake "index" and "library" bare repos.
add_test(
    NAME e2e_pkg_registry_dep
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_registry_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/registry_dep
)

# REG-5: a registry dependency's lockfile reproducibility guarantee - a
# repeat build stays pinned even as the index/upstream move, a pinned
# rebuild needs no index consultation at all, and a manifest requirement
# edit that no longer fits the pin re-resolves on the next build.
add_test(
    NAME e2e_pkg_registry_lock_reuse
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_registry_lock_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/registry_lock_reuse
)

# REG-6: `ebpm add`/`ebpm remove` end to end (index lookup, --version,
# manifest text editing both directions, duplicate/not-found errors).
add_test(
    NAME e2e_pkg_add_remove
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_add_remove_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/add_remove_dep
)

# REG-7: `ebpm list`/`ebpm search`/`ebpm update` end to end (dependency-tree
# output, index-browse output, re-resolve-ignoring-the-pin behavior).
add_test(
    NAME e2e_pkg_list_search_update
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_list_search_update_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/list_search_update
)

# M5e: `ebpm test` on a [lib] package with one passing tests/*.bas file.
add_test(
    NAME e2e_pkg_lib_with_tests
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/run_case.sh $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_pkg/lib_with_tests . test
)

# M7b: docgen against a small fixture with a documented CONST/FUNCTION, an
# undocumented FUNCTION, and a plain (non-doc) comment that must not be
# attached - the Markdown output is diffed (the HTML output is hand-
# verified instead, per the M7 plan).
add_test(
    NAME e2e_docgen_simple
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/e2e_docgen/run_case.sh $<TARGET_FILE:docgen> ${CMAKE_CURRENT_SOURCE_DIR}/e2e_docgen/simple
)

# Version reporting + --help: -v/--version/-h/--help on all three binaries.
add_test(
    NAME cli_version_help
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/cli/version_help.sh $<TARGET_FILE:ebc> $<TARGET_FILE:ebpm> $<TARGET_FILE:docgen>
)

# REG-1: standalone SemVer/VersionReq unit test - see semver_test.cpp.
add_test(
    NAME semver_unit
    COMMAND $<TARGET_FILE:semver_test>
)

# LSP-1: a protocol-level smoke test - speaks real Content-Length-framed
# JSON-RPC over ebasic_lsp's stdio (initialize/didOpen/didChange/didClose/
# shutdown/exit, plus the unimplemented-method and unclean-disconnect exit-
# code paths) - see tests/lsp/smoke_test.sh's own comment for why this
# substitutes for interactive editor testing in CI/this sandbox.
add_test(
    NAME lsp_smoke
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/lsp/smoke_test.sh $<TARGET_FILE:ebasic_lsp>
)

# LSP-2: publishDiagnostics - a real Sema error, a didChange that fixes it,
# and a diagnostic inside an #include'd file routed to that file's own URI.
add_test(
    NAME lsp_diagnostics
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/lsp/diagnostics_test.sh $<TARGET_FILE:ebasic_lsp>
)

# LSP-3: documentSymbol (outline) and hover, backed by Sema's own resolved
# symbol table (Sema::index()), not a second name-resolution pass.
add_test(
    NAME lsp_symbols
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/lsp/symbols_test.sh $<TARGET_FILE:ebasic_lsp>
)

# LSP-4: go-to-definition + find-references.
add_test(
    NAME lsp_definition_references
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/lsp/definition_references_test.sh $<TARGET_FILE:ebasic_lsp>
)

# LSP-5: ebpm-awareness - the "dependency not built yet" hint, then real
# cross-package diagnostics/hover/go-to-definition once a real `ebpm build`
# has produced the dependency's interface.
add_test(
    NAME lsp_pkgaware
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/lsp/pkgaware_test.sh $<TARGET_FILE:ebasic_lsp> $<TARGET_FILE:ebpm> $<TARGET_FILE:ebc>
)

# LSP-6: completion (keywords + in-scope symbols), including the "last
# good parse" fallback when the current text has a syntax error.
add_test(
    NAME lsp_completion
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/lsp/completion_test.sh $<TARGET_FILE:ebasic_lsp>
)
