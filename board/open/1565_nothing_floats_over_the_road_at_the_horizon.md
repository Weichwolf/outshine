Type: bug
Area: world
Tags: bug

**Nothing floats over the road at the horizon**

`km0708.1-third.png` (2026-08-22): a detached white sliver of geometry hangs above the road at the
horizon, attached to nothing. `km0036.5-third.png` shows a smaller speck at the vanishing point.

Suspects: the carriageway ribbon drawn past the ground reach (board:1558's hairline, seen edge-on
and lit), or a far segment of the deck standing on an embankment whose fill the ground grid has not
graded at that distance -- the grading reach and the shown reach disagree (400 m against 900 m), so
everything between them is deck with no ground under it. If that is the mechanism, this item closes
with board:1558's reach unification and one still proves it.

- [ ] name the floating geometry (dump what stands at that station's far window)
- [ ] one reach, derived, for shown corridor and graded ground

## Comments

Filed from the reviewer's third round, ranked ninth of eleven.
