#include "shapelib.iface.bas"

DIM c AS Circle
c = MakeCircle(7, 3)
PRINT CircleId(c)
PRINT CircleColor(c)
