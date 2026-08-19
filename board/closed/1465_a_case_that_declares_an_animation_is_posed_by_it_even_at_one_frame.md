Type: bug
Area: harness
Tags: oracle, khronos

**A case that declares an animation is posed by it, even at one frame**

A manifest that names `scene.animation.animations` gets that animation applied, whatever its grid is
long. Whether a case is ANIMATED is what its declaration says, not how many frames it is decided at.

## What it is

`Case::Animated()` answers from the frame COUNT, so a case declaring one frame is built from the file's
rest pose while the oracle -- which applies what the file carries -- renders the animated one. **The two
sides then compare different bodies and neither says so.**

## Where it surfaced, and it took a corpus that has such files

[MEASURED] `Animation_NodeMisc_03` of the glTF-Asset-Generator keys ONE frame, translating its node by
`[-0.1, 0, 0]`. Blender applies it: the camera derived from the posed scene reports bounds
`x in [-0.4, 0.2]`, which is the 0.6 cube moved by -0.1. This engine builds the rest pose, `x in
[-0.3, 0.3]`, and its vertex 6 then sits **3.138705 m** along the view axis against a near plane of
**3.191292 m** -- 0.0526 m inside it, which is half the translation. The case refuses rather than
rendering, which is the only reason it was noticed at all.

`Animation_NodeMisc_05` is the same defect with a different face: it overrides a rotation to a
CONSTANT, so the animation cannot move the subject and can very much change where it is. It fails on
`disagreement_p99_px`.

## Why declaring two frames is not the answer

A two-frame grid over a channel that cannot move the pose renders one thing twice, and the sequence
check refuses it by name -- *the declared grid changes the picture ... so the sequence is not a still
rendered once per frame and agreeing with the oracle by construction*. **Both refusals are right and
they close on each other**, which is what says the fault is in the question rather than in either
answer.

## What must be true

- [x] **`Animated()` answers from the DECLARATION** -- a case that names `animations` is posed by them
  at every frame of its grid, including a grid of one
- [x] **A one-frame animated case still poses.** The previous-pose run and the velocity target are a
  SEQUENCE's business and stay tied to the frame count, which is a different question in the same
  struct and is why the two were conflated
- [x] **The whole corpus is re-measured**, because the change reaches every case that declares an
  animation -- and a case whose picture moves is a case that was being compared at the wrong pose

## What it became

`Case::Posed()` answers from the declaration and `Case::Animated()` stays the frame count's question --
the product names, the previous pose the velocity target differences against, and every claim about a
grid changing the picture are all about having MORE THAN ONE frame, and none of them is about whether
an animation applies.

[MEASURED] `Animation_Node_03`'s near-plane refusal moved from **3.138705 m to 3.061730 m**, which is
the pose changing; the whole corpus is unchanged, because no case in it had ever declared a one-frame
grid and the conflation was correct about every one of them.

## Comments

**This is what a new corpus is for.** 148 cases over two years never declared a one-frame animation,
so the conflation was correct about every case in the tree and wrong about the question. Thirty-four
generated models found it on the day they arrived.
