# `ebasic_lsp` - the language server

A [Language Server Protocol](https://microsoft.github.io/language-server-protocol/)
server for eBasic - real-time diagnostics, hover, go-to-definition, outline,
and completion in any LSP-capable editor, aware of
[`ebpm`](ebpm.md) dependencies (a `.bas` file can `#include` another
package's auto-generated `<name>.iface.bas`). Speaks the real protocol
(`Content-Length`-framed JSON-RPC over stdio) - any LSP client can launch
it, not just the one documented below.

**Status: work in progress, built up in slices.** This page describes
what's implemented so far; it grows with each slice rather than being
written all at once.

## What's implemented so far (LSP-1: transport + document sync)

- The `initialize`/`initialized`/`shutdown`/`exit` handshake, advertising
  `textDocumentSync: Full`.
- `textDocument/didOpen`/`didChange`/`didClose` - the server keeps an
  in-memory copy of every open document's current text (full-text replace
  on every change; files are small enough that incremental sync isn't
  worth the complexity).
- No diagnostics, hover, completion, etc. yet - those are later slices (see
  `docs/architecture/roadmap.md`'s "LSP Plan Summary" for the full slice
  list). A request for an unimplemented method gets a real JSON-RPC
  `MethodNotFound` (`-32601`) error, not silence.

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

Open a `.bas` file and check `:LspInfo` shows `ebasic_lsp` attached. At this
slice there's nothing user-visible to see yet beyond the attachment itself
- diagnostics land in LSP-2.

## Verifying without an editor

`tests/lsp/smoke_test.sh` drives the server directly over its real stdio
transport (the same Content-Length JSON-RPC framing any client speaks) and
is run as part of the normal test suite (`ctest -R lsp_smoke`) - useful for
confirming the handshake/document-sync/exit-code contract works without
needing an LSP-capable editor on hand.

## See also

- [`ebpm`](ebpm.md) - the LSP resolves `#include`d dependency interfaces
  the same way `ebpm build` does (from LSP-5 onward).
- [`ebc`](ebc.md)
