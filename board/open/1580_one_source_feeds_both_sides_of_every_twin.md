Type: issue
Area: render
Tags: instrument

**One source feeds both sides of every twin**

The C++/MSL twin idiom carries 300 lines of atmosphere physics twice
(`ParticipatingMedium.h`: C++ at 57-371, MSL at 373-643) with nothing but review keeping them
synchronized -- and this session measured the cost: a bounce term landed in the MSL and missed
the C++ once, caught only by the device-vs-twin probe. The reviewer's verdict: the C++ side
accumulates in double, so it is a higher-precision REFERENCE rather than a twin, and the
discipline is right but the duplication is not. Unreal's answer is one source (.ush) included
from both sides; here a shared core (C-subset with thin per-language wrappers, or generation)
would delete the drift class.

**The decision** (owner's, because it trades the twin's readability against single-source):
keep the explicit two-language twin and its test discipline, or move to one shared source per
shader family. Recommendation: shared source for the physics kernels (medium, BRDF), explicit
twins only where the languages genuinely diverge (texture sampling, storage layout).

---

Sharpened (review 2026-08-22 late): two developments feed the pending decision. 1634 gave
every runtime-assembled MSL blob a public static (SkyStage::ShaderSource et al.) — the blobs
now have an API surface, which hardens the embedded-string form just as the argument against
it grows: 1636's second executor table (softgl) consumes NO MSL, so the physics that must run
on both backends (medium LUTs from the C++ twins) already proves the shared-core direction.
Recommendation stands and sharpens: shader source as FILES in the tree (loaded once at
Configure, hashed into the content store like any asset), shared physics core included from
both sides; string literals inside .cpp remain the drift-and-blind-edit class the pi sweep
demonstrated even with the compile gate standing.

---

DECIDED (2026-08-22, owner delegated the call for this session, on record): the
recommendation stands as the binding form. Shader source becomes FILES in the tree, loaded
once at Configure and hashed like any asset; the physics kernels (medium, BRDF) move to ONE
shared core in the common C-subset, included by the C++ reference and the MSL alike; explicit
twins remain only where the languages genuinely diverge (texture sampling, storage layout).
Grounds: the compile gate catches syntax, not twin drift -- the bounce-term incident measured
the class; and 1636's second backend consumes the same physics with no MSL, so the shared
core has two consumers already. Implementation is sliced: medium family first (1647), the
gate's public-static door stays the seam.

---

Slice 2 landed (the shared medium core): src/render/stages/MediumCore.h is ONE dialect file
holding the nine scalar physics functions -- topReach, groundReach, heightAlong,
transmittanceParams, rayleighPhase, miePhase, subUvsToUnit, unitToSubUvs, skyViewParams --
compiled as C++ by ParticipatingMedium.h (medium_core wrapper: max/clamp shims, std usings,
MEDIUM_CONST/THREAD/PI defines) and appended as MSL text by ParticipatingMediumMsl (defines +
mediumLayout.msl + core + medium.msl). The nine C++ twins are DELETED, their call sites moved
to the shared names and reference-out signatures; the MSL Medium fields rose to the C++
spelling (PascalCase). The vector functions (float3 extinction/scatter, the sampling loops,
the double-accumulating references) stay explicit twins -- genuine language divergence.
Proof: render/outshine/shader 42/42 (device vs C++ agreement over the LUT chain), fast gate
128/128 warm at 68.9 s. Remaining slices: the BRDF family twins (MetalRoughBrdf, SheenLobe,
IridescenceLobe, MicrofacetEnergy generate their MSL from C++ constants -- one source
already, audit their form), then the content-store hashing when the asset pipeline wants it.
