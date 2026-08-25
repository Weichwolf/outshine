Type: issue
State: open
Area: generators
Tags: determinism, content

# The forest's randomness is stable under a new stream and a new species

A region's wood must be a pure function of `(zoom, x, y)` — that is what makes it a place a
player can return to and a picture an oracle can bound. At HEAD it is a pure function of
`(zoom, x, y, kStreamsPerCell, Stems_.size())`, and the last two move whenever the code or the
asset directory grows: `src/generators/Forest.cpp` indexes per-cell entropy by a STRIDE
MULTIPLY (`index * kStreamsPerCell + n`), so raising the stride from 3 to 4 moved every jitter,
every density draw and every trunk size in every region on Earth — and the commit that did it
reported the gain and not the cost.

## What will be true

- [ ] A cell's streams are addressed by a NAME, not by an offset into a stride, so adding a
      stream moves nothing that already existed.
- [ ] A species added to the directory does not renumber the ones before it.
- [ ] Proving test: a region's stems are compared before and after a stream is added and a
      species is appended, and only the asked-for change appears.
