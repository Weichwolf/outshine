Type: feature
Area: render
Tags: perf

**I.10 Render frame**

- [x] Forward scene pass; no G-buffer
- [x] As few passes as possible: a pass must beat its own base price of 0.35–0.5 ms before it exists
- [x] A generator's material is a row of numbers with no field that can switch pipeline state
- [x] Core derives discard, two-sided lighting, transmission, blending and emission from what the generator declares
- [ ] Blended transparency ordered back to front inside the scene pass, with a declared budget of blended clusters
- [x] No pipeline creation while playing
- [ ] A title's own entity shader compiled during loading
- [x] Shadow pass, ambient-occlusion pass, exposure pass, temporal pass, present pass
- [x] The tone-mapping slot in the pass enumeration is empty since the fold — a dead slot is where a pass hides. **Closed by deleting the enumeration**: there is no fixed pass list to be empty in, only `kMaxPasses = kStageCount` as a bound (`render/GpuTimer.h:28`) over the passes the compiler derived (`render/plan/RenderPlan.cpp:206-232`, `test/unit/render/plan/APlanIsPulledFromWhatItRequests.cpp` — the coverage declaration compiles to two)
- [x] `GpuTimer::Pass::Cloud` is a dead slot — `GpuTimer::Pass` no longer exists (`render/GpuTimer.h`), so a dead slot has no spelling (`test/unit/render/plan/APlanIsPulledFromWhatItRequests.cpp`)
