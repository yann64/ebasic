#include "common_lib.iface.bas"

' Expected output is deliberately platform-independent (which PlatformCode
' resolves to differs per platform; that it matches whichever platform
' macro actually fired does not) - so the same expected.stdout works
' unchanged on all four CI platforms.
DIM expectedCode AS INTEGER
expectedCode = 0

#ifdef __FB_WIN32__
    #include "windows_lib.iface.bas"
    expectedCode = 1
#endif
#ifdef __FB_LINUX__
    #include "linux_lib.iface.bas"
    expectedCode = 2
#endif
#ifdef __FB_DARWIN__
    #include "macos_lib.iface.bas"
    expectedCode = 3
#endif
#ifdef __FB_HAIKU__
    #include "haiku_lib.iface.bas"
    expectedCode = 4
#endif

PRINT "platform lib matches macro: ", (PlatformCode() = expectedCode)
PRINT Doubled(21)
