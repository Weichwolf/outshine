Type: bug
State: open
Area: test
Tags: corpora, measured, provenance

# The corpus is made by ONE preparer, and the guard that says so runs

`EveryOracleWasPreparedByThisPreparer` hashes every source under the corpus harness and compares
it against the `preparerDigest` each case recorded when it was prepared. It did not COMPILE from
the cut until 9de36ce7 -- `LayerIncludes harness/claims` had lost `-Isrc/core` while `LayerGroups`
still linked `Sha256.cpp` beside it -- so for that whole stretch it went green in every summary
by not running at all.

Running, it finds this, measured 2026-08-25 over the prepared tree:

| cases | preparerDigest |
|---|---|
| 813 | `c2eea660e026` |
| 263 | `48fb1dc07303` |
| 162 | `944911f2d785` |
| 150 | `6ceaa1545b60` |
| 34 | `70175e4aad7f` |
| 21 | `d94bead1a69d` |

**Six tool versions made one corpus.** 1160 of 1425 checks fail. This is not a threshold that
drifted: the digest covers the preparer's own code precisely so that changing it invalidates
what it made, and six of them means six different answers to "what would this case look like if
it were prepared today".

The 263 at `48fb1dc0` are the glTF-Validator cases prepared this hour; they were prepared before
`prep/fetching.py` was factored out and are already one behind. That is the guard working.

## What will be true

- [ ] Every prepared case records ONE digest and it is the tree's. The cheap trees -- wpt,
      test262 and refuse/gltf, 1238 cases whose preparation is a fetch -- can be reprepared in
      minutes. The 185 Khronos and 34 generator cases carry Cycles oracles and cost a Blender
      render each, so they are the expensive half.
- [ ] The claim is GREEN, or its count stands in `EXPECT_FAIL` with the number of cases still
      carrying an older digest, so the day a case is reprepared the gate notices.
- [ ] A run that reprepares reports which digest it moved cases FROM, so this table can be
      re-taken rather than re-derived.
