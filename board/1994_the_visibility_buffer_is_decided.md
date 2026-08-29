Type: issue
State: open
Parent: 1995
Area: render
Tags: benchmark, gpu-driven

# The visibility buffer is DECIDED, with its reason, rather than assumed in or out

**Benchmark** — Unreal: Nanite writes a visibility buffer — cluster id and triangle id per pixel — and shades in a later compute pass over material tiles. RAGE: a conventional G-buffer. **Neither answer is obviously right here**, which is why this is an issue rather than a feature: the row has to be decided before it is built, and the decision is the deliverable. **References**: Burns & Hunt, *The Visibility Buffer: A Cache-Friendly Approach to Deferred Shading*, JCGT 2013 — the original, and its argument is about bandwidth and cache rather than about geometry; Karis, *Nanite: A Deep Dive*, SIGGRAPH 2021, for what it becomes when paired with a compute rasteriser.

## What

Write `(cluster, triangle)` per pixel and resolve attributes later, instead of writing a G-buffer
of interpolated attributes.

## Why it is a question and not a step

Its win has two sources and this tree only gets one of them.

**Bandwidth**: a visibility buffer writes 8 bytes per pixel where a G-buffer writes 20 to 40.
That is real at any resolution and would be real here.

**Overshading**: Nanite's larger win is that a compute rasteriser plus a visibility buffer avoids
shading a sub-pixel triangle four times over in a 2x2 quad. **This tree refuses the compute
rasteriser** (board:1636 holds the separate question of a software executor; the refusal's reason
is that we generate our geometry and choose its density, so a DAG tuned to 720p never asks the
hardware for a sub-pixel triangle). Without it, that half of the win is not available.

What is left is a bandwidth argument on a device with unified memory and a 720p target, against
the cost of a second shading pass and a material-tile classification. That is a real trade and it
deserves a number, not a preference.

## How the decision gets made

Measure the G-buffer's bandwidth over a drive: bytes written per frame by the geometry pass, and
what fraction of the frame budget that is on this device. If it is small, the answer is no and
this item closes as a decision rather than a build.

- [ ] the geometry pass's per-frame write bandwidth is measured over the driver client (deleted)
- [ ] the row is written into CLAUDE.md's settled table with its number -- taken or refused, and
      an item that ends in a REFUSAL is closed the same as one that ends in code
