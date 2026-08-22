Type: bug
Area: render
Tags: tests

# The fast gate compiles the MSL it ships

The pi sweep (5b5cc642) rewrote literals inside the MSL raw strings to std::numbers -- a
spelling Metal does not know -- and the fast gate stayed green because kernel source only
compiles at SDL_CreateGPUComputePipeline time on the device: the breakage surfaced two suites
later as a stills refusal ("the medium's transmittance kernel was refused"). A unit-mirror
test per stage must assemble the kernel string and compile it (xcrun metal -c, or the device
pipeline in a headless GPU context) so an unbuildable kernel fails in the gate, not in a
five-minute driver run. Until then, every edit near a R"( block is a blind edit.


---

The gate compiles what the engine assembles (this commit): the three medium kernels, sky,
present, overlay and the transmission composite build their EXACT runtime source through
public statics and compile on a headless MSL device inside the fast gate (~0.4 s) --
EveryAssembledKernelCompilesOnTheDevice. The pi-sweep class refuses in seconds now. Remaining:
tonemap's optioned source and the subject unit's three shaders (parameterised), the next
slice.
---

Sharpened (review 2026-08-22 late): the seven statics are the right seam and the gate
compiles the exact runtime text. Two residues for the named next slice (tonemap's optioned
source, the subject unit's three): (a) the test re-spells each pipeline's binding counts
(EveryAssembledKernelCompilesOnTheDevice.cpp:28-33,50-51) instead of taking them from the
stage — count drift is silent because MSL compilation does not validate them; if a stage ever
grows a binding the gate compiles a shape the runtime never uses; (b) the suite sits at
test/unit/render/kernels/ with no src/render/kernels — fold it into the stages mirror or give
the shape a name the mirror rule recognises.

---

Residues (a) and (b) repaid (board queue): every gated stage now declares its pipeline SHAPE
as a public constexpr beside its source -- ComputeShape/DrawShape in
src/render/stages/KernelShape.h -- and the stage's own Configure builds its create-info from
those fields (the local kGroup*/kCompositeImages origins fold into or derive from the shape),
so the gate compiles the exact runtime source in the exact runtime shape and neither can
drift. The suite moved from the mirror-less unit/render/kernels to unit/render -- the true
mirror of src/render -- and src/render's excuse left the mirror claim. Proving test:
test/unit/render/EveryAssembledKernelCompilesOnTheDevice.cpp, 127/127. Remaining for close:
tonemap's optioned source and the subject unit's three parameterised shaders.
