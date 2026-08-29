Type: bug
State: open
Area: render
Tags: picture

# The road stands twice and one copy is in the sky

**Benchmark** — Unreal and RAGE both draw a road surface once, from one instance. **Neither has this defect** — it is a bug rather than a design question.

`the driver client (deleted) --headless --offline --frames 400 --stills 4` writes stills in which one surface is
drawn TWICE, mirrored about the horizon.

Measured on `along01.png` (1280x720), pixel colours read from the file:

    the shape below the horizon   RGB (23, 44, 83)
    the shape above the horizon   RGB (23, 44, 83)
    grass                         RGB (69, 84, 71)
    sky at that height            RGB (78, 118, 159)

The two are BIT-IDENTICAL in colour, which rules a reflection out -- a reflection carries the
water's tint and its own attenuation, and would not land on the same three bytes. The silhouettes
mirror each other about the horizon line. So it is one surface, submitted twice, with the vertical
component of one copy negated.

(23, 44, 83) is also not asphalt: the surface below the horizon is a saturated dark blue where a
road should be grey, so there may be two defects here or one.

**NOT INTRODUCED BY THE TARGET REFACTOR.** The same command at `132f07d9^` -- before this
session's first commit -- writes a file that is BYTE-IDENTICAL to the current one. That is also
the strongest single statement available about the refactor: `SubjectProxy`, `View`, `Overlay`,
`Asset`, the placement delta, the namespace move and the deletion of 1052 lines left the client's
frame exactly where it was.

Waits on the refactor (board:1953).

- [ ] the surface is drawn once
- [ ] the road reads as a road
