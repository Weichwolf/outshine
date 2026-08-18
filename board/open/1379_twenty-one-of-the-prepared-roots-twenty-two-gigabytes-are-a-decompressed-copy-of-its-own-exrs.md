Type: bug
Area: harness
Tags: instrument, oracle

**Twenty-one of the prepared root's twenty-two gigabytes are a decompressed copy of its own EXRs**

[MEASURED] over the prepared root after a full preparation of 151 cases:

| class | on disk |
|---|---|
| `.raw` | **21.0 GB** |
| `.exr` | 182 MB |
| `.png` | 255 MB |
| everything else | ~600 MB |

**The raws carry nothing the EXRs do not.** `board:1119` closed with a C++ EXR decoder and
`RawF32::ReadExrFile` already fills the same object from the same file; its own comment states the
raw is *a 16x decompressed cache of the EXR*. What did not happen in that round is the retirement:
the preparer still writes both, and 120x the bytes are the cost of that.

**It matters because disk is the scarce resource here and CPU is not.** The owner's standing decision
is that Cycles renders are NOT cached because there is more CPU than disk -- and this is 21 GB of a
quantity that is recomputed from a file already on the same disk.

## The decision this item has to take, and it is not obvious

**Retiring the raw retires its own proof.** `test/outshine/harness/TheOraclesExrReadsAsItsRaw.cpp`
holds the decoder to the flat dump *bit-for-bit over every prepared case*, and that population exists
only because both files are written. Deleting one side leaves the test comparing a thing to nothing.

Three ways out, and the third is recommended:

| | |
|---|---|
| keep both for a declared subset | the population shrinks to whatever the subset is, and *population too small* is one of the four faces of the wrong-domain failure `CLAUDE.md` names. It also leaves the rule "written for some cases and not others", which is a convention rather than a shape |
| keep both and accept 21 GB | pays the whole cost to keep a proof of something the corpus does not need proven per case |
| **prove the DECODER instead of the corpus** | the claim is *our EXR decoder is correct*, and the instrument for a decoder is inputs whose answer is known independently -- a synthesised EXR with named samples, at each compression the oracle can emit. **That is a unit test with no corpus, no 21 GB and a population that does not move when the case list does** |

**The third is what the split by instrument already says.** *What would fail this test? the code
computed the wrong thing* -> unit. A corpus-wide bit comparison was the right instrument while there
was no other; once the decoder can be exercised directly, agreeing with a dump of itself over 151
cases is 151 samples of one question.

## What must be true

- [ ] **A unit test exercises the EXR decoder against synthesised inputs** whose samples are known
  independently, at every compression the recipe can declare -- `ZIP` today, and the test names which
- [ ] **The preparer stops writing `oracle*.raw`** and every consumer reads the EXR
- [ ] **`TheOraclesExrReadsAsItsRaw` is deleted in the same round its subject is** -- a test whose
  population is gone is a dead path, and a dead path that can still fire is worse than one line too many
- [ ] **The prepared root is re-measured** and the number is quoted with the case count it was taken over
