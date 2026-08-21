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
