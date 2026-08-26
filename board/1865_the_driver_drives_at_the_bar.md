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

## The ledger -- 2026-08-26 03:1x, HEAD 84115df7, own worktree

```
make                                                    -> EXIT 0, both apps link, 164 objects
sh test/run.sh                                          -> EXIT 1, 16 RED (board:1799)
                                                           1829 PASS 0 FAIL, 1 unbuilt, 15 unprepared
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
   ROUTED the declared drive
   REFUSED the screenshot could not be opened for writing at DIR/along01.png   <- board:1903,
   exit 1, no directory, no stills                                                third round
mkdir -p DIR && the same command
   DROVE 15531 frames over 2.895 of 2.915 km, kept 9 still(s)
```

| question | answer | what proves it |
|---|---|---|
| is there a program a user runs? | **yes** | `apps/driver/src/main.cpp`, `-Iinclude` alone |
| does the gate build it? | **yes** | `make` exits 0 |
| does the DECLARED drive ROUTE? | **YES, and that is new** | 2.895 of 2.915 km. Three items did it: the tie index kept current (1894), at-grade crossings made junctions (1911), the target named as a component (1862). The graph went 4193 pieces -> 284 |
| does the acceptance command leave its stills? | **NO** | board:1903, unfixed for three rounds. With `mkdir` first: nine of ten |
| do consecutive stills DIFFER? | **yes** | 18-30 % of pixels change between consecutive stills; still 01->02 only 5.7 % |
| **is there COLOUR in the frame?** | **YES, and that is the round's headline** | mean max(RGB) **43.3 to 48.7** of 255 across nine stills, peak 250, **alpha 255 on every pixel**, 65 % of pixels above 32. Last round: mean 0.45, two alpha values, 0.77 % above 32. A declared sphere with air now carries a sky (board:1870) and the presented frame is opaque at the tonemap (board:1908) |
| is there a horizon? | **yes** | a hard line at y = 360, dead level, in all nine |
| is there a sky above it? | **yes, a gradient** | (57,85,116) at the top down to (91,105,114) at the horizon. Linear 0.168 at the band -- correctly exposed |
| is there GROUND under the car? | **NO -- there is a painted plane** | everything below the horizon is `ParticipatingMedium.h:29` `GroundAlbedo = {0.10,0.13,0.07}` seen through the atmosphere: (34,42,32), **uniform to one count over 2.895 km**. 258 batches drawn, all of them the car. `Engine::State::Composes` still refuses: *"a drive stands, and the ground is APPENDED to the driven vehicle's own glTF"* (board:1890) |
| is the car lit? | **yes** | the bonnet carries a moving specular and a reflected sky; the near door mirror carries a highlight. The cabin is dark because it is in shadow, which is correct |
| does it cast a shadow? | **onto nothing** | 258 shadow batches, no surface to receive them |
| does it sit on a surface or float? | **it floats** | there is no surface |
| is there a SUN in the sky? | **NO, in none of the nine** | key elevation 42 deg, bearing 150 deg, and no disc at any camera heading (board:1868) |
| road markings, guard rails, an oncoming carriageway? | **NO** | nothing declares them |
| buildings, trees, water beside the road? | **NO** | 37 of 150 sources reach no suite and the generators are 30 of them (STATE.md) |
| does a key move the car? | **NO** | `apps/driver` offers no `Host` (board:1803) |
| what does the picture do at one kilometre it does not do at another? | **the sky rotates and the bonnet reflection with it** | and nothing else. The lower half is byte-stable |

Against the bar -- Gran Turismo 7 on PS4 -- the frame is now on the same axis for the first time
and the gap is one thing: **GT7's weakest still has a ROAD under the car.** This has a car, a sky,
a horizon and a green field that is not a place. The material response on the bonnet would not
embarrass GT7 at this size; the world would.

**NICHT ABGENOMMEN.** Between this picture and the bar, in order:

1. ground under the car: the ring is a COMPOSITOR draw item with its own scale, not a part
   appended to the vehicle's glTF (board:1890). Everything below waits on it
2. the acceptance command leaves N of N stills without a `mkdir` (board:1903) -- one line, three
   rounds
3. the sun is in the sky and the exposure is derived from the medium, not carried by the tonemap
   (board:1868)
4. the car's shadow lands on that ground (board:1575)
5. road furniture: markings, guard rails, the second carriageway
6. a bridge is above what it crosses, not coplanar with it (board:1813)
7. a key that moves the car, press AND release (board:1803)
8. draw distance, material response and geometry density measured against a PHOTOGRAPH of the
   place the still claims to be

## What will be true

- [ ] Every row of the ledger says yes, and the architect writes *ABGENOMMEN* in its report.
- [ ] Each still is judged against a PHOTOGRAPH of the place it claims to be, not against its
      own previous version -- a picture that agrees only with itself measures nothing.
