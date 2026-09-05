# Keyword Index

Every eBasic keyword and operator symbol, alphabetically, linking to where
it's actually documented.

## Topic pages

- [Types and Literals](types-and-literals.md)
- [Operators](operators.md)
- [Control Flow](control-flow.md)
- [Procedures and Arrays](procedures-and-arrays.md)
- [TYPE and Object-Oriented Programming](type-oop.md)
- [Namespaces, Pointers, and Unions](namespaces-pointers-unions.md)
- [`EXTERN` / C-C++ Interop](extern-interop.md)
- [Doc Comments (`'''`)](doc-comments.md)
- [Preprocessor](preprocessor.md)
- [String Library](string-library.md)
- [File Library](file-library.md)
- [Math Library](math-library.md)
- [Date/Time Library](date-time-library.md)
- [Process Library](process-library.md)

## Symbols

| Symbol | Meaning | Reference |
|---|---|---|
| `:` | statement separator | [Control Flow](control-flow.md#statement-separator) |
| `'''` | doc comment | [Doc Comments](doc-comments.md) |
| `+` | addition | [Operators](operators.md#addition---subtraction-multiplication) |
| `-` | subtraction, unary negate | [Operators](operators.md#addition---subtraction-multiplication) |
| `*` | multiplication (binary), dereference (unary) | [Operators](operators.md#addition---subtraction-multiplication), [Pointers](namespaces-pointers-unions.md#pointers-ptr--) |
| `/` | real division | [Operators](operators.md#real-division) |
| `\` | integer division | [Operators](operators.md#integer-division) |
| `^` | power (exponentiation) | [Operators](operators.md#power) |
| `&` | string concatenation | [Operators](operators.md#the-string-concatenation-operator) |
| `=` `<>` `<` `>` `<=` `>=` | comparison | [Operators](operators.md#comparison-operators) |
| `@` | address-of | [Pointers](namespaces-pointers-unions.md#pointers-ptr--) |
| `->` | member access through a pointer | [Pointers](namespaces-pointers-unions.md#pointers-ptr--) |

## Standard library functions

Not reserved keywords - ordinary, always pre-declared functions (see
[String Library](string-library.md) for why a program still can't
redeclare one of these names).

| Function | Reference |
|---|---|
| `Len` | [String Library](string-library.md#len) |
| `Left` / `Right` | [String Library](string-library.md#left--right) |
| `Mid` | [String Library](string-library.md#mid) |
| `InStr` / `InStrRev` | [String Library](string-library.md#instr--instrrev) |
| `UCase` / `LCase` | [String Library](string-library.md#ucase--lcase) |
| `LTrim` / `RTrim` / `Trim` | [String Library](string-library.md#ltrim--rtrim--trim) |
| `Str` | [String Library](string-library.md#str) |
| `Val` | [String Library](string-library.md#val) |
| `Chr` / `Asc` | [String Library](string-library.md#chr--asc) |
| `Space` | [String Library](string-library.md#space) |
| `Repeat` | [String Library](string-library.md#repeat) |
| `FileExists` | [File Library](file-library.md#fileexists) |
| `FileLen` | [File Library](file-library.md#filelen) |
| `Kill` | [File Library](file-library.md#kill) |
| `MkDir` | [File Library](file-library.md#mkdir) |
| `RmDir` | [File Library](file-library.md#rmdir) |
| `Rename` | [File Library](file-library.md#rename) |
| `FileCopy` | [File Library](file-library.md#filecopy) |
| `ReadFile` | [File Library](file-library.md#readfile) |
| `WriteFile` | [File Library](file-library.md#writefile) |
| `Abs` | [Math Library](math-library.md#trigonometric-and-exponential-functions) |
| `Sgn` | [Math Library](math-library.md#trigonometric-and-exponential-functions) |
| `Sqr` | [Math Library](math-library.md#trigonometric-and-exponential-functions) |
| `Sin` / `Cos` / `Tan` | [Math Library](math-library.md#trigonometric-and-exponential-functions) |
| `Asin` / `Acos` / `Atn` / `Atan2` | [Math Library](math-library.md#trigonometric-and-exponential-functions) |
| `Exp` / `Log` | [Math Library](math-library.md#trigonometric-and-exponential-functions) |
| `Int` / `Fix` | [Math Library](math-library.md#int--fix) |
| `Rnd` / `Randomize` | [Math Library](math-library.md#rnd--randomize) |
| `CByte` / `CUByte` / `CShort` / `CUShort` / `CInt` / `CUInt` / `CLngInt` / `CULngInt` / `CSng` / `CDbl` / `CBool` | [Math Library](math-library.md#numeric-conversions-cxxx) |
| `Hex` / `Oct` / `Bin` | [Math Library](math-library.md#base-conversions) |
| `Now` / `Timer` / `Date` / `Time` | [Date/Time Library](date-time-library.md#now--timer--date--time) |
| `DateSerial` / `TimeSerial` | [Date/Time Library](date-time-library.md#building-and-reading-a-serial) |
| `Year` / `Month` / `Day` / `Hour` / `Minute` / `Second` / `Weekday` | [Date/Time Library](date-time-library.md#building-and-reading-a-serial) |
| `DateAdd` / `DateDiff` | [Date/Time Library](date-time-library.md#dateadd--datediff) |
| `Environ` | [Process Library](process-library.md#environ) |
| `Command` | [Process Library](process-library.md#command) |
| `Shell` | [Process Library](process-library.md#shell) |
| `Sleep` | [Process Library](process-library.md#sleep) |
| `ExitProcess` | [Process Library](process-library.md#exitprocess) |
| `UBound` / `LBound` | [Procedures and Arrays](procedures-and-arrays.md#ubound--lbound) |

## Preprocessor directives

| Directive | Reference |
|---|---|
| `#define` | [Preprocessor](preprocessor.md#define) |
| `#ifdef` / `#ifndef` / `#else` / `#endif` | [Preprocessor](preprocessor.md#ifdef-ifndef-else-endif) |
| `#include` / `#include once` | [Preprocessor](preprocessor.md#include-include-once) |
| `__FB_WIN32__` / `__FB_LINUX__` / `__FB_DARWIN__` / `__FB_HAIKU__` | [Preprocessor: Platform macros](preprocessor.md#platform-macros) |

## Keywords

| Keyword | Reference |
|---|---|
| `ALIAS` | [`EXTERN` interop: Alias](extern-interop.md#alias) |
| `AND` | [Operators](operators.md#and-or-xor-not) |
| `ANY` | [Pointers: `ANY PTR`](namespaces-pointers-unions.md#pointers-ptr--) |
| `AS` | [`DIM`](types-and-literals.md#dim) |
| `BASE` | [`EXTENDS`](type-oop.md#extends---inheritance) |
| `BOOLEAN` | [Primitive types](types-and-literals.md#primitive-types) |
| `BYREF` | [Parameters](procedures-and-arrays.md#parameters-byval-byref) |
| `BYTE` | [Primitive types](types-and-literals.md#primitive-types) |
| `BYVAL` | [Parameters](procedures-and-arrays.md#parameters-byval-byref) |
| `CALL` | [`CALL`](procedures-and-arrays.md#call) |
| `CASE` | [`SELECT CASE`](control-flow.md#select-case) |
| `CDECL` | [Standalone `Declare`](extern-interop.md#standalone-declare-no-block) |
| `CONST` | [`CONST`](types-and-literals.md#const) |
| `CONSTRUCTOR` | [`Constructor` / `Destructor`](type-oop.md#constructor-destructor) |
| `DECLARE` | [`Extern` blocks](extern-interop.md#extern-c-end-extern), [Standalone `Declare`](extern-interop.md#standalone-declare-no-block) |
| `DESTRUCTOR` | [`Constructor` / `Destructor`](type-oop.md#constructor-destructor) |
| `DIM` | [`DIM`](types-and-literals.md#dim) |
| `DO` | [`DO` / `LOOP`](control-flow.md#do-loop-while-wend) |
| `DOUBLE` | [Primitive types](types-and-literals.md#primitive-types) |
| `ELSE` | [`IF`](control-flow.md#if-then-elseif-else-end-if) |
| `ELSEIF` | [`IF`](control-flow.md#if-then-elseif-else-end-if) |
| `END` | (closes `IF`/`SUB`/`FUNCTION`/`TYPE`/`SELECT`/... - see each construct's own entry) |
| `ENUM` | [`ENUM`](types-and-literals.md#enum) |
| `EXIT` | [`EXIT`](control-flow.md#exit), [`EXIT SUB` / `EXIT FUNCTION`](procedures-and-arrays.md#exit-sub-exit-function) |
| `EXTENDS` | [`EXTENDS`](type-oop.md#extends---inheritance) |
| `EXTERN` | [`Extern "C"`](extern-interop.md#extern-c-end-extern), [`Extern "C++"`](extern-interop.md#extern-c-and-namespace-nested) |
| `FALSE` | [Literals](types-and-literals.md#literals) |
| `FOR` | [`FOR`](control-flow.md#for-to-step-next) |
| `FUNCTION` | [`SUB` / `FUNCTION`](procedures-and-arrays.md#sub-function) |
| `GOSUB` | [`GOSUB` / `RETURN`](control-flow.md#gosub-return) |
| `GOTO` | [`GOTO` and labels](control-flow.md#goto-and-labels) |
| `IF` | [`IF`](control-flow.md#if-then-elseif-else-end-if) |
| `INTEGER` | [Primitive types](types-and-literals.md#primitive-types) |
| `LIB` | [`Extern` blocks](extern-interop.md#extern-c-end-extern) |
| `LONG` | [Primitive types](types-and-literals.md#primitive-types) |
| `LONGINT` | [Primitive types](types-and-literals.md#primitive-types) |
| `LOOP` | [`DO` / `LOOP`](control-flow.md#do-loop-while-wend) |
| `MOD` | [`MOD`](operators.md#mod) |
| `NAMESPACE` | [`NAMESPACE`](namespaces-pointers-unions.md#namespace) |
| `NEXT` | [`FOR`](control-flow.md#for-to-step-next) |
| `NOT` | [Operators](operators.md#and-or-xor-not) |
| `OPERATOR` | [Operator overloading](type-oop.md#operator-overloading) |
| `OR` | [Operators](operators.md#and-or-xor-not) |
| `OVERRIDE` | [`EXTENDS`](type-oop.md#extends---inheritance) |
| `PRESERVE` | [`REDIM`](procedures-and-arrays.md#redim-redim-preserve) |
| `PRINT` | [`PRINT`](control-flow.md#print) |
| `PROPERTY` | [`PROPERTY`](type-oop.md#property) |
| `PTR` | [Pointers](namespaces-pointers-unions.md#pointers-ptr--) |
| `REDIM` | [`REDIM`](procedures-and-arrays.md#redim-redim-preserve) |
| `RETURN` | [`GOSUB` / `RETURN`](control-flow.md#gosub-return), [`SUB` / `FUNCTION`](procedures-and-arrays.md#sub-function) |
| `SELECT` | [`SELECT CASE`](control-flow.md#select-case) |
| `SHL` | [Operators](operators.md#shl-shr) |
| `SHORT` | [Primitive types](types-and-literals.md#primitive-types) |
| `SHR` | [Operators](operators.md#shl-shr) |
| `SINGLE` | [Primitive types](types-and-literals.md#primitive-types) |
| `STDCALL` | [Standalone `Declare`](extern-interop.md#standalone-declare-no-block) |
| `STEP` | [`FOR`](control-flow.md#for-to-step-next) |
| `STRING` | [Primitive types](types-and-literals.md#primitive-types) |
| `SUB` | [`SUB` / `FUNCTION`](procedures-and-arrays.md#sub-function) |
| `THEN` | [`IF`](control-flow.md#if-then-elseif-else-end-if) |
| `THIS` | [Methods](type-oop.md#methods) |
| `TO` | [`FOR`](control-flow.md#for-to-step-next) |
| `TRUE` | [Literals](types-and-literals.md#literals) |
| `TYPE` | [`TYPE` - fields](type-oop.md#type---fields) |
| `UBYTE` | [Primitive types](types-and-literals.md#primitive-types) |
| `UINTEGER` | [Primitive types](types-and-literals.md#primitive-types) |
| `ULONGINT` | [Primitive types](types-and-literals.md#primitive-types) |
| `UNION` | [`UNION`](namespaces-pointers-unions.md#union) |
| `UNTIL` | [`DO` / `LOOP`](control-flow.md#do-loop-while-wend) |
| `USHORT` | [Primitive types](types-and-literals.md#primitive-types) |
| `VIRTUAL` | [`EXTENDS`](type-oop.md#extends---inheritance) |
| `WEND` | [`DO` / `LOOP`](control-flow.md#do-loop-while-wend) |
| `WHILE` | [`DO` / `LOOP`](control-flow.md#do-loop-while-wend) |
| `XOR` | [Operators](operators.md#and-or-xor-not) |
| `ZSTRING` | [Primitive types](types-and-literals.md#primitive-types), [`EXTERN` interop](extern-interop.md#zstring-at-the-interop-boundary) |
