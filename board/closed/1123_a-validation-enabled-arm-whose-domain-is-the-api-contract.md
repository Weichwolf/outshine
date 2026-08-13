Type: feature
Area: harness
Tags: instrument, bug

**A validation-enabled arm, whose domain is the API contract rather than the picture**

`SDL_CreateGPUDevice(..., debug_mode = true, ...)` has **never** been enabled in this harness. One flag
caught a defect that **118 tests, a full render suite and a picture bound over 34 cases all passed**:
`board:1121`'s prune left `SubjectDraw` writing two colour outputs into a pass with one attachment, and
Metal aborts on it — *"for color attachment 1, the renderPipelineState pixelFormat must be
MTLPixelFormatInvalid, as no texture is set."*

**Every pixel was correct. The API contract was not.** No instrument in this tree has that as its domain:
`unit` decides whether the computation is right, `render` whether the pixels match Cycles, `shader`
whether a shader agrees with its C++ twin, `frame` whether the cost moved. **A pipeline whose output set
disagrees with its pass is invisible to all four**, and it is undefined behaviour that happens to render.

This is the same class as `~sanitised`: a second arm over the same cases, differing only in what it is
allowed to notice. It is cheap for the same reason — no new case, no new asset, no new oracle.

**Done when** a validation-enabled arm runs the render cases, its failures are attributed like any other,
and its cost is published so nobody quotes a timing from it. The sanitised arm's precedent applies:
**no timing number comes from an instrumented run.**
