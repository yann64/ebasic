# `ebasic_lsp` - the language server

A [Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
server for eBasic - real-time diagnostics, hover, go-to-definition, outline,
and completion in any LSP-capable editor, aware of
[`ebpm`](ebpm.md) dependencies (a `.bas` file can `#include` another
package's auto-generated `<name>.iface.bas`). Speaks the real protocol
(`Content-Length`-framed JSON-RPC over stdio) - any LSP client can launch
it, not just the one documented below.

**Status: all six planned slices implemented** (built up incrementally -
see `docs/architecture/roadmap.md`'s "LSP Plan Summary" and per-slice
Implementation Notes for how each one was verified).

## What's implemented

**LSP-1 (transport + document sync):**

- The `initialize`/`initialized`/`shutdown`/`exit` handshake, advertising
  `textDocumentSync: Full`.
- `textDocument/didOpen`/`didChange`/`didClose` - the server keeps an
  in-memory copy of every open document's current text (full-text replace
  on every change; files are small enough that incremental sync isn't
  worth the complexity).

**LSP-2 (diagnostics):**

- Every `didOpen`/`didChange` re-runs the real compile pipeline
  (Preprocessor -> Lexer -> Parser -> Sema) over the document's current
  text and publishes `textDocument/publishDiagnostics` - real Sema errors
  and warnings, with the same messages `ebc` itself would report. A
  diagnostic inside an `#include`d file is published against *that* file's
  own URI, not the document you're editing.
- Diagnostic ranges are one character wide (eBasic's AST tracks a single
  point per node, not a start/end span), the closest honest approximation
  available today.

**LSP-3 (outline + hover):**

- `textDocument/documentSymbol` - one entry per top-level TYPE/UNION/CONST/
  ENUM/NAMESPACE/free SUB/FUNCTION, in source order.
- `textDocument/hover` - the resolved signature/type of the symbol under
  the cursor (a SUB/FUNCTION's real parameter/return types, a TYPE's
  EXTENDS base, a variable's declared type), sourced from Sema's own
  resolved symbol table (`Sema::index()`), not a second, potentially
  drifting name-resolution pass. Available even while a document has a
  body-level type error elsewhere, since module-level signatures are
  always fully registered before any statement body is checked.

**LSP-4 (go-to-definition + find-references):**

- `textDocument/definition` - jumps to where the symbol under the cursor
  was declared (its real `Sema`-recorded declaration site, not a guess).
- `textDocument/references` - every use of that symbol in the current
  compilation unit (including any `#include`d file, since preprocessing
  flattens them into one `Module` before Sema ever runs), plus the
  declaration itself when the request's `includeDeclaration` is set.

**LSP-5 (`ebpm`-awareness):**

- A document belonging to an `ebpm` package (walked up from its own
  directory looking for `ebasic.toml`, the same way `ebpm` itself finds
  "the package rooted at the current directory") gets its dependencies'
  `-I` search path computed the exact same way a real `ebpm build` would
  (`ebasic_pkg`'s own `resolveDependencyGraph`/`computeConsumerDirs`,
  reused in-process) - so `#include "dep.iface.bas"` resolves correctly.
- If a dependency hasn't been built yet (its `target/<name>.iface.bas`
  doesn't exist), the resulting diagnostic gets an actionable hint
  appended: *"...- dependency 'mathlib' hasn't been built yet; run `ebpm
  build`"* - instead of a bare, confusing file-not-found error.
- Hover and go-to-definition fall back to a dependency's own parsed
  interface when a symbol isn't found in the current document - landing a
  go-to-definition in the dependency's real, on-disk generated interface
  file, the same convention many language servers use for vendored/
  pre-built dependencies.
- **Known limitation**: dependency resolution (which can run a real `git
  fetch` for a `git` dependency) is deliberately only ever re-run on
  `didOpen`, never per-keystroke on `didChange` - a `workspace/
  didChangeWatchedFiles` handler exists to invalidate a package's cached
  resolution, but this server doesn't yet dynamically register interest in
  it (`client/registerCapability`), so most editors won't send it
  unprompted today. After running `ebpm build`, reopen the file (or
  restart the server) to pick up the fresh interface - a real, honest gap,
  not a silent one.

**LSP-6 (completion):**

- `textDocument/completion` offers every reserved keyword, every in-scope
  local/global/procedure/`TYPE` name, and every name exposed by the
  package's own resolved dependencies - not context-sensitive (no attempt
  to filter by grammatical position). A symbol's completion label is its
  *canonical* (lowercased) form, unlike hover/go-to-definition - `SemaIndex`
  doesn't retain a declaration's original casing, and completion has no
  on-screen token to borrow casing from the way a hover/go-to-definition
  request does.
- If the document's current text has a syntax error (mid-edit), completion
  falls back to the last version that parsed successfully, rather than
  going blank while you're still typing.

This completes all six planned slices (see `docs/architecture/roadmap.md`'s
"LSP Plan Summary"). A request for an unrecognized method still gets a
real JSON-RPC `MethodNotFound` (`-32601`) error, not silence; a malformed
request for an *implemented* method gets `InvalidParams` (`-32602`), never
a crash.

## Trying it in Neovim

Neovim's built-in LSP client needs no plugin for a bare-bones setup like
this - add to your config (e.g. `init.lua`):

```lua
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'ebasic', -- or whatever filetype you associate with *.bas
  callback = function(args)
    vim.lsp.start({
      name = 'ebasic_lsp',
      cmd = { '/path/to/build/linux-gcc/lsp/ebasic_lsp' },
      root_dir = vim.fs.dirname(vim.fs.find({ 'ebasic.toml', '.git' }, { upward = true })[1]),
    })
  end,
})
```

Open a `.bas` file and check `:LspInfo` shows `ebasic_lsp` attached - you
should now see real error/warning squiggles from Sema as you type, an
outline via `:lua vim.lsp.buf.document_symbol()`, and hover via
`K`/`vim.lsp.buf.hover()`.

## Verifying without an editor

`tests/lsp/smoke_test.sh`, `tests/lsp/diagnostics_test.sh`,
`tests/lsp/symbols_test.sh`, `tests/lsp/definition_references_test.sh`,
`tests/lsp/pkgaware_test.sh`, and `tests/lsp/completion_test.sh` drive the
server directly over its real stdio transport (the same Content-Length
JSON-RPC framing any client speaks) and
run as part of the normal test suite (`ctest -R lsp_`) - useful for
confirming the protocol contract works without needing an LSP-capable
editor on hand. `pkgaware_test.sh` in particular runs a real `ebpm build`
against the `tests/e2e_pkg/lib_and_app` fixture to verify both the
"dependency not built yet" hint and the real, correct cross-package
diagnostics/hover/go-to-definition once it is built.

## See also

- [`ebpm`](ebpm.md) - the LSP resolves `#include`d dependency interfaces
  the same way `ebpm build` does (from LSP-5 onward).
- [`ebc`](ebc.md)
