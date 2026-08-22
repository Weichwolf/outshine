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
