Type: task
Parent: 1630
Area: core

# The last pi spellings die

M_PI leaves Camera.h and Ephemeris.h for the one kPi; TileMath.h stops re-aliasing kPi beside
Units.h. grep -rn "M_PI\|kPi =" src/ finds one definition.
