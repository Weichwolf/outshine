Type: task
Parent: 1630
Area: core

# The last pi spellings die

M_PI leaves Camera.h and Ephemeris.h for the one kPi; TileMath.h stops re-aliasing kPi beside
Units.h. grep -rn "M_PI\|kPi =" src/ finds one definition.


---

Closed: M_PI left Camera.h (kPi) and Ephemeris.h (kDeg2Rad); TileMath.h, Subject.cpp and
LeafAngleDistribution.cpp stopped re-aliasing -- `grep -rn "M_PI\|kPi = " src/` finds the one
Units.h definition (the MSL emitter's "constant float kPi" is generated text, not a C++
spelling). Fast gate 123/123.