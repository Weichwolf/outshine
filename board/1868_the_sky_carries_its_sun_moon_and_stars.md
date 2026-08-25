Type: feature
State: open
Area: render
Tags: picture, plan, driver

# The sky carries its sun, moon and stars, and the exposure is derived

The TARGET render plan draws `sun`, `moon`, `stars` into `SceneHdr` and derives exposure
through `irradiance -> autoExposure`. CURRENT has none of the five: the plan ends at `sky`,
and `Ephemeris` (Ephemeris.h:11) computes a direction nothing draws a disc for.

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
