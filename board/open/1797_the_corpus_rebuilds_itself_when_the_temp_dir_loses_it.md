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

- [ ] `prepare.py all --every-case` plans and runs over every manifest in the tree, including
      those that declare no scene -- a case with no scene renders nothing, which is a fact
      about the case and not an error.
- [ ] `run.sh` finding no corpus on disk rebuilds it before the timed run, says so on one line,
      and reports what the rebuild cost beside the bound.
- [ ] A run whose corpus is already present does NOT rebuild and does not touch the network.
- [ ] Proving test: the preparer's dry-run over every case returns a plan rather than a
      traceback. Negative control: the guard removed -> the `AttributeError` above.
