Type: bug
Area: test
Tags: process

# The corpus rebuilds itself when the temp dir loses it

The corpus lives in the system temp dir -- `$TMPDIR/outshine-prepared` beside the content store
`$TMPDIR/outshine-content` -- because `CLAUDE.md` puts artefacts there and never in the tree.
The temp dir is swept by the machine. Tonight it took both between two gate runs, and the tree
had no way back:

```
239 tests: 239 PASS ... 0 UNPREPARED        (before)
24 tests: 21 PASS ... 3 UNPREPARED          (after, corpus gone)
```

`run.sh` names the cure and does not apply it (`test/run.sh:1087`):

```sh
printf 'run.sh: %s has no prepared input -- run test/harness/shared/corpus/prepare.py\n'
```

And the named cure does not work as one command:

```
$ python3 test/harness/shared/corpus/prepare.py all --every-case
AttributeError: 'NoneType' object has no attribute 'animation'
  prep/manifest.py:90  in frame_grid
  prep/jobs.py:59      in plan
```

`frame_grid()` reads `self.scene.animation`, and the test262 and wpt manifests declare no
`scene` at all -- they are fetched sources, not Blender renders. **The only way to build the
corpus crashes on the majority of the corpus.** The per-family subcommands (`wpt-cases`,
`test262-cases`, `generator-cases`, `scenario-assets`) each work; the one that means "all of
it" does not.

Two defects, one consequence: a machine that sweeps its temp dir leaves the tree unable to
judge itself, and the trailer's `UNPREPARED` count is the only trace.

## Why this is not "the preparer is offline by design"

It is offline by design and stays so. What must not happen is a GATE that silently loses a
third of its subjects. The two are reconciled by ANNOUNCING the rebuild and standing its cost
BESIDE the bound, exactly as the builds already do:

```
run.sh: gate headroom 136363 ms of 230000 (run 93637 ms, builds 71472 ms beside the bound)
```

A rebuild is a build. It is not part of the timed run, it is reported, and the run that follows
it judges a complete corpus.

## What will be true

- [x] `prepare.py all --every-case` plans and runs over every manifest in the tree, including
      those that declare no scene -- a case with no scene renders nothing, which is a fact
      about the case and not an error.
- [x] `run.sh` finding no corpus on disk rebuilds it before the timed run, says so on one line,
      and reports what the rebuild cost beside the bound.
- [x] A run whose corpus is already present does NOT rebuild and does not touch the network.
- [x] Proving test: the preparer's dry-run over every case returns a plan rather than a
      traceback. Negative control: the guard removed -> the `AttributeError` above.

## Comments

- 2026-08-24 -- repaid, and the shape changed under measurement.

  | what I first wrote | what the measurement said |
  |---|---|---|
  | `run.sh` rebuilds the whole corpus when it is absent | `all --every-case` wrote **5.2 GB in minutes** and was heading for hundreds -- 256 MB for a single animation case. That is the sporadic proof's corpus, not this run's. |
  | one rebuild per case, from the case's own manifest | a case can consume ANOTHER case's product: the glTF twins read `test-render-outshine-grown-trs-hierarchy/scene.glb` |

- **The mechanism that survived.** `RebuildCase` fills a case's own prepared directory from its
  own manifest; `RebuildOwner` reads the prepared PATHS a failing case names in its log,
  inverts the prepared-directory mapping (`tr / -`) onto the manifests the tree declares, and
  rebuilds every owner it finds. `Judge` then re-runs the case once per successful round, which
  terminates because `RebuildCase` declines a directory that is already filled.
- Two things the walk had to be taught, both found by running it:
  - `Box With Spaces` -- an unquoted `for` over `find` split the name. The map is built once,
    tab-separated, and read with `IFS=` and `awk`.
  - BSD `sed` has no `\|` alternation in a basic regex, so `UNPREPARED` and `REFUSED` are two
    `-e` expressions rather than one.
- `frame_grid()` returns an EMPTY grid for a manifest with no scene. A case that renders
  nothing has no frames -- `renders * frames` is zero from both sides -- where the code read
  `self.scene.animation` and crashed.
- **Proving test**: `test/harness/claims/TheCorpusIsRebuiltByOneCommand` -- **1181 of 1181**
  manifests planned. It captures the preparer's stderr to a FILE rather than merging it: the
  preparer's stdout is block-buffered when piped while its notices are not, so a merged stream
  lands a notice inside a JSON line and cuts a token in half. That is how the claim first read
  1180 of 1181.
- **Negative control**, run: the `self.scene is None` guard removed ->

  ```
  NOTE the preparer exited 256
  NOTE it said: Traceback (most recent call last):
  FAIL **THE CORPUS IS PLANNED BY ONE COMMAND**
  ```

  and `harness/claims/EveryOracleWasPreparedByThisPreparer` went red beside it, which is the
  tree's own guard noticing the preparer changed.
- **The runner arm**, proven by removing a prepared case and running its suite:

  ```
  run.sh: render/outshine/grown/cube has no prepared input -- rebuilding it from its manifest
  run.sh: rebuilt render/outshine/grown/cube in 2775 ms
  3 tests: 3 PASS ... 0 UNPREPARED
  ```

  **Negative control**: `RebuildCase` taken out of the case loop -> the same case, same swept
  directory, `3 tests: 0 PASS 3 FAIL`.
- Gate 240/240, 0 UNPREPARED, 84 084 ms of run.
