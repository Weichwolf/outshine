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

- [x] named by mechanism: the swept ribbon's OPEN END cross-section, its unlit inside edge-on --
      dark from the shadow side, white when the sun reached in, which is why the artefact changed
      colour between rounds
- [x] the sweep is a closed solid now: end caps with their own outward normals at both
      cross-sections, held by `test/unit/corridor/ARibbonIsClosedAtBothEnds.cpp` -- every cap
      triangle's winding agrees with its normal by determinant
- [ ] the next reviewer round confirms the vanishing point clean over the twelve stations

## Comments

Filed from the reviewer's third round, ranked ninth of eleven.

Round four: the white sliver at km 708 is gone and DARK dashes stand at the vanishing point at four
stations (36.5, 267.1, 666.2, 708.1) -- the artefact class survived and changed colour, which points
at the corridor ribbon's END at `laidToM`: an open cross-section shows its unlit inside (dark) or
lit outside (white) depending on the sun side. The mechanism to check first: cap or skirt the ribbon
ends, or end the lay beyond the visible span.
