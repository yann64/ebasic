' eBasic's standard file library (see docs/reference/file-library.md) -
' every function's normal case, plus its documented failure cases
' (a missing file/directory, a non-empty directory, a plain file passed
' to RmDir). Uses plain relative paths (resolved against the running
' program's own current working directory, like any other language) so
' this test has no platform-specific temp-directory dependency - and
' cleans up everything it creates using the very functions under test.

DIM baseName AS STRING
baseName = "eb_test_file_library"

DIM f1 AS STRING
f1 = baseName & "_1.txt"
DIM f2 AS STRING
f2 = baseName & "_2.txt"
DIM missing AS STRING
missing = baseName & "_missing.txt"
DIM dir AS STRING
dir = baseName & "_dir"

' --- normal cases ---------------------------------------------------

PRINT FileExists(missing)

DIM writeOk AS INTEGER
writeOk = WriteFile(f1, "hello")
PRINT writeOk
PRINT FileExists(f1)
PRINT FileLen(f1)

DIM appendOk AS INTEGER
appendOk = WriteFile(f1, " world", 1)
PRINT appendOk
PRINT FileLen(f1)

DIM readOk AS INTEGER
DIM contents AS STRING
contents = ReadFile(f1, readOk)
PRINT readOk
PRINT contents

PRINT FileCopy(f1, f2)
PRINT FileExists(f2)

DIM renamed AS STRING
renamed = baseName & "_renamed.txt"
PRINT Rename(f2, renamed)
PRINT FileExists(f2)
PRINT FileExists(renamed)

PRINT MkDir(dir)
PRINT MkDir(dir)                          ' already exists - fails

DIM inner AS STRING
inner = dir & "/inner.txt"
CALL WriteFile(inner, "x")
PRINT RmDir(dir)                          ' not empty - fails
PRINT Kill(inner)
PRINT RmDir(dir)                          ' now empty - succeeds

' --- failure cases on things that don't exist -----------------------

PRINT FileLen(missing)
PRINT Kill(missing)
PRINT RmDir(baseName & "_missing_dir")
PRINT Rename(missing, baseName & "_wontexist.txt")
PRINT ReadFile(missing, readOk)
PRINT readOk

' --- RmDir on a plain file (not a directory) ------------------------

PRINT RmDir(f1)

' --- cleanup (also exercises Kill's normal success path once more) --

PRINT Kill(f1)
PRINT Kill(renamed)

PRINT "file library ok"
