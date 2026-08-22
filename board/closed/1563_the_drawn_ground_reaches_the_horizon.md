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

- [x] **ZERO dark horizon pixels over all twelve stations** (2026-08-22, 120 rows sampled per
      frame), by three mechanisms together: the 12 km far ring (240 m posts), earth curvature on
      its heights, and Hillaire's ground-albedo bounce filling the sky's ground half past the rim
- [x] the ring's cost is stated beside its reach: `kHorizonReachM`/`kFarStepM` sit with the grid
      constants; 10 201 GroundAt samples per relay, and a missing far sample carries the last
      height rather than sea level

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


## CLOSED -- and the residue's history is the lesson

Full-width black bands -> one-pixel line (far ring) -> WIDER line (earth curvature, correct but
exposing more of the unlit LUT ground) -> zero (ground-albedo bounce). Each step was measured over
the same twelve-station population, and the middle step made the number worse while being
physically right -- the kind of intermediate a single-number gate would have rejected. The bounce
is the reference's own term with the albedo the reference sets to zero; grassland's (0.10, 0.13,
0.07) turns the last black into the earth's own light.
