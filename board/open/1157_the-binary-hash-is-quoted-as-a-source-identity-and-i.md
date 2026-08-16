Type: bug
Area: harness
Tags: instrument

**The binary hash is quoted as a source identity and it is a build identity**

Two clean builds of one unchanged source produce binaries differing in **338 bytes with different
`LC_UUID`s**. So *the binary's hash appears in the measurement line* does not say **this number came from
this code**; it says *this number came from this build event*. Two measurements of one source get two
hashes, which is the opposite of what the field is quoted for — a reader comparing two runs cannot tell
*same code* from *changed code*, and that is the only question the hash was ever put there to answer.

**Three routes, and the middle one is refused.**

- **Repair the determinism.** `LC_UUID` is 16 bytes and the difference is 338, so the UUID is a symptom
  and not the cause — the ad-hoc code signature covers the UUID and moves with it, and load-command
  padding and linker stamps sit in the same region. It is a toolchain fight on a platform that signs
  every binary, with the frame budget and the picture untouched at the end of it. **Not worth its cost.**
- **Stop printing it.** Refused: a build-event identity is genuinely useful for pairing artefacts
  produced by one run, and deleting a field because its label is wrong loses a capability to fix a name.
- **Declare what it is, and print the thing it was meant to be.** Taken.

**And the thing it was meant to be already has an implementation in this tree.** `board:1120` put
`render_code_digest()` into the oracle key for exactly this reason: **a digest over the sources
themselves**, every file under a directory, no list to keep in step, so a changed source cannot produce an
unchanged key. **The measurement line wants the same digest** — over the sources the measurement was built
from — and it answers *same code or not* with no toolchain in the path. The binary hash stays beside it,
relabelled as what it is.

**The caveat, sought and cleared.** *Is a git commit id enough?* No — it is empty exactly when it matters,
because every measurement worth arguing about is taken on a working tree with uncommitted changes, and a
commit id plus a dirty flag says *something changed* without saying what. A source digest is defined on a
dirty tree and is what the preparer already relies on.

**The rule this is an instance of**, so the next one is caught by name rather than by accident: **a field's
name is part of its claim.** *The number was right and about something else* is this repository's own
front-page failure, and a hash labelled as identifying source while identifying a build event is that
failure in one word.

**Done when** the measurement line carries a digest of the sources it was built from, the binary hash is
labelled as the build event it identifies, and no claim in the tree reads the second as the first.

## MEASURED, and every pin quoted this session is void

**Two clean builds of byte-identical sources produce different digests**, and the mechanism is named:
**`ar` embeds mtimes in the archive**. So `build/liboutshine.a`'s hash identifies **when it was built**,
and this item's headline — *it is a build identity* — is now measured rather than inferred.

**And the artefact is worse than mislabelled: it is not even the thing under test.** The render suite does
not link the archive. **A hash of `liboutshine.a` was never a statement about what the failing binaries
contained**, so quoting it beside a render verdict was two errors compounded.

**SAID PLAINLY RATHER THAN QUIETLY CORRECTED: every "the binary's hash" quoted in this session is void**,
including the ones relayed to the owner as pins. They identify build events, several of them for an
archive the measurements did not run. **A practice that changes without the old numbers being withdrawn
leaves the old numbers in the record looking like evidence.**

**The replacement is what this item already argued for and now has a value.** A **source content
digest** — sha256 over the sha256 of every tracked file under `src/` and `test/` — which is
`board:1120`'s `render_code_digest()` reasoning applied to the engine instead of the preparer, and
answers *same code or not* with no toolchain in the path. First measured value:
**`1e4135cd7f0b309ff34ceeb11358512a220a13a7804275bbc2c317c4c6f6b875`**.

- [ ] **`ar -D` as well, because a deterministic archive is cheap and correctness is not traded for it** —
  but it does **not** replace the source digest: `-D` makes the archive reproducible, and the archive is
  still not what the suite links
- [ ] **The digest's population is stated with it** — *tracked files under `src/` and `test/`* — because a
  digest over an unstated set is the same defect one level down. `git ls-files` is the enumeration and it
  excludes exactly the derived artefacts that would make it unstable
- [ ] **It is published on the measurement line and the binary hash is either dropped or relabelled**,
  never both quoted as though they answered one question

## The digest is taken at run time from a tree the binary may no longer be

**Visible is not unspellable, and the developer says so plainly.** The source digest is computed **when
the run happens**, from the working tree; the binary it labels was **compiled earlier**. A tree edited in
between publishes a digest of code the binary does not contain — **and that happened once during
`board:1187`'s round.**

**It is now detectable**: `newest-source=` and `binary=` are printed on one clock, so a reader can see
that a source is newer than the binary. **Nothing prevents it**, and a reader who does not compare two
timestamps in the same line will not notice.

- [ ] **The digest belongs to the BUILD, not to the run** — computed when the binary is produced and
  carried inside it, so a measurement quotes what it was built from and cannot quote anything else.
  **That makes the mismatch unspellable rather than visible**, which is this engine's stated preference
- [ ] **Until it is, the two timestamps stay printed and are read as a pair.** A digest whose source is
  newer than its binary is **not a weaker claim, it is a wrong one**, and it should say so in a sentence
  rather than leaving the arithmetic to the reader
- [ ] **This is the same defect as the archive hash one layer in**: a value that identifies *a moment*
  presented as identifying *a thing*. The archive said *when it was built*; this says *what the tree was
  when somebody ran it*. **Both were read as *what the code is***
