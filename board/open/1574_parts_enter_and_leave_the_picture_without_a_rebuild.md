Type: feature
Area: render
Tags: perf, scope

**Parts enter and leave the picture without a rebuild, and glass is a partition, not a clone**

Technical review round 1 (RAGE/Unreal benchmark), the top structural gap beside board:1538: the
draw path has no incremental entry point. `SubjectDraw::SetMesh` re-uploads every vertex stream,
re-uploads indices and rebuilds the full CPU triangle BVH on ANY content change
(`SubjectDraw.cpp:1298-1400`), and `Live::Restand` does exactly that mid-drive at every 400 m
relay -- allocation, lock, disk and unbounded block all firing on the frame path at once. When
the plan holds glass, everything doubles: `Renderer.h:86-107` mirrors every Set* into `Glass_`,
a complete clone of the stage -- second geometry copy, second BVH, second pipeline set.

What ships: geometry is resident and shared; entities are lightweight instances added and
removed incrementally (RAGE drawable dictionaries + entity pools; Unreal FPrimitiveSceneInfo
against a persistent scene). The engine's own ladder diagram -- store, handles, completion queue
-- IS this design; the render path just cannot receive it yet.

- [ ] parts are added/removed against persistent residency; a relay uploads only what arrived
- [ ] the BVH refits or rebuilds only the region that changed, off the frame path
- [ ] `Glass_` dies: transmissive draws become a batch partition over ONE residency
- [ ] a relay's frame cost is measured before and after, over the windowed drive

Depends: 1538
