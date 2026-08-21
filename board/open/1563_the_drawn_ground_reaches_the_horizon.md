Type: bug
Area: world
Tags: bug, scope

**The drawn ground reaches the horizon**

The reviewer's third round (2026-08-22, nine stills) ranks this third of eleven: **black void bands
at the horizon in seven of nine frames** -- the drawn ground ends at `kGroundReachM` = 400 m, and
past its rim the sky's ground half answers with the medium's honest near-black (measured in
board:1558's comment: 0.0028 against 0.19 at the horizon from 10 m up). At km 61.4 the rim's
CURVATURE is visible across the full frame width.

What a shipped title does: a far low-detail ring plus aerial perspective, so the streaming edge is
physically invisible. Both halves already exist here in parts: the ground grid can be ringed
coarsely, and the sky chain provides the haze colour the far ring fades into.

- [ ] the drawn ground reaches at least the 10 m eye's horizon distance (11.3 km) at some LOD, or
      aerial perspective hides the rim -- either mechanism, measured by zero black band pixels at
      the horizon over the twelve stations
- [ ] the ring's cost is stated beside its reach

## Comments

Filed from the reviewer's round; the band was first measured in board:1558's comments and is now
ranked by an outside eye as the third-worst thing in the picture.

**The far ring stands** (2026-08-22 01:33): a second, coarser grid from 400 m to 12 km
(`kHorizonReachM`, 240 m posts, a missing far sample carries the last height rather than sea
level), indexed as a ring around the fine grid. Measured over the twelve fresh first-person
stills, 120 horizon rows sampled per frame: **seven of twelve stations at ZERO dark pixels**, and
the residue is a one-pixel line (463 px at km 61) -- the angular gap between the ring's silhouette
seen from the 2 m eye and the sky LUT's horizon computed for its 10 m table eye. The remaining
checkbox closes when the LUT eye height follows the camera.
