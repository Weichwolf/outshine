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

## Measured once the numbers were published, and the suspicion is DEAD

    the exposure the picture applied                5.20833e-05
    the brightest the scene's linear buffer reached 3326
    the brightest the presented frame shows         138 of 255

Against the hand computation, end to end:

    exposure   1 / (1.2 * 2^13.966)         = 5.208e-05     agrees exactly
    x          3326 * 5.208e-05             = 0.1732
    filmic(x)  x(2.51x+0.03)/(x(2.43x+0.59)+0.14) = 0.2555
    sRGB       0.2555^(1/2.2) * 255         = 137.3         measured 138

**One count.** The tone chain is correct and the frame is sRGB-encoded, which the arithmetic
alone could not decide before these three numbers existed.

So the still is dark because the WORLD is dark, not because the chain is: the brightest thing in
the frame reaches 3326 cd/m^2 and the ground's albedo is 0.10, which under a filmic curve is a
dim olive whatever the sun does. That is a content finding and it belongs to board:1890's surface
box, not here.

## What will be true

- [x] The frame publishes, per render: the brightest linear value the scene reached, the exposure
      applied, and the brightest display value that came out -- three numbers a hand computation
      can be compared against, and it agrees to one count.
- [ ] `ScoreWhatALitSurfaceReads` asserts a VALUE against the derivation above, with its tolerance
      stated, not only an ordering. An ordering is satisfied by any monotone wrongness.
- [ ] Proving case: a level Lambertian surface of declared albedo under a declared illuminance
      reads the value the closed form gives, within a stated tolerance, at two elevations.
      Negative control: the exposure scaled by two, and the case reports the factor.
