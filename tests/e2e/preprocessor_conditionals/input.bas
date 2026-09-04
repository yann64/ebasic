' #elseif/#elseifdef/#elseifndef chains, #undef, and defined() - the
' Tier-1 conditional-compilation gap FreeBASIC has and eBasic's own
' preprocessor previously didn't (see docs/reference/preprocessor.md).

#define DEBUG_LEVEL 2

#if (DEBUG_LEVEL >= 3)
Print "level 3+"
#elseif (DEBUG_LEVEL = 2)
Print "level 2"
#elseif (DEBUG_LEVEL = 1)
Print "level 1"
#else
Print "level 0"
#endif

' Only one branch of a chain ever fires, even when a later condition would
' also be true.
#if (DEBUG_LEVEL >= 1)
Print "first true branch"
#elseif (DEBUG_LEVEL >= 2)
Print "should not print - first branch already matched"
#endif

#define FEATURE_A
#ifdef FEATURE_A
Print "A on"
#elseifdef FEATURE_B
Print "B on"
#else
Print "neither"
#endif

#ifndef FEATURE_B
Print "B off"
#elseifndef FEATURE_A
Print "should not print"
#endif

#undef FEATURE_A
#ifndef FEATURE_A
Print "A undefined after #undef"
#endif

#if defined(DEBUG_LEVEL) and DEBUG_LEVEL > 1
Print "defined() and arithmetic together"
#endif

#if defined(NOT_A_REAL_SYMBOL)
Print "should not print"
#else
Print "not defined"
#endif

' Arithmetic/comparison/logic in a single #if expression.
#if (1 + 2 * 3 = 7) and not (10 mod 3 = 0)
Print "expression math ok"
#endif
