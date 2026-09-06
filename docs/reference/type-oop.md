# TYPE and Object-Oriented Programming

eBasic's OOP lives entirely under the `TYPE` keyword - there is no separate
`CLASS` keyword (real FreeBASIC itself never finished implementing `CLASS`
either; eBasic follows the same path deliberately).

## `TYPE` - fields

```
TYPE Name
    field1 AS Type1
    field2 AS Type2
    ...
END TYPE
```

Declares a record type. Fields have no initializer syntax - give them
values via a [constructor](#constructor-destructor) or after declaring a
variable of the type.

```basic
TYPE Point
    x AS INTEGER
    y AS INTEGER
END TYPE

DIM p AS Point
p.x = 3
p.y = 4
PRINT p.x   ' 3
```

Assigning one variable of a `TYPE` to another does a **memberwise copy** (a
real, independent copy - not aliasing):

```basic
DIM q AS Point
q = p
q.x = 100
PRINT p.x   ' still 3 - q's mutation didn't affect p
```

A `TYPE` can nest another `TYPE` as a field, and arrays of a `TYPE` work like
arrays of anything else:

```basic
TYPE Line
    startPt AS Point
    endPt AS Point
END TYPE

DIM ln AS Line
ln.startPt.x = 1

DIM pts(2) AS Point
pts(0).x = 10
```

A `TYPE`-typed `SUB`/`FUNCTION` parameter defaults to `BYREF` (see
[Procedures and Arrays](procedures-and-arrays.md#parameters-byval-byref)).

## `Constructor` / `Destructor`

```
TYPE Name
    Declare Constructor()
    Declare Destructor()
END TYPE

Constructor Name()
    ...
End Constructor

Destructor Name()
    ...
End Destructor
```

Every method (constructor, destructor, or ordinary method below) is
**declared inside** the `TYPE` body with `Declare`, and **defined outside**
it, out-of-line - matching real FreeBASIC's own "declared within, defined
outside" convention. Only a no-argument constructor/destructor is supported
(no parameterized construction or overloading yet).

```basic
TYPE Counter
    value AS INTEGER
    Declare Constructor()
    Declare Destructor()
END TYPE

Constructor Counter()
    value = 0
    PRINT "created"
End Constructor

Destructor Counter()
    PRINT "destroyed"
End Destructor
```

## Methods

```
TYPE Name
    Declare Sub MethodName(params...)
    Declare Function MethodName(params...) AS ReturnType
END TYPE

Sub Name.MethodName(params...)
    ...
End Sub

Function Name.MethodName(params...) AS ReturnType
    ...
End Function
```

Inside a method body, `This` refers to the current instance - needed to
disambiguate when a parameter shadows a field of the same name, and to call
another method on the same instance:

```basic
TYPE Counter
    value AS INTEGER
    Declare Sub Increment()
    Declare Sub Bump()
    Declare Sub SetLabel(label AS STRING)
    label AS STRING
END TYPE

Sub Counter.Increment()
    value = value + 1
End Sub

Sub Counter.Bump()
    CALL This.Increment()   ' one method calling another on itself
End Sub

Sub Counter.SetLabel(label AS STRING)
    This.label = label      ' parameter 'label' shadows the field - This disambiguates
End Sub
```

A method is called with normal member-access syntax:

```basic
DIM c AS Counter
CALL c.Increment()
```

## `EXTENDS` - inheritance

```
TYPE Derived EXTENDS Base
    ...
END TYPE
```

At most one *ordinary* (fielded) base - see "Interfaces" below for
implementing more than one contract at once via `EXTENDS Base,
Interface1, Interface2`. A method declared `Virtual` in the base and
`Override` in a derived type participates in dynamic dispatch - called
through a `BYREF`/pointer reference to the base type, it still resolves
to the derived type's own override, not the base's:

```basic
TYPE Shape
    name AS STRING
    Declare Virtual Function Area() AS SINGLE
End TYPE

Virtual Function Shape.Area() AS SINGLE
    Area = 0
End Function

TYPE Circle EXTENDS Shape
    radius AS SINGLE
    Declare Virtual Function Area() AS SINGLE Override
End TYPE

Virtual Function Circle.Area() AS SINGLE
    Area = 3.14159 * radius * radius
End Function

Sub PrintArea(BYREF s AS Shape)
    PRINT s.Area()   ' calls the real (dynamic) type's own Area, not Shape's
End Sub

DIM c AS Circle
CALL PrintArea(c)    ' prints Circle's area, not 0
```

`Override` implies `Virtual` too (an override necessarily participates in
the vtable), whether or not `Virtual` is also written on the derived
declaration. `Base.Method()` calls the immediate base's own implementation
directly, bypassing any override - useful from inside an overriding method
that wants to extend rather than replace the base behavior:

```basic
Virtual Function Circle.Describe() AS STRING
    Describe = Base.Describe() & " (circle)"
End Function
```

### Interfaces

A TYPE with zero fields where every declared method is `Virtual` is a
*pure interface* - a contract with no data of its own. `EXTENDS` accepts
a comma-separated list naming at most one ordinary base plus any number
of interfaces:

```basic
TYPE IClickable
    Declare Virtual Sub OnClick()
END TYPE

TYPE IResizable
    Declare Virtual Sub OnResize(w AS INTEGER, h AS INTEGER)
END TYPE

TYPE BaseWidget
    label AS STRING
END TYPE

TYPE Widget EXTENDS BaseWidget, IClickable, IResizable
    clicks AS INTEGER
    Declare Virtual Sub OnClick()
    Declare Virtual Sub OnResize(w AS INTEGER, h AS INTEGER)
END TYPE

Virtual Sub Widget.OnClick()
    This.clicks = This.clicks + 1
End Sub

Virtual Sub Widget.OnResize(w AS INTEGER, h AS INTEGER)
    ' ...
End Sub
```

An interface's own declared methods have no out-of-line definition (real
C++ pure virtuals under the hood - a TYPE that implements an interface
without providing every one of its methods stays abstract, a real
backend compile error if anything ever tries to `DIM` it). To
*implement* an interface, a TYPE redeclares each of that interface's
methods in its own body (as `Widget` does with `OnClick`/`OnResize`
above) and provides real out-of-line definitions for them - exactly the
same "declared within, defined outside" shape ordinary `Virtual`/
`Override` methods already use.

A value of any TYPE that implements a given interface (directly, or
transitively through an ordinary base) is assignable to a `BYREF`
parameter (or variable) of that interface's own type - real dynamic
dispatch through the reference, the same polymorphism ordinary `EXTENDS`
already gives a base-typed `BYREF` parameter:

```basic
SUB PokeClickable(c AS IClickable)
    CALL c.OnClick()   ' dispatches to whichever concrete TYPE was passed
END SUB

DIM w AS Widget
CALL PokeClickable(w)
```

**Not yet supported** (deliberate scope cuts, not oversights): a TYPE
naming more than one ordinary (fielded) base; an interface itself using
`EXTENDS` (interface-of-interface inheritance); and upcasting through a
`PTR` (`DIM c AS IClickable PTR : c = @w` is rejected today - only the
`BYREF`-parameter/variable form above works, matching this language's
existing single-inheritance upcast machinery, which has the identical
PTR-vs-BYREF gap already).

## `PROPERTY`

```
TYPE Name
    Declare Property PropName() AS Type            ' getter
    Declare Property PropName(BYVAL value AS Type)  ' setter
END TYPE

Property Name.PropName() AS Type
    PropName = ...
End Property

Property Name.PropName(BYVAL value AS Type)
    ...
End Property
```

A property is accessed with plain field-like syntax (`t.Celsius = 100`,
`PRINT t.Celsius`) but runs the getter/setter method underneath. Both a
getter and a setter (of the same type) are required - there's no read-only
or write-only property yet.

```basic
TYPE Thermometer
    cTemp AS SINGLE
    Declare Property Celsius() AS SINGLE
    Declare Property Celsius(BYVAL value AS SINGLE)
End TYPE

Property Thermometer.Celsius() AS SINGLE
    Celsius = cTemp
End Property

Property Thermometer.Celsius(BYVAL value AS SINGLE)
    IF value < -273.15 THEN     ' a validating setter
        cTemp = -273.15
    ELSE
        cTemp = value
    END IF
End Property

DIM t AS Thermometer
t.Celsius = 100
PRINT t.Celsius   ' 100
t.Celsius = -500
PRINT t.Celsius   ' -273.15 (clamped by the setter)
```

## Operator overloading

```
Operator SYMBOL(ByRef lhs AS Type1, ByRef rhs AS Type2) AS ReturnType
    ...
End Operator
```

Free-standing (global) operator overloads only - there is no
member-declared operator overload form yet. At least one of the two operand
types must be a user-defined `TYPE`; overloadable symbols are the same
binary operators listed in [Operators](operators.md#precedence-table) (`+`
`-` `*` `/` `\` `MOD` `^` `SHL` `SHR` `&` `=` `<>` `<` `>` `<=` `>=` `AND`
`OR` `XOR`).

```basic
TYPE Vector2
    x AS SINGLE
    y AS SINGLE
END TYPE

Operator +(ByRef a AS Vector2, ByRef b AS Vector2) AS Vector2
    DIM r AS Vector2
    r.x = a.x + b.x
    r.y = a.y + b.y
    Return r
End Operator

Operator =(ByRef a AS Vector2, ByRef b AS Vector2) AS BOOLEAN
    Return (a.x = b.x) AND (a.y = b.y)
End Operator

DIM v1 AS Vector2
DIM v2 AS Vector2
v1.x = 1 : v1.y = 2
v2.x = 3 : v2.y = 4

DIM total AS Vector2
total = v1 + v2       ' calls the overloaded '+'
PRINT total.x         ' 4
PRINT (v1 = v1)       ' -1 (calls the overloaded '=')
```

## See also

- [Namespaces, Pointers, and Unions](namespaces-pointers-unions.md)
- [Procedures and Arrays](procedures-and-arrays.md)
- [`EXTERN` / C-C++ Interop](extern-interop.md) - opaque "handle" types (a `TYPE` with no fields/methods, used only behind a `PTR`)
