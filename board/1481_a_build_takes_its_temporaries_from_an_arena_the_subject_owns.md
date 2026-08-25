Type: feature
State: active
Area: gltf
Tags: perf

**A build takes its temporaries from an arena the subject owns**

`board:1463` closed on the FRAME path: four phases take nothing on every frame, every tag reads zero
after the settling point, and what is left is 80 bytes a frame inside SDL's own arena. **These three
lines were written in that item and are about the BUILD path**, which is a different question with a
different answer, so they moved here whole rather than keeping a closed item open.

**The established answer**: Unreal's `FMemStack` is a linear stack the frame's temporaries are pushed
onto and an `FMemMark` pops the whole lot at once, and `std::pmr::monotonic_buffer_resource` with
`release()` is the same shape in the standard library. **The assumption that comes with it**: the
temporaries are dead at a KNOWN point and nothing outlives the mark -- which is true of a build and is
what makes a linear stack legitimate rather than merely fast.

## What must be true

- [ ] **A build's temporaries come from an arena the subject owns**, released once at the end of the
  build -- one `release()` for the whole build, which is the `FMemMark` shape and not fifteen rewrites
- [ ] **The arena's capacity is declared and reused across builds**, so the first build grows it and no
  later one takes anything from the allocator at all
- [ ] **A build that outgrows the arena is a REFUSAL naming the bound**, not a silent fallback to the
  heap -- the RAGE half of the answer, and the half a monotonic resource does not give for free

## What it is worth, measured before it is built

[MEASURED] `BoxAnimated`, 500 frames: `untagged` takes **24 778 928 bytes**, of which **6 713 552 after
the settling point**. That is the largest remaining term in the run by two orders of magnitude, and
none of it is on the frame path -- the four phases read zero. **So this is a LOAD-time cost and the
case for it is stand-up latency and fragmentation over a long session, not the frame.**

**The attribution has to come first.** `untagged` is everything no `Heap::Tagged` scope covers, and
6.7 MB after settling means something is still being taken repeatedly outside every phase this run
brackets. *A round that built an arena before naming what fills it would be an arena for a term nobody
measured*, which is the mistake `board:1463` made once already and recorded.
