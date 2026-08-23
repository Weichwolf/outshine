Type: bug
Area: render
Tags: khronos, instrument

**No picture has ever shaded a back-facing fragment, on any lit arm**

**0 back-facing shaded fragments of 1 328 002, over all 35 render cases.** Counted from the sign channel
of `outshine.normal.raw`, which every case writes: a fragment is shaded where the shading normal has
non-zero length, and back-facing where its fourth channel is negative. The eight cases that shade at all
report zero, individually and in total.

**Nine lit fragment entry points take `[[front_facing]]`** — the three plain lit arms through `facing()`,
the three lit-textured arms, and the three mapped arms through the basis — and **the back branch of every
one of them is unproven end to end in a picture**. `board:1127` proved the mapped arm's *algebra* on the
device with a shader tie and closed on that; what no test reaches is the **wiring**: that the bool the
rasteriser hands the fragment reaches the branch that flips, on the arm that draws.

**Reachable is not exercised, and this tree has paid for the distinction before.** Three lit cases already
declare `doubleSided: true` — `materials/normal-tangent`, `materials/normal-tangent-mirror`, and two of
the three materials of `lighting/point-light-intensity` — so back faces are **not culled** there and the
branch is live. It is never entered because **no camera in the corpus is behind a surface it lights**.
That is the same shape as `board:0030`'s eighteen catalogue rows and `board:1130`'s unreachable mip chain:
the capability is present, and nothing exercises it.

**So the missing thing is a CAMERA, not an asset**, which is what makes this cheap. Two routes, and the
ladder decides:

- **A second placement on an existing case.** `materials/normal-tangent-mirror` is a flat grid with 374 568
  shaded pixels and `doubleSided: true`; viewed from the other side, its whole shaded population is
  back-facing. It costs a manifest and one case's oracle render, and it couples this to `board:1126`'s
  open front-face disagreement and to the mirrored tangent the asset exists for.
- **A small case of its own** — a double-sided quad with a normal map, camera behind it — which decouples
  all three and is the cleaner instrument. It is a new subject, so it is `board:0078`'s ladder.

**ONE CONSTRAINT THE CASE MUST MEET OR IT DECIDES NOTHING, and it is not obvious.** A back-facing fragment
whose light is on the *other* side has `nl <= 0`, so the light loop skips every light and the fragment
comes back black — the count would go non-zero while the picture proved only that the branch was entered.
**The light has to be on the camera's side of the surface**, so that the flipped normal faces it and the
basis actually decides a value. A case that gets this wrong reads as a pass.

**BLOCKED ON `board:1148` FOR ITS SECOND HALF, AND NOT FOR ITS FIRST — which is why it carries no
`Depends:`.** Producing the population and publishing the count needs nothing: it is our own render and our
own channel. *Scoring* those fragments against Cycles needs the oracle's back-face convention for a
tangent-space normal map, which is unestablished here (`board:1148`). Written as a line in the body,
because that is what this board does with a partial block, and an item held out of *ready* for half of
itself is an item nobody starts.

**The developer's suggested `Depends: 1126` is judged and refused.** `board:1126` is a **confound to
control for**, not a blocker: it is an open disagreement about the *front*-facing normal on exactly the
assets route A would use, so a back-face disagreement measured there could not be attributed. That is an
argument for route B or for reporting the two populations separately — both of which are actions this item
can take today — and not an argument that the work cannot start. A `Depends:` written for a confound
removes an item from *what is ready* for a reason the body can state better.

**Done when** at least one case reports a non-zero back-facing shaded population, that population is lit
rather than merely present, and the count is published beside the front-facing one — so that a later
change to any lit arm's facing branch has a picture that would move.

---

Closed -- route B, as its own instrument: ABackFacingFragmentShadesLit stands a double-sided
tangentless quad, views it from behind (whole shaded population back-facing: 2704 px, front
view 0), with the light on the CAMERA'S side so the flipped normal is LIT (radiance 6761
summed linear, ~93% of the front view's) -- the constraint the body named, met. Both counts
publish side by side, so a change to any lit arm's facing branch has a picture that moves.
The Cycles-scored half stays where the body put it: blocked on 1148's back-face convention,
and the mapped/textured arms' back branches ride the same facing() the case exercises
(1127's algebra tie covers the basis).
