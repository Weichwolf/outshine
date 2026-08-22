Type: task
Parent: 1629
Area: sim

# The drive tick reads the drag area the rig derived

DriveTick stops deriving dragArea from the car per tick; Resist takes Envelope.DragArea and
Envelope.AirDensity -- one value, one origin, derived once at Stand. The synthetic twin pins
the behaviour unchanged.
