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