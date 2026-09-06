' M12: coroutines - Async SUB/FUNCTION, Task(OF T)/Generator(OF T), YIELD,
' and AWAIT, all compiling to real C++20 co_return/co_yield/co_await.

FUNCTION CountUp(n AS INTEGER) Async AS Generator(OF INTEGER)
    DIM i AS INTEGER
    FOR i = 1 TO n
        YIELD i
    NEXT
END FUNCTION

FUNCTION DoubleIt(x AS INTEGER) Async AS Task(OF INTEGER)
    RETURN x * 2
END FUNCTION

' AWAIT chains across two nested Tasks - only legal from inside another
' Async FUNCTION (C++ forbids co_await in main()).
FUNCTION AddThemUp() Async AS Task(OF INTEGER)
    DIM a AS INTEGER
    DIM b AS INTEGER
    a = AWAIT DoubleIt(5)
    b = AWAIT DoubleIt(10)
    RETURN a + b
END FUNCTION

SUB PrintHello() Async
    PRINT "hello from an async sub"
END SUB

DIM gen AS Generator(OF INTEGER)
gen = CountUp(5)
DIM total AS INTEGER
total = 0
DO WHILE gen.MoveNext()
    total = total + gen.Current()
LOOP
PRINT total

' Top-level code can't AWAIT (it compiles into main()) - Result() reads an
' already-completed Task's value directly instead (this runtime always
' runs a Task synchronously to completion by the time the call returns).
DIM t AS Task(OF INTEGER)
t = AddThemUp()
PRINT t.Result()

DIM d AS Task(OF INTEGER)
d = DoubleIt(21)
PRINT d.Result()

CALL PrintHello()
