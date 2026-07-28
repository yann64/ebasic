# `docgen` - documentation generator

```
usage: docgen <input.bas> -o <output-dir>
       docgen [-v | --version] [-h | --help]
  produces <output-dir>/index.md and <output-dir>/index.html
```

Extracts [`'''`-marked doc comments](../reference/doc-comments.md) from
`<input.bas>` and renders them, alongside each declaration's own signature,
into a browsable page.

```sh
$ docgen examples/documented_mathlib.bas -o docs_out
$ cat docs_out/index.md
```

Produces both `index.md` (plain Markdown) and `index.html` (a small,
self-contained static page - not a general CommonMark renderer: doc-comment
prose is split into paragraphs and HTML-escaped, with no bold/italic/links/
lists rendering; a deliberate scope cut, not an oversight).

Declarations are grouped into sections by kind - **Types**, **Constants**,
**Functions/Subs** - in source order within each section. Only
`SUB`/`FUNCTION`/`TYPE`/`UNION`/`NAMESPACE`/`CONST`/`ENUM` are documentable;
an undocumented one still appears, marked `(undocumented)`, rather than
being silently omitted - so gaps in your own documentation coverage are
visible instead of hidden.

`docgen` reuses the exact same Preprocessor/Lexer/Parser eBasic's own
compiler uses (no second, drifting implementation) - see
[Developer Documentation](../developer/architecture.md#shared-libraries-why-ebc-ebpm-docgen-and-ebasic_lsp-arent-four-silos).
It never runs Sema, so it has no type information beyond what the parser
itself resolves structurally - this is why, for example, an `ENUM`
member's value is never shown (auto-increment resolution is Sema's job).

## See also

- [Doc Comments (`'''`) reference](../reference/doc-comments.md)
- [Getting Started](getting-started.md)
