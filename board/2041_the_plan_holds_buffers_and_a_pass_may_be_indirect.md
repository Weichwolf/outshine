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
