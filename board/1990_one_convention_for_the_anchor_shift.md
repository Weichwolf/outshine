Type: bug
State: open
Parent: 1995
Area: render
Tags: measured, gpu-driven

# The anchor shift is applied ONE way, and the model matrix is where it lands

**Benchmark** — Unreal: `FViewMatrices::PreViewTranslation` is subtracted once and every transform is built against that one; a primitive's transform carries it and the shader never re-applies it. RAGE: the same, one camera-relative origin per frame. **Both agree**, and CLAUDE.md already states it as an invariant. **The defect is ours alone.**

## What

`SubjectDraw` applies the anchor shift in two different places depending on whether a placement
table exists.

## Why it matters

Measured, with R a 90-degree turn about Z:

    R*(p+s)+T = [  8, 101, 3]     without placements -- the shift is added to the VERTEX
    R*p+(T+s) = [108,   1, 3]     with placements -- the shift is added to the model's TRANSLATION

They agree only while the model matrix is IDENTITY, which it is today
(`SubjectDraw.h:227` initialises it so). The tree therefore carries two conventions that have not
yet had cause to disagree, and the first rotated subject placed through the placement path draws
in the wrong place with no error anywhere.

CLAUDE.md: *the frame picks ONE origin; view, light and every instance transform build against
THAT one. A subsystem that subtracts its own origin is the defect that costs whoever gets it
right.* Two spellings of where the shift lands is that defect one level down.

## How

The model matrix carries the shift, always — `carried[12+axis] += Anchor + PreViewTranslation`
unconditionally — and `S::anc` goes away rather than being zero on one path and meaningful on the
other.

This blocks board:1989 step 3: a shader that reads a placement per instance cannot ALSO have a
per-draw `anc` that means different things.

- [ ] the shift is applied in one place, and `S::anc` is gone
- [ ] a case places a ROTATED subject with and without a placement row and reads the same pixels
      -- it cannot be written today, which is what makes this a bug rather than a preference
