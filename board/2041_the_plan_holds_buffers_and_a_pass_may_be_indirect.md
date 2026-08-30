Type: feature
State: open
Parent: 1995
Depends: 1992
Area: render
Tags: benchmark, target, gpu-driven

# The plan holds BUFFERS, and a pass may be INDIRECT

**Benchmark** — Unreal: the Render Dependency Graph registers passes at runtime with their reads
and writes, and its resources are TRANSIENT TEXTURES AND BUFFERS pooled with automatic lifetime,
barriers and culling of passes nothing consumes; Nanite adds its passes per view and per material
bucket, so their number is not known when the engine is compiled. RAGE: an explicit, hand-authored
phase list with named render targets, decided once and legible on the page. **Taking RAGE's SHAPE
and Unreal's RESOURCES**, and the reason is that they are answers to two different questions: what
a frame DOES is a decision worth writing down once, which is why this tree's plan compiles and can
be refused; what a pass READS is data, and data that a GPU-driven pass reads is a BUFFER at least
as often as a texture.

## What stands, and exactly where it stops

`Compiled` pulls a plan from the declared `Outputs`, merges adjacent stages of one kind into
passes, and refuses a plan whose stage nothing reads. That is a static frame graph and it is the
right shape: it is legible, it is decided once, and a client can be told why its plan was refused.

It stops at three places, all of them measured against what board:1992 has to build:

    ResourceKind { Given, Derived, Attachment }    every one of them a TEXTURE
    TexelFormat { Handle, Rgba16Float, ... }       a texel format, so a buffer has no way to say
                                                   its stride or its element
    PassKind { Compute, Raster }                   neither can take its size or its draw from a
                                                   buffer the GPU wrote

A cluster cull needs four buffers in the graph -- the cluster table, the surviving list, the
compacted index run, the indirect argument -- and a draw whose count the GPU decided. None of the
four can be declared today, so they would have to live INSIDE a stage, unseen by the plan. That is
the defect this item exists to prevent: a plan that says what a frame does, with the half that
matters hidden behind one of its own stages.

## What is taken

- **`ResourceKind::Buffer`**, with an element stride and a usage rather than a texel format. It is
  pulled, bound, refused and reported exactly as an attachment is, so nothing else in the compiler
  learns a second way to think.
- **A pass may be INDIRECT**: `PassKind::Compute` gains a dispatch whose group count is read from a
  declared buffer, and `PassKind::Raster` a draw whose arguments are. This is the whole of
  board:1993 -- the CPU's work becomes O(1) in the scene -- and it cannot be said at all today.

## What is REFUSED, and why

**Runtime pass registration is not taken.** Unreal builds its graph every frame from lambdas; this
tree's plan is a CATALOGUE, and that is what lets `--audit-layers` walk it, a case cover it and a
refusal name the stage that was wrong. A graph assembled at runtime cannot be refused before it
runs, and the legibility is worth more here than the flexibility: outshine draws ONE subject path
and a ground, not an editor's arbitrary view stack.

**Where Nanite needs a pass per material bucket**, the answer is one declared stage that issues N
indirect draws from a GPU-written argument table -- N is data, not structure. That keeps the
catalogue fixed and the dispatch count constant, which is what board:1943 asks for.

## What is measured

- [ ] a buffer resource is declared, pulled, and refused when nothing reads it, exactly as a
      texture is -- and the refusal names it
- [ ] `subject draw calls` stays constant while the scene grows, because the count came from a
      buffer rather than from the CPU
- [ ] Shibuya's frame falls from 59.90 ms toward the 16.67 that 60 fps wants, and the factor is
      the one board:1992 measured: 73 706 clusters kept as 17 256

## What this does NOT cover

Nothing here makes a frame faster on its own. It is the vocabulary board:1992 and board:1993 need
in order to be built at all; if the frame does not move once they are, this item was not the
reason and the cause is elsewhere.

## WHAT IS BUILT, AND THE TWO THINGS THE FIRST BUILD MEASURED

`Stage::SubjectCull` stands: a compute stage that sweeps one thread per cluster, rejects the
cluster against the six planes the view matrix already carries, and appends the survivors' indices
into a compacted run while an atomic add on `num_indices` writes the draw's own count. The subject
pass records `SDL_DrawGPUIndexedPrimitivesIndirect` for every batch the culler decided and the
direct draw for every batch it could not.

**It sits BEFORE the shadow and the sky, not in front of the draw**, and that is the one placement
decision worth writing down: a cull immediately ahead of the pass that consumes it is a hard wait,
where here it shares the compute pass the atmosphere already opens and the raster work in between
covers its latency.

### 1. The plan needed a THIRD arm, not a fourth kind

The first attempt gave `ResourceKind` a `Buffer` arm. That conflated two axes -- where a resource
comes from, and what its element is -- and the door said so within one run: `irradiance` is a
compute write with no texel format and no stride, so a check asking every compute write for one
refused a plan that had always been correct. `TexelFormat::Table` is the arm that was missing, a
`static_assert` holds "a table states a stride and a picture states a format" over the whole
catalogue, and `ResourceKind` still means only provenance.

### 2. THE INDEX RUN EXISTED SIX TIMES, and three of those were this item's

Measured on Shibuya, 28.3 M indices at 4 bytes:

    Indices          CPU   113 MB   the subject's own run
    ClusterIndices   CPU   113 MB   the cooked order, kept beside it
    scratch.Indices  CPU   113 MB   the same run repacked in batch order
    Idx              GPU   113 MB   the packed run
    ClusterIdx       GPU   113 MB   the cooked order again
    DrawIdx          GPU   113 MB   what the culler compacts into

678 MB for one list of numbers, and the two biggest places -- Shibuya and Central Park -- were
killed by the system before they drew a frame. `memorystatus_available_pages` fell to 24 147, which
is 96 MB, and the kernel was shedding daemons.

**The cooker reorders `Indices` IN PLACE now** and `ClusterIndices` and `ClusterIdx` are gone,
along with the staging that carried them. A cluster's range is carried in the JOB -- one `uint4`
of cluster, batch, first index and count, already in the packed run's numbering -- so the kernel
reads the same buffer the direct path draws from and no table is gathered to find a range.

**The reorder changes a depth-test tie**, which is what the second run was protecting: where two
surfaces COINCIDE this renderer resolves the tie by arrival order, and Khronos's NormalTangentTest
has one 198x48 cell that moves from 6 codes off the oracle to 8, past its own bound of 6.435.
**That is not a reason to keep the second run.** The engine's layout is decided by what the device
needs, and a vendor corpus is where a standards body states an ANSWER, not where it states a
memory layout; if the importer has to change to keep that case green, the importer is what
changes. Recorded here so the red, when it comes, is the expected one rather than a surprise.
