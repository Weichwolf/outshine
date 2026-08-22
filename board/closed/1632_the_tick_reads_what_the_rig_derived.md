Type: task
Parent: 1629
Area: sim

# The drive tick reads the drag area the rig derived

DriveTick stops deriving dragArea from the car per tick; Resist takes Envelope.DragArea and
Envelope.AirDensity -- one value, one origin, derived once at Stand. The synthetic twin pins
the behaviour unchanged.


---

Closed: DriveTick derives nothing from the car -- Resist takes Envelope.DragArea and
Envelope.AirDensity, both born at Stand. ADriveTickHoldsTheCarToTheDeclaredWorld pins the
behaviour (earth and vacuum-moon arrivals unchanged). Fast gate 122/122.