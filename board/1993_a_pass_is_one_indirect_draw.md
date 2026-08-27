Type: feature
State: open
Parent: 1995
Depends: 1992
Area: render
Tags: benchmark, target, gpu-driven

# A pass is ONE indirect draw whose count the GPU wrote

**Benchmark** — Unreal: a Nanite pass issues an indirect draw whose argument buffer the culling compute stage filled, so the CPU never learns how much survived. RAGE: the device consumes a prepared draw list. **Taking Unreal** — an indirect draw is the only shape where the CPU's work is O(1) in the scene, and it is the end state the other four items exist to make possible. **Reference**: Haar & Aaltonen, *GPU-Driven Rendering Pipelines*, SIGGRAPH 2015 — `MultiDrawIndirect` fed from a GPU-written argument buffer, and the observation that the CPU cost becomes a constant.

## What

The subject pass records one indirect draw. The draw count, the instance count and the first
index come from a buffer the compute stage wrote.

## Why

This is where "no CPU term scales" stops being a claim and becomes a measurement. Every step
before it reduces the CPU's per-item work; this one removes the last of it — the CPU stops
knowing how many things there are.

Today: `SubjectDraw.cpp:828` issues `SDL_DrawGPUIndexedPrimitives` inside a loop over batches, one
call per batch. Board:1989 merges identical batches, board:1992 decides which survive — and after
both, the loop is still a CPU loop over a GPU-decided set, which is the last place a readback
would sneak back in.

## How

The culling stage writes an indirect argument buffer; the pass records
`SDL_DrawGPUIndexedPrimitivesIndirect` against it. Whether SDL_GPU exposes a multi-draw variant
or one indirect draw per material class is a question for the device layer and is answered
before the code moves, not during it.

**What would show this was wrong**: if the argument-buffer round trip costs more than the draws
it replaces at this scene size, then a 720p engine with tens of subjects does not need what an
engine with tens of thousands does. Frame time over `apps/driver` before and after is the number,
and board:1989 takes the baseline so there is something to compare against.

- [ ] the culling stage writes an indirect argument buffer
- [ ] the subject pass records ONE indirect draw per material class, not one per batch
- [ ] the CPU's draw-call count over a drive is CONSTANT in the number of subjects -- published
      as a measure so a regression is visible
