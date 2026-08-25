Type: bug
State: open
Area: render
Tags: bug

**The exhaust tip wears the body's light**

Round four: *"a small unlit white sphere floating at the rear valance, present in every chase
frame"* (`km0017.3-third` et al.). One part of the joined F31 reads as a white blob while its
neighbours read as paint -- either its material row resolved to a default (the file-material table
missing an entry falls back to slot 0's declared surface), or its normals are degenerate in the
asset and the shading saturates.

- [ ] name the part (dump the part under those pixels) and its resolved material slot
- [ ] the tip draws with the body's material response

## Comments

Filed from the reviewer's fourth round, ranked sixth; small in pixels, loud in every chase frame.
