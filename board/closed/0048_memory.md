Type: feature
Area: core
Tags: scope

**I.2 Memory**

- [x] Fixed heap, forced by the graphics API refusing a resizable buffer as an argument source
- [x] Heap probe reporting bytes (`core/io/HeapProbe`)
- [x] Stack probe per thread (`core/io/StackProbe`)
- [x] Device-resident picture data with a handle and a time-to-live on the processor, never a second copy
- [ ] Per-thread stack sizes set per purpose rather than one default for a network thread and a mesher


---

**Closed as stale (2026-08-22).** Fixed-heap/linear-memory framing is wasm; the platform is SDL3 on device memory.
