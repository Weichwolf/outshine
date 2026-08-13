Type: feature
Area: core
Tags: perf, instrument

**What the scenario suite must measure, and it is the CPU half**

- [ ] **The compositor is PARTLY on the frame path even though the generator is not, and a GPU-only distribution would report a frame floor the CPU misses.** Culling, LOD selection and dynamic placement run inside 16.67 ms. That blindness is the same class as `worst_disagreement_px` measuring boundaries and being quoted about the picture (§ I.26.15)
- [ ] **The scenario verdict is the WALL-CLOCK frame distribution, p50/p95/p99, with the compositor's own CPU span published beside the per-pass GPU spans** — the `GpuTimer` shape on the CPU side, one scoped span per compositor call and one telemetry column each. **TOOL**, and it is *not yet measured* rather than not measurable: the effort is a scoped timer and a column
- [ ] **`frameMs − Σ(GPU spans) − Σ(compositor spans)` is published as its own column**, so *unattributed* stays measured rather than subtracted by hand — § I.11 already requires the two-term version of this and the compositor is the third term
- [ ] **The first scenario case is a camera path over composed content**, which is what finally populates the suite § I.11 records as having a definition and zero members — and it is the fourth constraint's first instrument
- [ ] **Popping at an LOD change, a hitch on stream-in and a scatter with a radius are decidable only in motion** and belong here rather than in the render suite, by § I.26.9's placement rule. *A still frame does not prove any of the three*
