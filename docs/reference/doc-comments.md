# Doc Comments (`'''`)

A `'''`-marked comment (three or more leading apostrophes) documents the
declaration immediately following it, extracted by the
[`docgen`](../guide/docgen.md) tool into Markdown/HTML. This is eBasic's own
convention (FreeBASIC has no equivalent) - chosen after real prior art
(VB.NET's XML doc-comment marker follows the same "distinguish from a plain
comment by repeating the marker" idea).

```basic
''' Squares a number.
''' Returns n multiplied by itself.
FUNCTION Square(n AS INTEGER) AS INTEGER
    Square = n * n
END FUNCTION
```

- A **plain** `'` or `''` comment is an ordinary comment - discarded
  entirely, never attached to anything.
- **Multiple consecutive `'''` lines** join into one doc comment
  (newline-separated). They must be perfectly consecutive and immediately
  followed by the declaration they document - **a blank line anywhere from
  the first `'''` line through the declaration itself is a parse error**,
  not a silent split into separate comments. A blank line *before* the
  first `'''` line is fine (ordinary blank-line tolerance applies there).
- Only certain declaration kinds can carry a doc comment: `SUB`, `FUNCTION`,
  `TYPE`, `UNION`, `NAMESPACE`, `CONST`, and `ENUM` - the same "public API
  surface" set `ebpm --lib`'s auto-generated interface file exports. A
  `'''` line immediately preceding anything else (e.g. an `IF` or a plain
  `DIM`) is still consumed as a doc comment token but has nothing
  documentable to attach to.
- A declaration with no doc comment isn't omitted from `docgen`'s output -
  it appears marked `(undocumented)`, so documentation gaps are visible
  rather than silently hidden.

```basic
' This one is intentionally left undocumented, to show docgen's
' "(undocumented)" fallback in the generated output.
FUNCTION Cube(n AS INTEGER) AS INTEGER
    Cube = n * n * n
END FUNCTION
```

See [the end-user guide's `docgen` page](../guide/docgen.md) for how to
actually run the tool and what its output looks like.

## See also

- [End-user guide: `docgen`](../guide/docgen.md)
