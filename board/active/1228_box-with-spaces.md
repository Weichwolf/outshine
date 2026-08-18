Type: feature
Area: corpus
Tags: khronos, core

**Box With Spaces has no case, and that is the whole of it**

*Box With Spaces* -- tagged `core`, `testing` at the pin, published as `glTF`.

**No case declares this model.** It is one of the **118 of 148** in that state, and that population is
the dominant distance between this tree and the finish line `board:1171` sets -- larger than every red
case and every unimplemented extension put together.

**A case is a directory and the `.gltf` is the declaration**, so adding one is not a writing task. It is
worth adding only where a render can prove something, which is what `board:0079`'s ordered sequence
decides. *This item is the inventory entry for one model; it is not a dispatch and it schedules nothing.*

## Comments

**The model had a manifest and still had no case, and the harness is where it was lost.** `test/run.sh`
iterated its case list with an unquoted `for oneCase in $cases`, so this directory word-split into
`test/khronos/glTF/Box`, `With` and `Spaces` -- three paths that do not exist. They were reported as
**UNPREPARED**, which reads as *the preparer has not run yet* rather than as *this case is not real*,
so the run said 15 UNPREPARED and nobody could tell one of them was a case that could never execute.
**The dangerous direction again: a missing thing reported as a pending one.** Fixed by splitting on
newline, which is what the enumeration actually produces.

**Two authoring defects of my own, both from the same root -- a URI is not a filename.** The helper
that reads the pin fetched `metadata.json` fine and then 404ed, because this file MIXES the two
spellings deliberately: `Box With Spaces.bin` carries a raw space and its three images are
percent-encoded (`Normal%20Map.png`). Decoding before touching the filesystem is a reader's
obligation and `unquote` of an unencoded name is the identity that lets one rule serve both.
**src/gltf/Document.cpp:122 already did this correctly and nothing exercised it** -- which is the
whole value of this case.
