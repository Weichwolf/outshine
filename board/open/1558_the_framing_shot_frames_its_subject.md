Type: bug
Area: clients
Tags: bug

**The framing shot frames its subject**

Four framing stills across 700 km are interchangeable: the terrain tile fills a fifth of the frame,
97.7 % of the pixels are unwritten, the car is a black blob, and the road leaves the tile as a
one-pixel hairline into blank space. A framing shot is composed -- subject filling the frame, camera at
a chosen distance, ground continuous under everything drawn.

Three separate defects share these pictures:

- [ ] **the camera**: frame the subject from its bounds (the engine already computes `FrameItself`),
      not from a fixed distance that shrinks the world to a stamp
- [ ] **the ground under the road**: the shown corridor reaches `kShownM` = 900 m while the sampled
      ground reaches `kGroundReachM` = 400 m, so the road runs off its tile as a hairline. One reach,
      derived from the other, so the mistake becomes unspellable
- [ ] **the unwritten surround**: alpha 0 in a published PNG is the absence of a sky, which is
      board:0120's chain; until the sky stage lands, the screenshot writes its declared clear colour
      so a still is never 97 % undefined bytes

## Comments

Found by the magazine-reviewer round, ranked eleventh by damage but named the cheapest fix on the
list. The reach mismatch (900 shown vs 400 sampled) was measurable in the tool's own constants once
the picture pointed at it.
