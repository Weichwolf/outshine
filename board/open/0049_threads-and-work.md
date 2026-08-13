Type: feature
Area: data
Tags: scope

**I.3 Threads and work**

- [x] Worker pool where a pthread is a Web Worker, so a synchronous fetch blocks nothing (`world/TilePool`)
- [x] Fetch, decode, mesh and cluster-DAG build off the frame thread
- [ ] Every long-lived thread created at bring-up before the frame loop, with runtime creation a hard failure
- [ ] Thread count taken from what the runtime reports, never from the developer machine
- [ ] Dedicated non-computing threads sized by the protocol's connection limit per origin
- [x] Request-level timeout; no timeout on the load as a whole
- [ ] Audio worklet thread that neither blocks nor allocates in its callback
