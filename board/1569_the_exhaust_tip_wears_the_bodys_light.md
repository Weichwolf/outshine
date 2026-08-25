Type: bug
State: open
Area: render
Tags: bug, picture, driver
Supersedes: 1570

# Every part of a joined asset wears the material its file declares

Two stations of the same class, seen in every chase frame of the F31:

- a small unlit white sphere at the rear valance — one part reads as a white blob while its
  neighbours read as paint;
- the right tail light glows red and the left is dark, with no brake input in frame to explain
  one side.

Both are single parts of the joined asset resolving to the wrong surface: either the
file-material table is missing an entry and the fallback is slot 0's declared surface, or the
asset carries the emissive on one side only. A paired emissive shares one state; a one-sided
lamp is a defect on one of the two sides.

## What will be true

- [ ] The part under those pixels is NAMED with its resolved material slot, in both cases.
- [ ] A material row that resolves to a fallback is COUNTED and published, so a missing entry is
      a number rather than a picture somebody noticed.
- [ ] Looked at from behind, both lamps burn or neither does.
