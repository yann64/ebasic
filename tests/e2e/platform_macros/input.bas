' M-post-packaging: exactly one platform macro is auto-defined, matching
' real FreeBASIC's own naming (__FB_WIN32__/__FB_LINUX__/__FB_DARWIN__),
' plus __FB_HAIKU__ (eBasic's own extension - Haiku isn't a real
' FreeBASIC target). This program runs unchanged on all four CI
' platforms - its expected output is deliberately platform-independent
' (which one is defined differs per platform; that exactly one is
' defined does not), so a single expected.stdout works everywhere.

DIM count AS INTEGER
count = 0
#ifdef __FB_WIN32__
count = count + 1
#endif
#ifdef __FB_LINUX__
count = count + 1
#endif
#ifdef __FB_DARWIN__
count = count + 1
#endif
#ifdef __FB_HAIKU__
count = count + 1
#endif
PRINT "defined count: ", count

' #ifndef must agree with #ifdef for the same macro on every platform.
DIM notWin32 AS BOOLEAN
notWin32 = FALSE
#ifndef __FB_WIN32__
notWin32 = TRUE
#endif
DIM isWin32 AS BOOLEAN
isWin32 = FALSE
#ifdef __FB_WIN32__
isWin32 = TRUE
#endif
PRINT "ifndef/ifdef agree: ", (notWin32 <> isWin32)
