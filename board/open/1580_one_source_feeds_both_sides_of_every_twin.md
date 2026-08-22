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
