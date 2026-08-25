Type: bug
State: open
Area: render
Tags: bug

**Both tail lights burn, or neither does**

Round five, two stations: the right tail light glows red and the left is dark, with no brake input
in frame to explain one side (`km0061.4-third`, `km0117.4-third`, 2026-08-22). Paired emissives
share one state; a one-sided lamp is either a material-table row that resolved differently for the
mirrored part, or an emissive the asset carries on one side only.

- [ ] name both lamp parts and their resolved material rows
- [ ] the pair shares one emissive state, looked at from behind

## Comments

Filed from the reviewer's fifth round, ranked third of eight; the same family as board:1569 (the
exhaust tip) -- single parts of the joined asset resolving to the wrong surface.
