Type: bug
Area: corpus
Tags: oracle, instrument

**The preparer's digest moved again and nothing hashed the products across it**

`board:1120` put `render_code_digest()` into the oracle key so that a preparer change invalidates the
corpus. It closed with a lesson recorded in its own words: the drift it was filed about was found **by
hashing before and after on a hunch, which is not an instrument**.

**The hunch has now not happened.** `board:1169` changed `in_blender_render.py`, the digest moved, **the
whole still corpus was re-prepared, and its oracle bytes were not hashed before and after.** What was
checked is that **no verdict moved** — which is strictly weaker, and weaker in exactly the direction
`1120` measured: it found **8 of 58 beauty products changed by up to 8 ulps under keys that still
matched**, with no verdict moving, and recorded that *no verdict moved was luck rather than design*.

**The expectation is byte-identity and the expectation is well founded** — the new work is gated on a
declared animation, so a still case's inputs should be untouched. **But *should be* is not *was*, and this
repository's own front page says a change that alters the picture by design cannot reproduce it to six
decimals, while a change that alters nothing must.** *Identical is a finding* has a twin: **unverified
identity is not a finding at all.**

**Why this is a bug and not a missed step.** The mechanism `1120` built is correct and works; what is
absent is the **observation** that would tell anyone whether an invalidation changed bytes. A key that
misses is a re-render; a re-render that silently produces different bytes is a corpus that moved under
every case at once, and **nothing in the tree would say so.** The instrument was named as missing when
`1120` closed and it is still missing, and it has now been exercised twice.

**What would be right instead, and it is small.** The preparer already digests every product it writes —
`_sizes(targets)` and `sha256_of_file` are in the same function. **A re-prepare publishes, per product,
whether the bytes changed across the key move**, as three counts: **unchanged · changed · new**. That is
one line per run and it turns *no verdict moved* into *no product moved*, which is the claim actually
being relied on.

- [ ] **The comparison is against the previous product, not against a stored expectation** — no baseline
  file, no second place for the truth to live. What the case directory holds before the re-prepare is the
  before
- [ ] **`changed` is not a failure.** A digest move that alters bytes is often correct — that is what the
  digest is for. It must be **visible**, and a run that reports `changed: 8` invites the question a run
  reporting nothing does not
- [ ] **The animated products are exempt by construction**, since they did not exist before; they are the
  `new` count and must not dilute the other two

**Done when** a run that moves the preparer digest publishes how many products changed bytes, an
unattended re-prepare cannot silently move the corpus, and `board:1120`'s recorded lesson has an
instrument instead of a hunch.

## AMENDED, because pruning lands first and the *before* moves with it

**`board:1181` prunes each case as it finishes, so *what the case directory holds before the re-prepare*
stops being available as the comparison's before.** The mechanism above is amended rather than the order
reversed.

**The coordinator's reading — that hashing must precede pruning because afterwards there is nothing local
to hash against — is refutable, and the refutation is the amendment.** The before survives in two places
that pruning does not touch:

- **`provenance.json` is in the keep set** and records the **preparer digest** and the **product keys** of
  the run that wrote them
- **the content store never evicts**, so the object under the *previous* key is still there when the
  digest moves and mints a new one

**So the comparison becomes: the product under the previous key, named in provenance, against the product
under the new one.** That is **stronger than what this item first proposed**, not merely compatible: a
working-tree file can be overwritten by any run between the two moments, while a keyed store object cannot
change under its own name.

- [ ] **Two conditions become load-bearing and are stated as such**: `provenance.json` survives pruning,
  and the store does not garbage-collect superseded keys. **Neither is true by accident today and both
  must stay true by decision** — a later round adding store eviction would silently remove this
  instrument
- [ ] **They are two tasks and not one.** Pruning is a runner change; this is a preparer observation. They
  touch the same products in the same run, which is why the coupling is written here rather than
  discovered
- [ ] **Pruning goes first**, on the compounding-cost argument in `board:1181`, and this item is amended
  ahead of it so that dispatching in that order cannot silently invalidate it

## The cost of the mechanism this item observes, measured for the first time

**A preparer edit invalidates every product of every case, and that cost has never been named.**
[MEASURED] on the acceptance run for `board:1181`: **94 minutes**, because the digest moved and most
renders missed.

**It recurs on every edit under `test/corpus/prep/`** — and `board:1181`, `board:1154` and this item all
touch that directory, so three of the last four corpus rounds paid it.

**The coarseness is deliberate and is not overturned here.** `board:1120` chose the whole directory over a
named list with a reason that still stands: *a named list is a second copy of a fact*, it went stale once
already, and it has to agree with the one the harness holds. **Narrowing the digest re-opens a decision
that was made correctly.**

**But the trade has never been priced, and this item is what would price it.** The question is not *is the
invalidation coarse* — it is **how many products actually changed when it fired.** If a `fixtures.py` edit
re-renders 37 cases and this item's count comes back `changed: 0`, the 94 minutes bought nothing and the
coarseness has a measurable cost with no measurable benefit. **If it comes back `changed: 8`, as
`board:1120`'s own measurement did, the invalidation is doing exactly its job.**

- [ ] **The three counts are published per digest move**, which is this item's mechanism, and **the wall
  time is published beside them** — otherwise a cost and a benefit are recorded in different places and
  nobody divides them
- [ ] **Only then is narrowing the digest a decision anybody can take**, and it stays the owner's: it
  trades a correctness property for wall time, and this repository's default is the correctness property
