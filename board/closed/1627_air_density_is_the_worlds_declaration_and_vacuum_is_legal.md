Type: task
Parent: 1622
Area: scenario

# Air density is the world's declaration and vacuum is legal

rho moves from <vehicle/body airDensity> to <world airDensityKgM3>; the Envelope carries it
from the world exactly as GravityMs2; SpeedProfile accepts rho = 0 (drag term vanishes, TopMs
unbounded by drag -- bounded by the remaining terms), refusing only rho < 0. The moon plans a
drive in vacuum; the proof extends ASpeedPlanScalesWithTheDeclaredGravity or stands beside it.


---

Closed: <world airDensityKgM3> is in the grammar (default 1.2250, ISA sea level, the earth
seed); the vehicle no longer declares air; Stand takes (vehicle, gravity, air) and refuses only
air below nothing; SpeedProfile's gate accepts a vacuum, TopMs is unbounded there (infinity,
guarded at CorridorLay's drag-at-top), and DriveTick's drag reads the envelope. Proving tests:
ADriveTickHoldsTheCarToTheDeclaredWorld drives the moon leg in a declared VACUUM to arrival;
TheDriversScenarioLoadsAndItsNumbersArePhysical derives the F31's 233 km/h from the WORLD's
air. Fast gate 122/122.