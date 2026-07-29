Extern "C" Lib "ebfixturec"
    Declare Sub eb_fixture_invoke_callback(ByVal cb AS ANY PTR, ByVal value AS INTEGER, ByVal user_data AS ANY PTR)
End Extern

SUB OnFixtureCallback(value AS INTEGER, user_data AS ANY PTR)
    PRINT "callback fired"
    PRINT value
END SUB

CALL eb_fixture_invoke_callback(@OnFixtureCallback, 42, 0)
