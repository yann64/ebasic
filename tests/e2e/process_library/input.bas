' eBasic's standard process/environment library (see
' docs/reference/process-library.md) - Environ on an unset variable,
' Command() with no arguments (the standard e2e harness never passes any
' - real argv-forwarding is verified separately, by hand, since
' run_case.sh doesn't pass args to the compiled binary), Shell's exit-
' status passthrough (success, failure, and an arbitrary explicit code),
' and Sleep actually returning. ExitProcess has its own dedicated script
' (process_library_exitprocess.sh) since it deliberately exits nonzero,
' which this golden-diff harness can't express.

PRINT Environ("EB_TEST_DEFINITELY_UNSET_VAR_XYZ")
PRINT Command()
PRINT Shell("true")
PRINT Shell("false")
PRINT Shell("exit 42")
CALL Sleep(10)
PRINT "process library ok"
