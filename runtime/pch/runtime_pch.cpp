// M6/MSVC PCH source file - cl.exe can't precompile a bare header file the
// way `g++ -x c++-header` can; it needs a real .cpp whose first
// non-comment content is the #include being precompiled (see
// runtime/CMakeLists.txt's MSVC PCH block, which compiles this file with
// /Yc"ebasic/runtime/runtime.hpp"). The precompiled state is everything
// reachable from this single #include - keep it in sync with the GCC .gch
// block's own DEPENDS list right above it.
#include "ebasic/runtime/runtime.hpp"
