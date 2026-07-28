#pragma once

#include <string>

namespace ebasic::lsp {

/// Converts a `file://` URI (as sent in every `textDocument.uri` this
/// server receives) to a plain, percent-decoded filesystem path - handling
/// both a POSIX absolute path (`file:///home/x/f.bas` -> `/home/x/f.bas`)
/// and a Windows drive-letter one (`file:///C:/Users/x/f.bas` ->
/// `C:/Users/x/f.bas`, RFC 8089's own form: the extra leading slash before
/// a drive letter is stripped). Returns `uri` unchanged if it doesn't start
/// with "file://" - every URI this server is given is expected to be one.
std::string uriToPath(const std::string& uri);

/// The inverse of uriToPath - builds a `file://` URI from an absolute
/// filesystem path (backslashes normalized to `/`, characters a URI can't
/// contain literally percent-encoded).
std::string pathToUri(const std::string& path);

} // namespace ebasic::lsp
