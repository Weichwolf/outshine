Type: bug
Area: render
Tags: hygiene

# Pi in a shader file derives from the injected origin

1630 closed on the ruling that the MSL raw strings keep their pi literals — an excuse scoped
to raw strings inside .h/.cpp. 1647/1580 moved the MSL into files and the literals moved with
it, out of every detector's sight:

- src/render/shaders/medium.msl:47,74,89,163 spell `3.14159265358979` — in the SAME assembled
  translation unit whose prelude defines `MEDIUM_PI` at 17 digits
  (ParticipatingMedium.cpp:20). The shared core (MediumCore.h) uses MEDIUM_PI; the sibling
  file one concatenation later re-spells the digits. Two origins for pi in one kernel, and
  the 14-digit spelling is exactly the drift the pi sweep (board:1630) hunted down in C++.
- src/render/shaders/sky.msl:53 spells the same digits with no injected origin in its
  assembly at all (SkyStage.cpp interpolates VELOCITY_STATIC but not pi).
- test/harness/claims/PiStandsOnceAndItIsStdNumbers.cpp walks only .h/.cpp and excuses raw
  strings — .msl files are invisible to it, so the guard that closed 1630 cannot see the
  regression class it now hosts.

Today the two spellings round to the same float32, so no pixel moves — this is the drift
CLASS, not a wrong picture. Demanded: every pi in src/render/shaders/*.msl derives from the
one injected define (MEDIUM_PI or a shared constants prelude the assemblers prepend, the
VELOCITY_STATIC form), and the pi claims test extends its walk to .msl files so a digit
spelling there refuses in the gate.

---

Closed: the prelude is now a function -- MslPrelude() = kMslPrelude + "#define OUTSHINE_PI
%.17g" from std::numbers -- and every stage assembly consumes it, so every shader file sees
the ONE injected origin. medium.msl's four digit sites and sky.msl's one derive from
OUTSHINE_PI; MediumCore.h's dialect macro is the same name (the C++ wrapper defines it from
std::numbers, MEDIUM_PI is gone -- one name, one origin, two languages). The guard sees the
class again: PiStandsOnceAndItIsStdNumbers walks .msl files too, and a digit spelling there
refuses in the gate. Parity 42/42, gate 129/129 warm.
