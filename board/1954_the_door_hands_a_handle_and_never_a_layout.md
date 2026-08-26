Type: bug
State: open
Parent: 1953
Area: door

# The door hands a handle and never a layout

`include/Geometry.h` publishes this:

```cpp
std::span<const float> PositionsM, Normals, Uv, Uv1, Tangents, Colours;
```

That is not an interface. **It is the renderer's input layout wearing an interface's clothes**, and
publishing it freezes the vertex arrangement into the ABI -- against TARGET's own sentence,
`contiguous, one-width, pointer-free layouts`. Every SIMD change we still intend costs a door break
once a client compiles against this.

**What the benchmarks publish.** Unreal's public mesh surface is `FMeshDescription`, a CLASS with
methods; `FStaticMeshVertexBuffers` -- the thing that is actually shaped like the above -- is never
public. RAGE keeps `rmcDrawable` inside entirely and builds it in the tool chain. Neither hands a
client float spans, and both for this reason.

**The universal exchange format already exists and CLAUDE.md already names it: glTF 2.0, the only
content surface.** Between processes that is the answer. INSIDE the process a generator hands its
result over without a serialisation round trip -- which is an argument for one shared internal
representation, not for publishing that representation's memory layout.

So the door gains a BUILDER and a HANDLE: a client's generator fills a form it cannot see the
inside of, gets back an opaque handle, and hands the engine that. The layout stays ours and stays
free to move.

- [ ] `include/Geometry.h` is gone or carries no span of float
- [ ] a client generator stands a part through the builder, proven by a case
- [ ] the internal layout changes in one commit with no client recompile, proven by the same case
