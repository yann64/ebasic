#pragma once

#include "ast/ast.hpp"

#include <string>

namespace docgen {

/// Renders `module`'s documentable top-level declarations - TYPE/UNION,
/// CONST, ENUM, NAMESPACE, and free (non-method - see ast.hpp's `ownerType`)
/// SUB/FUNCTION, grouped into one section per kind, in source order within
/// each section - as a single Markdown document titled `title`. An
/// undocumented declaration (no `'''` doc comment - see
/// Parser::isDocumentableKind) still appears, marked "(undocumented)"
/// rather than silently omitted, so gaps in documentation coverage are
/// visible instead of hidden.
std::string renderMarkdown(const ebasic::Module& module, const std::string& title);

/// Same content as renderMarkdown, as a small, self-contained static HTML
/// page - deliberately not a general CommonMark engine: doc-comment prose
/// is split into paragraphs (blank-line-separated) and HTML-escaped; no
/// bold/italic/links/lists rendering. A documented scope cut (see the M7
/// plan), not an oversight - extending to full Markdown rendering is a
/// natural, separate future improvement.
std::string renderHtml(const ebasic::Module& module, const std::string& title);

} // namespace docgen
