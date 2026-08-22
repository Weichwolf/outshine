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
