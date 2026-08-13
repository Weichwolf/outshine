Type: feature
Area: harness
Tags: oracle, khronos

**The three layers map onto the three suites, and that is a claim with one exception**

- [ ] **A generated part is a RENDER case** — one part, one `.gltf`, one Cycles reference, the picture bound of § I.26.15. This is what the emit path buys and it is why the emit path is first
- [ ] **A composition is a SCENARIO case** — camera path, wall-clock distribution, residency, determinism, popping
- [ ] **The renderer keeps the Khronos criteria**, unchanged
- [ ] **The exception, stated so the mapping is not read as tidier than it is:** a compositor's *decisions* — culling yield, batch counts, LOD cut, sort order — are **unit** cases over a `DrawList`, with no device and no oracle, and they are the cheapest strong tests in the whole proposal. **A three-way mapping that hid them would push a deterministic, GPU-free check into a suite whose verdict is a frame-time percentile**, which is § I.26.9's own warning about borrowing another suite's verdict shape

---

## Band II — World
