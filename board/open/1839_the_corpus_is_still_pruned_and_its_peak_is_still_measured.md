Type: bug
Parent: 1789
Area: test
Tags: corpus, disk, telemetry, unmeasured

# The corpus is still pruned, and its peak is still measured

`PruneCase` exists for one stated reason -- *"to hold the 26 GB peak down"*
(`board:1789`, and the claim's own words at
`test/harness/claims/TheCorpusRefusesASecondPruner.cpp:56`). `board:1789`'s scoping made it
delete only what THIS nest prepared, and there is exactly one writer of the ownership marker:

```sh
test/run.sh:912   mkdir -p "$2" && printf '%s' "$NEST" > "$2/.prepared-by"
```

That line sits inside `RebuildCase`, which runs only when a case's prepared input has gone
missing. Every other route into `$PREPARED` writes nothing:

| how a case is prepared | writes `.prepared-by`? |
|---|---|
| `test/harness/shared/corpus/prepare.py all --manifest ...`, run offline -- the documented way | **no** |
| `prepare.py scenario-assets`, from `test/run.sh:888` | **no** |
| `RebuildCase`, `test/run.sh:909-912` | yes |

`grep -rn "prepared-by" test/ tools/ apps/ src/` returns three lines, all in `run.sh` and the
claim. So on a machine whose corpus was prepared the documented way -- which is every machine
-- `PruneCase` now returns at `test/run.sh:988` for every case, and **nothing is ever pruned**.

The consequence is a number, and the closure states none:

- the trailer's `test corpora: peak N MB, M MB after the last prune -- K cases pruned` now
  reports `K = 0` and a peak that only grows;
- `notMine` counts the cases left standing and prints a sentence, which is honest and is not a
  bound;
- the 26 GB the prune existed to hold down is now the floor, not the peak.

"A case NOBODY claims is left alone" is the right rule -- the closure argues it well. What is
missing is the other half: somebody must claim what `prepare.py` prepares, or the eviction
must move to a mechanism that does not need an owner (age, or a total-bytes budget over the
whole directory taken once at the end of a run).

## What will be true

- [x] `test/harness/shared/corpus/prepare.py` writes `.prepared-by` for every case it prepares,
      naming the nest that invoked it -- or, where it was invoked by no nest, a marker that says
      so and that a runner may act on.
- [ ] The run publishes the peak and the end size whether or not it pruned, so the trailer
      carries the disk number in every run rather than only in runs that deleted something.
- [ ] A measured before/after: the peak on this machine with the scoping in place, beside the
      26 GB the item quotes. A bound nobody measures is a hope.
- [x] Proving test: `harness/claims` asserts that a case prepared through the documented path
      carries an owner, so `PruneCase` has something to compare against. Negative control: the
      marker not written -> red, naming the case with no owner.

## Comments

- 2026-08-25 -- filed by the hourly review. `board:1789`'s requirement 1 is met and the guard
  is right. This is the cost of it, which the closure names nowhere.

**Closed on the ownership half; the disk-number half is board:1839's remainder, stated below.**

```python
test/harness/shared/corpus/prepare.py:266   def _own(destination):
test/harness/shared/corpus/prepare.py:271       nest = os.environ.get("OUTSHINE_CORPUS_OWNER", "")
test/harness/shared/corpus/prepare.py:273       marker.write(nest if nest else "no-runner")
```

called from `_prepare` (every manifest route, before any fetch) and from `_scenario_assets`
(the licensed-copy route). `test/run.sh:23` exports `OUTSHINE_CORPUS_OWNER="$NEST"` -- the SAME
identity `PruneCase` compares against, which the first version got wrong: `OUTSHINE_NEST`
carries `$BUILD` and the guard compares `$NEST`.

A case prepared by no runner writes `no-runner`, which is a fact a runner can act on. Absent,
present-and-mine, present-and-another's and present-and-nobody's are four different states now,
where the first version had three and read the fourth as the first.

Proving test: `harness/claims/APreparedCaseCarriesItsOwner` -- it RUNS `prepare.py dry-run`
against a real manifest into a temporary destination, once with the variable set and once with
`env -u`, because `_own` writes before the dry-run returns and so the claim needs no network.

**Measured cost of the change**: 27 prepared cases carried the previous preparer's digest after
`prepare.py` was edited and `EveryOracleWasPreparedByThisPreparer` turned red naming each one.
Re-prepared, 27 of 27. That claim is the tree's existing guard against a corpus that outlived
the tool that made it, and it worked.

**The remainder**, and it is named rather than closed: the trailer still publishes the peak only
in runs that pruned, and no before/after peak was measured on this machine against the 26 GB the
item quotes. That is a disk measurement this session did not take.
