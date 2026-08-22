Type: task
Parent: 1622
Area: scenario

# Air density is the world's declaration and vacuum is legal

rho moves from <vehicle/body airDensity> to <world airDensityKgM3>; the Envelope carries it
from the world exactly as GravityMs2; SpeedProfile accepts rho = 0 (drag term vanishes, TopMs
unbounded by drag -- bounded by the remaining terms), refusing only rho < 0. The moon plans a
drive in vacuum; the proof extends ASpeedPlanScalesWithTheDeclaredGravity or stands beside it.
