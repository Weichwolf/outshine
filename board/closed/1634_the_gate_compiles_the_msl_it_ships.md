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

---

Sharpened (review 2026-08-22, round 2): the shape statics are the right seam for Configure and
the gate — but the suite's claim "neither can drift unseen"
(EveryAssembledKernelCompilesOnTheDevice.cpp:84-85) is not yet earned. SDL's MSL path does not
validate the num_* fields against the compiled source (they are binding bookkeeping, and the
gate's device stands with debug_mode=false), so a shape that misdeclares its counts still
compiles green and misbinds at runtime: the shape is a SECOND declaration of the MSL's binding
interface with nothing proving the two agree. Before close, the gate derives the expected
counts from the stage's own public source and CHECKs the identity: max `[[sampler(N)]]`+1 ==
Samplers, max `[[buffer(N)]]`+1 == UniformBuffers, max `[[texture(N)]]`+1 == Samplers +
ReadOnlyTextures + ReadWriteTextures (SDL's MSL slot order). That proof survives 1647's move
of the source into files unchanged. The shape's other loose end — GroupY dead at two dispatch
sites, positional init — is board:1648.

---

Progress (round 2's sharpening repaid): the gate now PARSES the assembled source's own slot
annotations -- [[texture/buffer/sampler(N)]] maxima -- and refuses a shape that disagrees:
texture and buffer slots exact (SDL's pair model: sampled + read-only + read-write share the
texture space), sampler slots at most the declared pairs (two textures may share one
sampler). The shape is no longer an unproven second declaration. Remaining for close:
tonemap's optioned source and the subject unit's three parameterised shaders.

---

Closed: the named next slice is in the gate. Tonemap's optioned source compiles in all four
rows (both transfer curves x temporal on/off) through TonemapStage::ShaderSource(options) and
its two shapes; the subject unit's assembly is a public seam
(SubjectDraw::ShaderSource(SourceOptions) with WritesVelocity/NormalIndex/IdentityIndex,
VertexEntry/FragmentEntry, ShaderShape with storage buffers, DepthOnlySource/DepthOnlyShape)
and every vertex layout's and surface kind's entry point compiles from the exact assembled
text in both attachment rows, plus the shadow pass's depth-only pair; the subject's six MSL
blobs left SubjectShader.h for src/render/shaders/ (the header is deleted), so no inline MSL
remains outside the C++/MSL twin generators that 1580's shared-core slice owns. Every gated
shape is proven against the source's own slot annotations. Proving test:
test/unit/render/EveryAssembledKernelCompilesOnTheDevice.cpp. 127/127 warm at 56.6 s.
