Type: bug
Area: corpus
Tags: perf, oracle, instrument

**The preparer materialises the whole corpus, so the peak is unbounded by the runner's prune**

**The owner's rule is *so size doesn't grow*, and growth is about the PEAK.** `board:1181` bounds the
**end state** — [MEASURED] `test/render/` **4821 MB → 139 MB**, 893 files declined per run — and it cannot
bound the peak, because **`prepare.py` materialises every case before the runner reaches the first one.**
**The measured peak of that same run was 3212 MB.**

**The end state scales with the number of cases; the peak scales with the corpus.** At the projected 147
in-scope cases the end state is ≈0.4 GB and **the peak is ~12.8 GB**, against 53 GiB free — so the
constraint the prune was built for returns at scale through a door the prune does not cover.

**The asymmetry is what makes this a defect rather than a limitation.** The runner already proves, per
case, that a product's bytes are recoverable — key present, size and digest equal — and it declines them
one case at a time. **The preparer has the same knowledge earlier and uses it to place everything at
once.** Nothing needs a second mechanism; what is missing is that placement is whole-corpus where
consumption is per case.

- [ ] **Placement becomes per case, driven by the consumer.** A case is materialised when the runner is
  about to run it and declined when it finishes, so **at most one case is live** and the peak equals the
  largest case rather than the corpus. `prepare.py` is already **idempotent and independently invocable**
  per manifest — [MEASURED] **one case 0.77 s from a warm store** against **14.3–17.6 s for all 37** — so
  the machinery exists and only the granularity is wrong
- [ ] **`prepare.py all` over the whole corpus stays**, because a cold build needs it and because Cycles
  work must be batched. **The peak claim is about a RUN, not about a build** — and the two must be
  distinguishable, or a green *size doesn't grow* would only mean *nobody rebuilt today*
- [ ] **The peak is published by the runner as a high-water mark**, which `board:1181` already requires
  and which is what makes this checkable rather than asserted. **A peak nobody records is how this defect
  survived being designed against**
- [ ] **`prepare.py all` requires Blender present even on a warm store**, [MEASURED], so a machine without
  it cannot re-materialise a pruned case at all. **Per-case placement must not inherit that** — a hit
  should need the store and nothing else, or pruning has traded disk for a toolchain dependency

**The caveat, sought and cleared.** *Is the peak simply the price of a cold build?* No — the 3212 MB peak
was measured on a run whose oracle products were largely cached. The corpus was materialised because that
is what the preparer does, not because it had work to do.

**Done when** a run's peak is one case rather than the corpus, the runner publishes the high-water mark
that proves it, and a warm-store re-materialisation of one case needs the store alone.
