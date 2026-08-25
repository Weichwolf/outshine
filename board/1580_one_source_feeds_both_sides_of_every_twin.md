Type: issue
State: open
Area: render
Tags: instrument, decision

# One source feeds both sides of every twin

Half of this is repaid: shader source lives as files in the tree (src/render/shaders/, 25 of
them) and no source string is embedded. What stands is the DUPLICATION of physics: the
atmosphere is written twice — the C++ reference in src/render/stages/ParticipatingMedium.h
(329 lines, accumulating in double) and the MSL in src/render/shaders/medium.msl — with nothing
but review keeping them in step. The cost was measured once: a bounce term landed in the MSL,
missed the C++, and only the device-vs-twin probe caught it.

**The decision is the owner's**, because it trades the twin's readability against one source:
keep the explicit two-language twin with its test discipline, or share one source per shader
family. Recommendation: a shared core for the physics kernels (medium, BRDF), explicit twins
only where the languages genuinely diverge (sampling, storage layout). board:1636's second
executor table consumes no MSL at all and needs the medium LUTs from the C++ side, which is the
same argument arriving from the backend.

## What will be true

- [ ] The decision is recorded here with its reason, and the tree carries one shape or the other.
- [ ] A term that lands on one side and not the other is caught by a case, not by a reader.
