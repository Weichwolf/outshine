Type: bug
State: open
Area: engine, world
Tags: measured, benchmark

# the sky stands where PLACE and TIME put it, and today it stands where a number says

**Benchmark** — Unreal: a `DirectionalLight` under Sun Position Calculator takes latitude,
longitude and a date-time and derives the sun; the artist sets the CLOCK, not the angle. RAGE: the
time of day drives the sun and the moon over a real day-night cycle, and a mission sets the hour.
**Both agree** — the angle is DERIVED from place and time and is never hand-set, because a hand-set
sun is a sun that disagrees with its own shadows the moment the clock moves.

## The whole chain is written and no declaration reaches any of it

    src/world/sky/Ephemeris.h    SolarAt(lat, lon, utc) -> sun elevation and azimuth,
                                 moon elevation, azimuth and PHASE
    src/world/sky/CivilTime.h    ParseIsoUtc(text, unixS)
    include/Scenario.h           struct Clock { bool Declared; std::string Start; double Rate; }

**`grep -rln 'Ephemeris.h' src/` finds nothing.** The only file in the tree that names it is
`test/CORPORA.md`. **`grep -rn 'Time.Start' src/`** finds exactly one line -- the scenario reader
storing it -- and no reader of the stored value.

What a scenario declares instead:

    struct Light { double Lux; double ElevationDeg; double BearingDeg; };

A hand-set angle. So `Scenario::Time` is accepted and does nothing, which is the failure mode
CLAUDE.md names by hand: *accepting a declaration and doing nothing with it is worse than refusing
it*.

## Why it is not cosmetic

The engine's own aim is that a picture can be made of ANY place on Earth and be comparable with
reality. A sun that does not follow from the place and the hour cannot be compared with anything --
the shadow falls where a number said, not where the sun is. The moon and its PHASE are computed by
the same call and are not asked for at all, and the stars need the same two inputs.

- [ ] a declared clock and a declared place derive the key light's elevation and bearing
- [ ] the moon's elevation, azimuth and phase reach the picture from the same call
- [ ] a scenario that declares a clock and hand-sets an angle is REFUSED, because only one of the
      two can be true
- [ ] the five places in `test/outshine/places/` declare a clock rather than an angle

**The measurement that would show I am wrong**: `outshine/door/ScoreWhereTheSunStands` passes today
and tests the sun's RADIANCE in its own direction, not its position from a time. If wiring the
ephemeris moves that case, the direction it currently tests was wrong and the case is the guard.
GeographicLib's corpus is already in the tree for geodesy; the sun's position for a named place and
instant is checkable against any published almanac, which is what turns this from plausible to
proven.
