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
