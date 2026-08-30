Type: feature
State: active
Area: render
Tags: picture, plan, driver, measured

# The sky carries its sun, moon and stars, and the exposure is derived

**Benchmark** — Unreal: `SkyAtmosphere` plus `SkyLight`, and exposure from the scene through auto-exposure. RAGE: a timecycle of hand-authored curves driving sky colour and exposure together. **Taking Unreal** — a curve is a look that has to be re-authored per condition; a derived exposure follows whatever the sky actually does.

The TARGET render plan draws `sun`, `moon`, `stars` into `SceneHdr` and derives exposure
through `irradiance -> autoExposure`. CURRENT has none of the five: the plan ends at `sky`,
and `Ephemeris` (Ephemeris.h:11) computes a direction nothing draws a disc for.

## Measured on the acceptance stills at 84115df7, review's own worktree

Nine stills, 1280x720, key 40 000 lx at elevation 42 deg, bearing 150 deg. The frame carries
colour for the first time (board:1870) and what it does NOT carry is now readable:

| what | measured | what a photograph at 42 deg would give |
|---|---|---|
| horizon sky band, sRGB | (91,105,114) = linear 0.168 | correctly exposed, near mid-grey |
| everything below the horizon | (34,42,32) = linear 0.016, uniform to 1 count across all nine | sunlit ground of albedo 0.1 within ~1 stop of the horizon sky |
| the sun | **absent in all nine**, at any bearing | a disc, and the sky bright around it |

The ground half is **3.4 stops** below the sky it meets. That is not an exposure defect -- the
sky is exposed correctly -- it is that the lower half is `ParticipatingMedium.h:29`'s
`GroundAlbedo[3] = {0.10, 0.13, 0.07}` seen through the whole atmosphere at grazing incidence,
with no aerial perspective washing it toward the horizon and no surface under it (board:1890).
A picture whose ground reads darker than its sky at midday is telling the truth about what it
drew and a lie about the place.

## Re-measured at bb9472db, 32 stills, four runs -- and the sun is now judged by CONSEQUENCE

The ground half of the paragraph above has moved: a composed ground now stands under the car and
reads (61,76,60) against a horizon sky band at (127..131), which is within a stop and correct.
Three things it did not move:

| what | measured | what it should be |
|---|---|---|
| frame peak, chase view, 8 stills x 2 routes | **149 of 255** | a midday scene reaches white somewhere |
| frame peak, first person | **130 of 255** | as above |
| the exposure applied | **5.20833e-05 1/(cd/m2), IDENTICAL on both routes and every frame** | derived from the scene, so it changes when the scene does |

An exposure that is the same constant on a route whose ground swings by 15x (board:1935) is not
reading anything. The whole picture lives in the bottom 58 % of the range and nothing is
overexposed anywhere, which is what a fixed divisor with headroom looks like.

**The sun disc cannot be reached from the product.** At 42 deg elevation it is above the top of
a 55 deg frame, and `outshine-driver --help` offers no way to point the camera up. So it is
judged by its two consequences instead, and both are absent: no highlight on the paintwork
(roof and tailgate read the same to one count, board:1934) and no cast shadow on the ground
(0.7 counts, board:1575). A sun with neither consequence is not in the scene whatever the sky
draws.

A driver at dusk is the case that needs all of it — the clock says the sun is at the horizon
and the picture must agree, which is the first row of the driver's ledger that no engine
capability answers today.

## What will be true

- [ ] `sun`, `moon` and `stars` are stages of the declared plan, each a catalogue row with its
      own `Writes`, drawn from the scenario's clock and the sphere's declaration — never from a
      constant named after a planet (board:1611).
- [ ] `irradiance` reduces the medium's radiance once per frame, and `autoExposure` reads it —
      the tonemap stops carrying a picture decision it was never declared to own.
- [ ] `ambientOcclusion` is decided or refused by measurement, and the decision is argued in
      TARGET before a stage is written.
- [ ] Proving test: a still at three declared clock values differs in sun elevation, and the
      disc sits where `Ephemeris` says; negative control — the clock frozen, the three stills
      are identical.
