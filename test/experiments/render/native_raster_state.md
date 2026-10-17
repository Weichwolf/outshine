# Metal renderpass state reuse

Owner: [#328](https://github.com/Weichwolf/outshine/issues/328).

The CPU profile of the native Tokyo turn identifies opaque draw recording as
the main validation overhead. Previously every draw rebound its pipeline,
depth/culling/winding/clip state, viewport, scissor and vertex streams.

`RenderState` now remembers those bindings for its one encoder. A new renderpass
starts empty. Pipeline ownership prevents a destroyed C++ object/address from
becoming a false cache hit. Vertex identity and byte offset are both compared;
shader buffer slots cannot overlap the dedicated vertex stream slots.
All ownership, range, reflection, alias and hazard validation still runs before
applying state. Uniform contents and shader resources are still bound per draw.
There is no frame history, asset cache, GPU ABI change or new synchronization.

An opt-in external Metal probe counts calls on the same complete Tokyo scene:

| Calls, including preparation and 30 frames | Before | After | Reduction |
|---|---:|---:|---:|
| Pipeline bindings | 3,557 | 372 | 89.54% |
| Vertex buffer bindings | 17,180 | 6,163 | 64.13% |

A18 Pro, macOS, 1280×720/30, fresh offline processes, before/after/after/before:

| CPU p99, ms | Before | After |
|---|---:|---:|
| Metal validation disabled | 5.868 / 6.104 | 5.905 / 5.744 |
| `MTL_DEBUG_LAYER=1` | 13.090 / 13.445 | 9.570 / 9.837 |

The two-sample mean of p99 decreases 26.86% with validation. The normal-path
2.70% difference is too small to establish a speedup. GPU time varies; no GPU
speedup is claimed. All eight PNG SHA-256 hashes are identical. A validated
turn still takes 1002.066 ms in one run: the one-second gate remains open.

The shared native draw probe exercises pipeline/raster/stream changes and
repeated bindings before independently checking color and reverse-Z depth for
direct, 16/32-bit indexed and indirect draws, blending and write masks. Its final
1496 checks pass with Metal validation; the complete native Metal probe also
passes. Seventeen dependent clang-tidy units pass. These checks do not replace
all-Place performance, memory or temporal image acceptance.
