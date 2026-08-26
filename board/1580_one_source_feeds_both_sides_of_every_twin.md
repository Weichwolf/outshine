Type: issue
State: open
Area: render
Tags: instrument, decision

# One source feeds both sides of every twin

Half of this is repaid: shader source lives as files in the tree (src/render/shaders/, 25 of
them) -- **with one exception the earlier wording of this item denied.** `src/render/stages/
Resolve.h:20-56` builds about forty lines of Metal out of C++ string literals: `filmic()`,
`covered()`, `rgbToYCoCg()`, `yCoCgToRgb()`, `clipTowards()` and `displayed()` itself, the
function `tonemap.msl:13` and `temporalResolve.msl:61` both call. So the tone curve and the
temporal clamp -- the two kernels that decide what a still LOOKS like -- are the only shading in
the tree a reader cannot open as a shader, and `MediumCore.h` already shows the technique that
fixes it: one file, included twice under a macro. What else stands is the DUPLICATION of physics: the
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

## What counts as an embedded shader, because a naive guard gets it wrong

Owner asked whether shaders are still strings in the cpp. The first answer given was NO, from a
grep for raw string literals -- and it was wrong, because `Resolve.h` uses ordinary ones. Walking
every C++ source for quoted Metal turns up exactly two files, and only one of them is a defect:

| file | what it emits | verdict |
|---|---|---|
| `src/render/stages/IridescenceLobe.h:127-` | a `constant` block: `kIriVal`, `kIriPos`, `kIriVar`, printed from the C++ constants with `%.17g` | **correct, and it stays.** The numbers have ONE spelling, in C++, and the shader is handed them. That is the cure for a twinned constant, not an instance of it |
| `src/render/stages/Resolve.h:17-56` | function BODIES: `filmic`, `covered`, `rgbToYCoCg`, `yCoCgToRgb`, `clipTowards`, `displayed` | **RED.** About forty lines of Metal a reader cannot open as a shader, and they are the tone curve and the temporal clamp -- the two kernels that decide what a still LOOKS like |

The distinction a guard has to make is CONSTANTS versus CODE. `IridescenceLobe.h` injects values
into a shader that lives in a file; `Resolve.h` is the shader.

## Measured, and the technique is already in the tree

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
- [ ] `Resolve.h`'s six functions live in a `.msl` file, included where they are needed the way
      `MediumCore.h` is included twice under a macro. The exposure and the curve choice stay
      INJECTED as constants, which is what `IridescenceLobe.h` already does correctly.
- [ ] A claim walks `src/` for quoted shader CODE -- a function body in a string -- and refuses,
      while letting a generated `constant` block stand. The first answer to the owner's question
      was wrong because a grep for `R"(` missed ordinary literals, and a guard that makes the
      same mistake is worth nothing.
- [ ] Proving case: the tonemap and temporal kernels compile from files and the picture is
      unchanged -- same mean max(RGB) on the door's sphere before and after. Negative control:
      the string-built source restored, and the two spellings can drift again.
