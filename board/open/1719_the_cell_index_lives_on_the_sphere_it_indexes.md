Type: bug
Area: actor
Tags: correctness, worldwide

**The cell index lives on the sphere it indexes**

`Network::CellOf` (src/actor/path/Wayfinding.cpp:62-68) hashes `floor(lonDeg / lonCell)` with
no wrap at ±180°:

- A way crossing the antimeridian (179.99° → −179.99°) puts its two ends in columns half a
  billion apart; the Weave node merge (lines 146-160) and `Within`'s box walk never see them
  as neighbours, so ways that meet at the date line weave as a network in pieces. `ApartM`
  wraps (lines 31-33); the index does not. board/open/1524 names the antimeridian as a route
  class this code must survive.
- The column modulus `lonCell` is evaluated at a CONTINUOUS latitude, not the row's: two
  points in the same row hash with slightly different `lonCell`, and the resulting index
  shift grows with longitude — bounded by `lon_rad * sin(lat)`, i.e. up to ~π cells near
  |lon| = 180° at high latitude — against a slack of exactly one ring
  (Wayfinding.cpp:243, "+ 1"). The merge and the box walk can miss true neighbours where
  the shift exceeds the slack.
- `Plan`'s turn geometry (lines 350-360) takes `here.LonDeg - was.LonDeg` raw: an edge
  across the antimeridian yields a ~360° pseudo-heading and the turn refusal judges garbage.

Demanded: the cell key quantises longitude by a PER-ROW modulus (the row's own latitude, so
every point in a row shares one `lonCell`) and wraps the column count at the row's
circumference; the lon deltas in Plan's turn test wrap like ApartM's; a unit arm weaves two
ways meeting at (60°, ±179.9999°) and proves the merge and a route across the line.
