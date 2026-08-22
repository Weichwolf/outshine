Type: task
Parent: 1580
Area: render
Tags: instrument

**The MSL a stage ships lives in a file the tree can read**

First slice of 1580's decided form. The seven gated stages' MSL texts move BYTE-IDENTICAL
from raw strings in .cpp to files under src/render/shaders/, loaded once through one loader
(ShaderFile) that refuses loudly with the path it tried and keeps the text cached. The
assembly seam stays the public statics 1634 built, so the compile gate keeps proving the
exact runtime source; byte-identity keeps the pixel oracles untouched. The shared physics
core (the dialect file both languages include) is the NEXT slice, per family, medium first --
the double-accumulating C++ reference stays an explicit twin where the divergence is genuine
(precision, template callbacks). Content-store hashing of the shader files follows when the
asset pipeline wants it; the loader is the one place that will learn it.

---

Closed: the seven texts stand as files under src/render/shaders/ (sky, present, overlay,
compositeTransmission, medium core, three medium kernels), extracted byte-identical from the
raw strings; one loader (ShaderFile::LoadShaderText) reads them, caches nothing it should
not, and refuses with the path it tried; every stage's Source static gained an
error-carrying overload and Configure refuses on an empty source. Proving test:
test/unit/render/EveryAssembledKernelCompilesOnTheDevice.cpp -- compiles every assembled
source on the headless device AND probes the refusal from a foreign cwd, which must name the
file. 127/127 warm, byte-identity keeps the pixel oracles untouched.

Strengthened (round 3's note): the loader's EMPTY refusal has its probe -- the gate test
plants a zero-byte file in the nest and the refusal must say "empty" (a zero-byte kernel is
a picture refusal, never a silent nothing).
