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

## Measured at b0b59b3a, and the technique is already in the tree

`MediumCore.h` (81 lines) is compiled TWICE from one source: `ParticipatingMedium.h:51` includes
it under `#define MEDIUM_CONST const`, and `ParticipatingMediumMsl` concatenates the same file
under `#define MEDIUM_CONST constant` before the MSL. Three functions and two phase kernels
already have exactly one spelling that way.

What is still twinned, function for function:

| MSL, src/render/shaders/medium.msl | C++, src/render/stages/ParticipatingMedium.h |
|---|---|
| `mediumExtinctionPerKm` | `MediumExtinctionPerKm` |
| `mediumScatterExtinctPerKm` | `MediumScatterExtinctPerKm` |
| `mediumTransmittanceUv` | `MediumTransmittanceUv` |
| `mediumTransmittance` | `MediumTransmittance` |
| `mediumMultiScatterTexel` | `MediumMultiScatterTexel` |
| `mediumSkyRay` | `MediumSkyRay` |

**Six functions, 182 MSL lines against roughly 248 C++ lines outside the shared core.** They
differ in two things and nothing else: `float3` against `float[3]`, and the address-space
keyword the macros already handle. The three that ARE shared are the three that use scalars only,
which is why they shared without a fight.

**The decision, recorded: one source.** The twin's readability is not worth a second spelling of
a physical model -- and the cost of the split has already been paid once, when a bounce term
landed on one side. A `float3` shim on the C++ side is what the sharing needs, and it is the
shape Unreal uses for its HLSL/C++ math layer.

## What will be true

- [ ] The decision is recorded here with its reason, and the tree carries one shape or the other.
- [ ] A term that lands on one side and not the other is caught by a case, not by a reader.
