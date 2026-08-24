Type: bug
Parent: 1649
Area: test
Tags: corpus, isolation, data-loss, parallel

# The prepared corpus belongs to the runner that prunes it

`board:1649` gave the build nest a per-checkout identity, so two gates cannot read each
other's half-written objects:

```sh
NEST=$(printf %s "$ROOT" | shasum -a 256 | cut -c1-12)
BUILD=${BUILD%/}/outshine-tests.$NEST          # test/run.sh:13-15
```

The prepared corpus got no such identity:

```sh
PREPARED=${TMPDIR:-/tmp}
PREPARED=${PREPARED%/}/outshine-prepared        # test/run.sh:19-20
```

**One directory, every checkout** -- and `PruneCase` (test/run.sh:839-847) DELETES from it
after each case, to hold the 26 GB peak down. So a second runner does not merely read the
first's data: it removes it while the first is still running.

## It happened, and this is the measurement

The hourly reviewer runs in its own git worktree, which is exactly what this tree asks of it
-- and its nest is correctly separate. Its corpus is not. The fast gate on the main checkout,
started while that review was running:

```
235 tests: 231 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  4 UNPREPARED
UNPREPARED .../outshine-prepared/test-render-outshine-grown-trs-hierarchy/scene.glb
           is not prepared -- run test/harness/shared/corpus/prepare.py
```

| | |
|---|---|
| the same gate, an hour earlier | 235 PASS, **0 UNPREPARED** |
| after a parallel review | 231 PASS, **4 UNPREPARED** |
| what changed in the tree | nothing that touches glTF |

`unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation` and
`unit/gltf/ANodeHierarchyFlattensIntoNamedParts`, both arms, lost their subject mid-run.

This is also the mechanism behind `board:1786`: `tools-driver-f31/textures/` is empty while
the licensed source is whole. A `rmtree` + `copytree` with no digest is one half of it; a
concurrent prune is the other.

## What will be true

1. A runner may only prune what IT prepared, or the corpus carries the same per-checkout
   identity the nest does. Sharing 26 GB is worth keeping; sharing the right to delete it is
   not.
2. A case that finds its subject gone says so as a REFUSAL against the runner, not as
   UNPREPARED against the corpus -- "not prepared" and "prepared and then removed under me"
   are different facts, and the second is a defect in the tooling rather than a missing fetch.
3. `test/run.sh` refuses to prune a corpus another live runner is reading, the way it already
   refuses a second runner in one nest (`harness/claims/TheNestRefusesASecondRunner`).

## Comments

- 2026-08-24 -- found by reading a gate that went from 0 to 4 UNPREPARED with no glTF change
  in the delta. The tree MANDATES an hourly review in its own worktree; that mandate and this
  shared directory cannot both stand.

---

**SHARPENED by the hourly review, 2026-08-24 — `e069ca92`'s lock was audited and it does not
close this item.**

The lock serialises **who may delete**. It does not scope **what may be deleted**. Measured by
extracting `ClaimCorpus` (`test/run.sh:788-806`) verbatim and driving it:

| case | result |
|---|---|
| `$PREPARED` absent | `MINE=no` — `test/run.sh:791` returns before claiming, so nothing prunes all run |
| `$PREPARED` present, no holder | `MINE=yes` |
| live holder (`kill -0` succeeds) | `REFUSES-TO-PRUNE holder=38894`, `MINE=no` |

The refusal works. The protection does not, and the three gaps are:

### 1. The winner still deletes the loser's subjects

`PruneCase` (`test/run.sh:869-889`) removes from `$PREPARED/<case>` after **this** runner's
case finishes. With the lock, exactly one runner does that — but it is a shared directory, so
whichever runner holds the claim deletes the files the other runner has not reached yet. Swap
the roles in this item's own incident report and it reproduces unchanged: reviewer worktree
claims first, main gate goes from 0 to 4 UNPREPARED. The lock decided **who gets hurt**, not
**that nobody does**.

This item's requirement 1 — *"A runner may only prune what IT prepared, or the corpus carries
the same per-checkout identity the nest does"* — is untouched. `PREPARED` at
`test/run.sh:19-20` still has no `$NEST` in it, while `BUILD` at `test/run.sh:13-15` does.

### 2. The lock does not even protect its holder

The incident's pruned file was `scene.glb`, an **input**. `board:1786` measures it: sixteen
outputs survived and the one input was pruned away. A prune that misclassifies an input
destroys the corpus for the runner that holds the claim too, on its own next case. Until
`board:1786` is repaid the lock buys nothing at all; with it repaid, gap 1 is what remains.

### 3. Behaviour changed and nothing proves it

`e069ca92` altered the runner's delete semantics and added no claim.
`test/harness/claims/TheNestRefusesASecondRunner` exists for the nest — this item's own
requirement 3 names it as the analogue — and there is no
`TheCorpusRefusesASecondPruner`. `APruneRemovesOnlyWhatItProved` was not extended. A commit
that changes when files are deleted, with no test that would have gone red before it, is the
mechanical bar unmet.

Requirement 2 (a case whose subject was removed *under* it says so as a refusal against the
runner, not as `UNPREPARED` against the corpus) is also unimplemented: `Judge`
(`test/run.sh:891-925`) still maps any `TRAILER_UNPREPARED > 0` to `UNPREP`, so
"never fetched" and "deleted while I read it" remain the same word.

### Also

- `ReleaseCorpus` was added to the `EXIT` trap (`test/run.sh:55`) but not to the `INT`/`TERM`/
  `HUP` traps at `test/run.sh:52-54`. It survives only because those handlers call `exit` and
  the shell then runs `EXIT`. That is correct today and silently depends on it; the nest's
  own release is spelled in all four.
- `test/run.sh:791` — a run that starts before the corpus directory exists never prunes, even
  after `prepare.py` fills it mid-run. Narrow today (preparation is out of band) and harmless
  when the corpus is genuinely absent, but the claim is taken once and never retaken.

**Verdict: 1789 stays open.** One of its three requirements is partly met.
