Type: bug
State: open
Parent: 1890
Area: render
Tags: instrument, picture, measured

# The tone chain publishes the numbers a reader can check it by

The reference drive declares `<key lux="40000" elevationDeg="42" bearingDeg="150"/>` -- a bright
day -- and the still is a dusk picture: a smooth olive gradient, the car barely readable, nothing
in the frame near white. Whether that is correct cannot be decided, because **the engine
publishes no photometric number at all.** `grep -icE "lux|exposure|luminance|nits|ev100"` over
every `Places(` in `Engine.cpp` returns 0. Counts of batches, tiles, normals and steps, and not
one radiometric quantity.

So the chain from a declared illuminance to a display value is unmeasurable end to end, and the
arithmetic a reader can do by hand does not settle it:

    a level surface, sun at 42 deg, albedo 0.10
    E_horizontal = 40000 * sin(42) = 26 770 lx
    L            = 26770 * 0.10 / pi = 852 cd/m^2
    ev100        = log2(40000 / 2.5) = 13.966
    exposure     = 1 / (1.2 * 2^13.966) = 5.208e-5
    x            = L * exposure = 0.0444
    filmic(x)    = x(2.51x+0.03) / (x(2.43x+0.59)+0.14) = 0.0367
    displayed    = 9 of 255 linear, or 60 of 255 if the result is sRGB-encoded

`harness/outshine/door/ScoreWhatALitSurfaceReads` measures **25.369** for that surface. It sits
between the two readings, so the arithmetic cannot even say which encoding the frame is in -- and
that case asserts only ORDERING (brighter than the next elevation down, dark with no key), never
a value against a derivation. A tone chain nobody can check by hand is one where a factor of two
lives forever.

## What will be true

- [ ] The frame publishes, per render: the scene luminance the brightest surface reached in
      cd/m^2, the exposure applied, and the display value that came out -- three numbers a hand
      computation can be compared against.
- [ ] `ScoreWhatALitSurfaceReads` asserts a VALUE against the derivation above, with its tolerance
      stated, not only an ordering. An ordering is satisfied by any monotone wrongness.
- [ ] Proving case: a level Lambertian surface of declared albedo under a declared illuminance
      reads the value the closed form gives, within a stated tolerance, at two elevations.
      Negative control: the exposure scaled by two, and the case reports the factor.
