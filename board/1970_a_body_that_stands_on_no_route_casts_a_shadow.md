Type: bug
State: open
Parent: 1953
Depends: 1969

# A body that stands on no route casts a shadow

board:1969 made a body without a journey stand, fall and reach the picture. It casts nothing:
`batches the shadow casts` reads 0 for such a scenario, with or without a declared shadow radius,
while the same subject reached through a drive casts every batch it draws.

A body that is drawn and casts no shadow is lit through itself. It is also the only kind of caster
that MOVES under a still camera, which is why its absence hides a second defect: `PlacedBounds`
cached a walk over every vertex in `BoundsPlaced_`, so the shadow frustum's centre was computed
once and never again while the subject stood. That is repaired -- a volume per part, folded through
each part's placement on every call -- and it cannot be PROVEN until something both moves and
casts.

Two candidates for the cause, neither confirmed:

    ShadowRadiusStoodM_   the plan carries `LightVisibility` only when it is above zero, and a
                          declared `Lit.ShadowRadiusM` did not appear to reach it on this path
    CastsBelow            `Cast` skips a batch whose `ModelSlot >= CastsBelow_`, and `Carry` is
                          what sets it from `Joined_`

- [ ] a freestanding body casts every batch it draws, as a driven one does
- [ ] the shadow frustum's centre follows a caster that moves under a still camera, which is the
      proof board:1926's fourth checkbox owes and this blocks
