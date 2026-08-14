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
