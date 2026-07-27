TYPE Thermometer
    cTemp AS SINGLE

    Declare Constructor()
    Declare Property Celsius() AS SINGLE
    Declare Property Celsius(BYVAL value AS SINGLE)
    Declare Property Fahrenheit() AS SINGLE
    Declare Property Fahrenheit(BYVAL value AS SINGLE)
END TYPE

Constructor Thermometer()
    cTemp = 0
End Constructor

Property Thermometer.Celsius() AS SINGLE
    Celsius = cTemp
End Property

Property Thermometer.Celsius(BYVAL value AS SINGLE)
    ' A validated setter: clamp to a physically valid range.
    IF value < -273.15 THEN
        cTemp = -273.15
    ELSE
        cTemp = value
    END IF
End Property

Property Thermometer.Fahrenheit() AS SINGLE
    Fahrenheit = cTemp * 9 / 5 + 32
End Property

Property Thermometer.Fahrenheit(BYVAL value AS SINGLE)
    ' A setter built on another property (through This.).
    This.Celsius = (value - 32) * 5 / 9
End Property

DIM t AS Thermometer

t.Celsius = 100
PRINT t.Celsius
PRINT t.Fahrenheit

t.Fahrenheit = 32
PRINT t.Celsius
PRINT t.Fahrenheit

' Setter validation: clamps an impossible value.
t.Celsius = -500
PRINT t.Celsius
