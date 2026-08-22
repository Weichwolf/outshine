Type: bug
Area: test
Tags: instrument

**The prepared corpora carry the digest of the preparer that stands**

`harness/claims/EveryOracleWasPreparedByThisPreparer` is red at HEAD: 1178 prepared cases under
`test/render/khronos/generator` (and siblings) record `preparerDigest`
`94c91b42...` while the preparer tree digests to `c59943365...`. The preparer's `.py` tree last
changed in `0fea65ef` (2026-08-21, board:1543) and the corpora were never re-prepared or
re-stamped, so the provenance chain -- the claim that THIS preparer can reproduce THIS oracle --
is broken for every case prepared before that commit.

Pre-existing at HEAD, found while proving board:1584 (the red is in every `harness/claims` run's
trailer and predates the build unification). Done when the one offline script
(`test/harness/shared/corpus/prepare.py`) has re-prepared or honestly re-stamped the affected
cases and the claim is green -- re-stamping is only honest if the preparer change provably does
not alter the prepared bytes; otherwise re-prepare.

---

**Closed.** The whole corpus was re-prepared offline by the standing preparer (`prepare.py
fetch/patch --every-case`, render products reattached from the content store), so every case's
provenance names the tree that can reproduce it. The bulk restore itself taught a lesson the
hard way: fetch/patch overwrite render-enriched metadata, so the index-pass claim went red until
`render --every-case` reattached from cache -- the prune/restore lifecycle owns all of today's
phantom reds. Proving tests: `harness/claims` 11 of 11, including
EveryOracleWasPreparedByThisPreparer and EveryRenderNamesItsIndices.
