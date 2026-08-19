Type: feature
Area: gltf
Tags: perf, instrument
Depends: 1462

**The frame path allocates nothing**

An advance takes no memory from the allocator and returns none. **A frame that allocates nothing cannot
leak, cannot fragment and cannot stall in an allocator**, and equality between one frame and the same
pose a lap later follows by construction instead of by measurement.

## The established answer, and both halves of it say the same thing

| | |
|---|---|
| **Unreal** | `FMemStack` -- a linear stack the frame's temporaries are pushed onto, and an `FMemMark` that pops the whole frame at once. Nothing is returned individually because nothing needs to be |
| **RAGE** | pools sized at build time and declared in content; exceeding one is a **refusal**, never a heap that grows |
| **`CLAUDE.md`** | *the frame path is made of bounded terms -- every step in it costs a number somebody can name, which is exactly what an allocation is not, so those live at load* |

**The standard library ships the same shape**: `std::pmr::monotonic_buffer_resource` is `FMemStack`, and
`release()` is `FMemMark`. `ES.1` and `SL.1` say to prefer it, so the mechanism needs no author here.

## What it costs today, measured and attributed

[MEASURED] `BoxAnimated`, 500 frames, over the engine's own allocator (`board:1462`, not the process's
zone):

| | |
|---|---|
| frames after settling whose live bytes moved at all | **172 of 249** |
| the mean difference one lap apart | -0.4783 bytes a frame -- it **oscillates and does not leak** |
| the live zone's churn, process-wide | **1.33 MB a frame** |
| an animated advance against a still one | 0.3154 ms against **0.0654 ms** p50 |

**Nothing grows and that is not the point.** Taking and returning a megabyte on two frames in three is
work, it fragments, and it is where the difference between a still frame and an animated one lives.

## THE FIRST CUT IS DONE AND IT MOVED THE POPULATION, so the next address is a different one

**`Clients::Live::Submit` rebuilt the whole studio on every advance** -- an empty `Studio`, every surface
copied, every emitted radiance assigned, every light pushed, and the camera re-derived -- when the only
thing that changes between two frames of an animated subject is which body two pointers name. It stands
once now, at stand-up.

| phase | frames it moved the heap on, of 250 | before | after |
|---|---|---|---|
| posing | | 15 | **7** |
| **submitting** | | 223 | **192** |
| aiming | | 0 | **0** |
| drawing | | 71 | **51** |

**A SECOND DEFECT CAME OUT WITH IT AND IT IS THE ONE WORTH KEEPING.** With the camera no longer
re-derived per pose it stood still -- which is what a scenario wants -- and `BoxAnimated` then stopped
advancing after 22 frames, because the body walks INSIDE the near plane and `Aim` refuses. **The bounds
a camera is framed from are the GRID's and not the rest pose's** (`board:1433` says the same thing for
the corpus), so the union over the whole grid is taken once at stand-up and the eye and aim stay the
rest pose's; what opens is the depth window.

## Where the rest of it is, read rather than guessed

**THE ATTRIBUTION REFUTED THE OBVIOUS ANSWER, twice.** The flattener's temporaries looked like the
culprit -- `Document::ReadElements` decodes each accessor into a fresh `std::vector<double>` and
`Subject.cpp` declares roughly fifteen more -- and POSING moves the heap on **7 frames in 250**.
`Gltf::Subject`'s own arrays already reuse their storage, and `clear()` keeps capacity.

**What remains is `Clients::Show` and what it hands the renderer**: the index run copied out of the
subject, `PackVertices` filling the interleaved buffer, and `SetSubjectMaterials`, `SetSubjectLights`
and `SetSubjectMesh` taking vectors by value on the other side of the boundary. That is the next cut and
it is renderer-side, not reader-side.

## What must be true

- [ ] **A build's temporaries come from an arena the subject owns**, released once at the end of the
  build -- one `release()` for the whole frame, which is the `FMemMark` shape and not fifteen rewrites
- [ ] **The arena's capacity is declared and reused across builds**, so the first build grows it and no
  later one takes anything from the allocator at all
- [ ] **A build that outgrows the arena is a REFUSAL naming the bound**, not a silent fallback to the
  heap -- the RAGE half of the answer, and the half a monotonic resource does not give for free
- [ ] **`AnEngineInSteadyStateReturnsToTheSameLiveByteCount` goes green**, and it goes green by equality
  rather than by a tolerance
- [ ] **The still and animated advance costs are re-measured**, because the arena is expected to move
  the 0.25 ms between them and a repair whose measurement comes back identical never reached what it
  was aimed at

## What this feature may NOT do

**It may not put an allocation on the free path.** A table from pointer to size, a per-temporary
registry, a shared_ptr anywhere in it -- each of those is the defect this item removes, wearing the
costume of the fix.

## Comments

**The claim is byte-exact because the horizon is.** Suspend and quick resume put one process at
21 600 000 frames over a hundred hours, where one byte a frame is 21.6 MB. No run a suite can afford is
statistical enough to see that, so the instrument compares a pose with itself and the repair has to make
the difference exactly zero rather than small.
