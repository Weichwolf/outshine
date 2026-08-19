Type: bug
Area: corpus
Tags: oracle, khronos

**A grid lands where the pose is furthest from where it started**

A two-frame case samples the instant at which its subject has moved the most, so the comparison has the
largest signal the file offers. The rate is derived from the animation's own keying and not from a
constant.

## What it is

The grid is `frame / fps` from zero, and the generator's animation groups key channels over spans that
neither start at zero nor return to their start at the midpoint:

| model | what it keys | what the current grid samples |
|---|---|---|
| `Animation_NodeMisc_01` | one rotation channel, 1 s to 5 s, through a full circle | 0 s and 2.5 s -- and the pose at 2.5 s is where the rotation is back at its start |
| `Animation_NodeMisc_02` | translation 2 s to 6 s and rotation 1 s to 5 s | 0 s and 3 s -- the two channels' contributions cancel in the picture |

Both report **0 frames whose picture differs from frame 0** and the sequence check refuses them, which
is right: a grid that renders one pose twice agrees with the oracle by construction.

## What was tried and refuted, because the route is worth keeping

**A fixed 2 fps** sampled 0 s and 0.5 s, which for `Animation_Node_03`'s STEP interpolation is twice
inside one step -- the same pose. **The span's END** is where a rotation comes back. **The span's
MIDDLE** is where these two put it back. *Each was measured, each was wrong for a different model, and
the pattern is that no constant fraction of a span is right for every curve.*

## What must be true

- [ ] **The instant is chosen by EVALUATING the animation**, not by a fraction of its span: sample the
  curve, take the pose whose furthest vertex is furthest from frame 0's, and derive the rate from that
- [ ] **The evaluation is the preparer's**, where the sampler already lives, and its answer is
  published beside the grid so a reader can see which instant was chosen and why
- [ ] **A file whose pose never differs from frame 0's declares one frame**, which is `board:1465`'s
  other half: it is a still that carries an animation, and both sides must pose it the same way

## Comments

Thirty of the thirty-four generated cases hold on the current rule. These two are what a *derived* grid
would fix and a *declared* one would paper over -- the manifest could simply state the instant, and
then the number nobody re-derives becomes a fact.
