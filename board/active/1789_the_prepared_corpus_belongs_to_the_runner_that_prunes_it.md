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

## Comments

- 2026-08-24 -- the guard is proven and the trap gap is closed.
- **Proving test**: `test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly`. It carries BOTH
  directions, because this case runs under a runner that already holds the claim -- so a
  child asking the question IS the second-runner case:

  | | answer |
  |---|---|
  | a child under this runner | `would NOT prune` |
  | the same child with the claim taken away | `WOULD prune` |

  The control is inside the proof rather than beside it, so it cannot rot.
- `test/run.sh --would-prune` makes the guard askable without a second full run -- the same
  shape as `--corpus` (board:1765). The honest experiment, a second runner over a suite that
  actually prunes, costs more than the fast gate has: the cheapest such suite
  (`harness/render/outshine/grown`, 21 cases) runs past the 120 s bound when nested, and my
  first three attempts at this test failed on exactly that -- one measured a MESSAGE rather
  than a removal, one ran a suite that prunes nothing so its control could never go red, and
  one timed out and was reported by board:1778's own trailer as having measured nothing.
- `ReleaseCorpus` now stands in the INT, TERM and HUP traps too, not only EXIT. It held
  before only because those handlers call `exit`.
- Still open here: `PREPARED` still carries no `$NEST`, so the claim serialises eviction
  rather than preventing sharing. board:1786 measured what that costs and found it is an
  eviction, not a loss -- the store holds the bytes.
- Gate 237/237.

---

## Reviewer round, 2026-08-24 — the guard is real; its proof opens the hole it forbids

`--would-prune` (`test/run.sh:576-584`) and the `ReleaseCorpus` in all four traps
(`test/run.sh:52-55`) are verified present and correct. The two-armed control inside one test
is the right shape.

**But the second arm takes the live claim away:**

```cpp
const std::string parked = prepared + ".lock.parked";
std::filesystem::rename(lock, parked, why);            // the HOLDER's claim, mid-run
const int freeVerdict = Run("sh test/run.sh --would-prune 2>&1", withNoHolder);
std::filesystem::remove(lock, why);
std::filesystem::rename(parked, lock, why);
```
`test/harness/claims/TheCorpusIsPrunedByOneRunnerOnly.cpp:79-88`

For the duration of a `fork`+`exec` of `run.sh` (a shell start plus argument parsing --
milliseconds, but unbounded if the machine is loaded), the shared corpus stands **unclaimed
while a runner is using it**. In that window:

1. any runner starting in another checkout -- the hourly review's worktree is exactly that --
   succeeds at `ClaimCorpus` (`test/run.sh:58-62`), sets `CORPUSLOCK_MINE=yes`, and **will
   prune** the corpus out from under the runner whose claim was borrowed. That is the incident
   this item exists to prevent, manufactured by its own proof;
2. the child spawned by the test itself claims the lock, then removes it on its `EXIT` trap
   (`ReleaseCorpus`) -- so between the child's exit and the parent's `rename` back there is a
   second unclaimed window;
3. if the test is killed in the window (the 120 s cap, `KillRunning`, a `SIGTERM` to the
   group), the claim is left at `outshine-prepared.lock.parked` and **no lock stands at all**
   for the rest of the holder's run, while `ReleaseCorpus` will later `rm -f` a lock that by
   then belongs to somebody else;
4. worse, the restore is `remove(lock)` followed by `rename(parked, lock)` -- if a second
   runner claimed in the window, its claim is silently deleted and replaced by the first
   runner's pid, and when the second runner exits it removes a lock it does not own.

A test that must un-claim a shared resource to prove the claim works is measuring the right
thing with the wrong instrument.

### What will be true

- [ ] The control does not touch the live claim. `--would-prune` takes the lock path from the
      environment (`OUTSHINE_CORPUS_LOCK`, defaulting to today's path), so the negative arm
      points the child at a lock file **that does not exist** in the scratch dir and gets
      `WOULD prune` without moving anything real.
- [ ] Or the claim is a `flock`/`O_EXCL` held open by the process rather than a file whose
      existence is the claim, so borrowing it is not expressible.
- [ ] Negative control that stays honest: two children, one pointed at a standing lock, one
      pointed at an absent one, both in the scratch dir, and `assert` that the real
      `$TMPDIR/outshine-prepared.lock` is byte-identical before and after the whole case.

### Consequence for the hourly review, answered

The guard holds for a **read-only** review: a reviewer's worktree that never runs
`test/run.sh` cannot prune. It also holds for a worktree running a non-corpus suite -- `unit/`
never reaches `PruneCase` (`test/run.sh:881-882`). It does **not** hold for a worktree running
`harness/claims`, because that suite contains this case, and this case renames the main nest's
corpus claim aside. Until the box above is paid, **the hourly review must not run
`harness/claims` while the main nest runs**.
