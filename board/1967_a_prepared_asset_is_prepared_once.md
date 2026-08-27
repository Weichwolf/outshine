Type: bug
State: open
Area: harness

# A prepared asset is prepared once

**Benchmark** — Unreal: DDC — an asset is cooked once and the result is keyed and shared. RAGE: the tool chain cooks once. **Both agree** — preparation is not frame work and not per-run work.

`test/run.sh` prepares each corpus case into the system temp directory, and it prepares the SAME
asset once per suite that names it. Measured while clearing the disk:

    2873 prepared directories, 50 GB
    test-khronos-glTF-CommercialRefrigerator        437 MB
    test-render-khronos-glTF-CommercialRefrigerator 437 MB   the same bytes, again

Every Khronos asset that both the `khronos/glTF` suite and the `render/khronos/glTF` suite touch is
unpacked twice under two names. That is half of the 50 GB, and it grows with every suite that
reaches the same corpus.

The prepared form is a pure function of (case, corpus revision), so it is content-addressable
exactly the way the content store already is -- hash the input, name the directory by the hash, and
let both suites reach the one copy. `test/CORPORA.md` already pins each corpus by URL and hash, so
the input half of that key exists.

Not urgent and not silent: it costs 25 GB of a developer's disk and nothing else, and a `make
clean`-equivalent recovers it. Filed so the next person who wonders where the disk went finds the
answer rather than the symptom.

- [ ] a prepared case is named by the hash of what produced it
- [ ] two suites reaching one corpus case unpack it once, proven by a count over the prepared tree
