Type: feature
State: open
Parent: 1573
Area: apps
Tags: driver, acceptance, product

# The driver drives at the bar, and the architect signs it off

`apps/driver` is outshine's ONE integration test and simultaneously its product. Everything it
uses is library, and what the library owes is corpus cases against invariant oracles. Emergence is
judged HERE, on the picture, from what was SEEN by RUNNING the programme.

**The day the driver is a driving simulation at Gran Turismo 7's level in an OSM world and the
architect accepts it, outshine's integration test has passed.**

## The ledger -- 2026-08-26 06:3x, HEAD a73c6ca5, own worktree

```
make                                                -> EXIT 0, both apps link, 164 objects
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   DROVE 15466 frames over 2.896 of 2.916 km, kept 10 still(s)     <- no mkdir, board:1903 gone
```

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, on the set `LayerIncludes` declares |
| does the gate build it? | **yes** | `make` exits 0 |
| does the acceptance command leave its stills? | **YES, ten of ten** | into a directory that did not exist. Three rounds opened with `mkdir`; this one did not |
| does the DECLARED drive ROUTE? | **yes** | 2.896 of 2.916 km, and the two corridor refusals of the last round are gone (board:1912) |
| do consecutive stills DIFFER? | **yes** | 17.0 % to 29.4 % of pixels between consecutive stills; none below 17 % |
| is there a sky? | **yes, a gradient** | (46,74,107) high, (83,97,108) at the horizon; the sun's bearing rotates through the drive |
| **is there GROUND under the car?** | **NO -- and nine tiles of terrain arrived anyway** | below the horizon, in ALL TEN stills, the frame is (34..35, 42..43, 32..34) -- four counts of variation over 2.9 km, which is the medium's painted `GroundAlbedo`. The 9-tile ring draws as a pale sheet ABOVE the horizon: (11,24,50) then (33,60,107) at x=700 in still 03, gone again by still 05. The terrain is in the SKY (board:1890) |
| is the car lit? | **yes** | A-pillar, mirror, dashboard and roof all carry shading; the cabin is dark because it is in shadow |
| does it cast a shadow? | **onto nothing** | 259 shadow batches, no receiving surface |
| does it sit on a surface or float? | **it floats** | there is no surface |
| **how bright is the frame?** | **dim, and dimmer than last round** | mean max(RGB) 43.3-49.0 of 255, peak **119-162** against 250 last round -- the specular that carried 250 is gone. `<key lux="40000" elevationDeg="42">` and a scene that reads at a fifth of the range (board:1908) |
| is there a SUN in the sky? | **NO, in none of the ten** | key elevation 42 deg, bearing 150 deg, no disc at any heading (board:1868) |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **NO** | 37 of 150 sources reach no suite and the generators are 30 of them (STATE.md) |
| does a key move the car? | **NO** | `apps/driver` offers no `Host` (board:1803) |
| what does the picture do at one kilometre it does not do at another? | **the ring appears and disappears** | stills 01 and 03 carry the pale sheet at the horizon, 05 and 10 do not. The lower half is the same four counts throughout |

Against the bar -- Gran Turismo 7 on PS4 -- the gap is unchanged in KIND and the tiles now exist
to close it. GT7's weakest still has a road, a kerb, a verge and a building; this has a car, a
sky, a flat painted field and a piece of terrain in the wrong place. The material response inside
the cabin would not embarrass GT7 at this size; everything outside the glass would.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. the ring stands at the corridor's datum, not the ellipsoid's, so the terrain is UNDER the car
   instead of over the horizon (board:1890) -- everything below waits on it
2. the ring carries its own material and its own scale as a compositor draw item, not
   `Material = 0` on the vehicle's first surface (board:1890)
3. the frame is exposed to the declared key: 40 klux and elevation 42 deg reading at a fifth of
   the range is a unit that decides nothing (board:1908, 1868)
4. the sun is in the sky (board:1868)
5. the car's shadow lands on that ground (board:1575)
6. road furniture: markings, guard rails, the second carriageway
7. a bridge is above what it crosses, not coplanar with it (board:1813)
8. a key that moves the car, press AND release (board:1803)
9. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version -- a picture that agrees only with itself measures nothing.
