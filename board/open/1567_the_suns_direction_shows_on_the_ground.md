Type: bug
Area: render
Tags: bug

**The sun's direction shows on the ground**

Round four put the sun disc in the frame and convicted the picture with it: *"a visible sun and no
consequence anywhere in the image"* -- terrain relief shows no sun-side/lee-side shading and the
bonnet's highlight does not align with the disc (`km0267.1-first`, 2026-08-22).

The caveat first: the terrain IS lit by the key light (its normals tilt with the relief and the
key is directional), so the harmless explanation is CONTRAST -- a 42-degree sun over near-white
albedo under a strong skylight flattens to nothing the eye can read. Ruled in or out by one
measurement: render one hill with the sun at 10 degrees and read the lit/lee luminance ratio; if
it exceeds the display's step and the picture still reads flat, the term is missing, not weak.

- [ ] lit/lee ratio measured on one hill at low sun
- [ ] the bonnet's specular highlight sits where the drawn sun disc is
- [ ] cast shadows remain board:0120's LightVisibility scope and are NOT claimed here

## Comments

Filed from the reviewer's fourth round, ranked first of seven new findings.

- 2026-08-25, hourly review -- the CONTRAST explanation the body offers can now be tested
  against a frame that has the relief it needs. `$TMPDIR/outshine-stills/km0114.5-framed.png`
  and `km0114.5-first.png` (fresh, 07:15) are the first stills this reviewer has seen with
  rolling terrain rather than a plane: hills rise on both sides of the corridor and the horizon
  undulates across the whole frame.

  **They are lit identically.** The left hill's slope, the right hill's slope and the flat verge
  between them carry the same sage value to the eye; nothing in the frame says where the sun is
  except the sky gradient. That is the second box above (`the bonnet's specular highlight sits
  where the drawn sun disc is`) failing in the ground's own terms, on the geometry the first box
  asked for. The lit/lee measurement the body demands should be taken at THIS station rather
  than on a synthetic hill -- the fixture exists in the stills case already.
