Type: bug
State: open
Area: base, all
Tags: measured

# The hash basis is the number it claims to be, or it stops claiming

**Benchmark** — neither Unreal nor RAGE faces this: both use their own named hash and neither
publishes a constant it did not derive. The rule that decides it is this tree's own -- *"Every number
carries its origin"* -- and a constant transcribed from a reference is only carrying its origin while
it still matches the reference.

## Measured 2026-09-02, while naming it

Five files spelled two literals for the same hash: `1099511628211ull` and
`1469598103934665603ull`, in `SubjectProxy.cpp`, `TilePool.cpp`, `BuildingMesh.cpp`, `Asset.cpp` and
`Picturing.cpp`. Naming them meant stating what they are, and stating it meant checking it.

| | value | is it the reference |
|---|---|---|
| multiplier | `1099511628211` | **yes** -- FNV-1a's 64-bit prime, and `2^40 + 2^8 + 0xb3` derives it exactly |
| basis | `1469598103934665603` | **no** -- FNV-1a's is `14695981039346656037` |

`14695981039346656037 // 10 == 1469598103934665603`. The tree's basis is the published one with its
LAST DIGIT DROPPED, which is what a transcription does and not what a decision does. It is 19 digits
where the reference has 20.

The static_assert beside the constants states both facts and goes red if either moves.

## Why this is not repaired in the commit that found it

It still hashes. Any odd 64-bit basis gives a usable FNV-1a mixer and nothing measured is wrong
because of this -- the avalanche is the prime's work. But **every picture digest, every tile cache
key and every vertex weld in this tree descends from that number**, so correcting it renumbers all
of them at once: `make shots` writes new filenames, board:2092's recorded digests stop matching, and
any comparison against an earlier run breaks.

That is a decision with a cost, and it belongs to somebody choosing it rather than to a commit that
was renaming constants.

## The choice, stated

- **Correct it.** The constant then means what its name says and a reader can check it against the
  reference. Cost: one commit that renumbers every digest in the tree and in `board/`, and it must
  say so in its own message.
- **Keep it and rename the claim.** `kDigestBasis` already does not say FNV; the assert says what it
  is not. Cost: a reader who recognises the prime will keep reaching for the reference basis.

## Done when

Either the basis matches FNV-1a and the assert says so, or a written line says why this tree keeps
its own -- and in both cases nobody has to divide by ten to find out.
