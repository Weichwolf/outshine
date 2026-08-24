Type: bug
Area: test
Tags: runner, gate, refusal

# The runner's own prose says what the runner does, and a warm-up can fail

## The expect-fail comment states the inverse of the code

```sh
test/run.sh:34   # a case listed here is a case whose RED is a standing finding, not a licence: run.sh prints
test/run.sh:35   # "expect-fail inverted" the moment one goes green, which is what forces the entry out again
```

The code does the opposite. `inverted` counts the declared cases that STAYED red:

```sh
test/run.sh:1175     if [ "$verdict" = FAIL ] && [ "$failures" -eq "$wanted" ]; then
test/run.sh:1176       verdict=PASS
test/run.sh:1177       inverted=$((inverted + 1))
test/run.sh:1178     else
test/run.sh:1179       printf 'run.sh: %s is declared to fail %s claim(s) and reported %s (%s failed)\n' ...
test/run.sh:1181       verdict=FAIL
```

A case that goes GREEN takes the `else` arm: it does not touch `inverted`, it prints a
different line to stderr, and the gate turns red. This round's own run proves it -- both
declared cases behaved as declared and the trailer said:

```
expect-fail inverted: harness/claims/ExpectFail:1 unit/actor/path/ACurveIsFittedAtTheRadiusItHas:1
261 tests: 261 PASS  0 FAIL ...
```

The MECHANISM is right and matches `CLAUDE.md`'s rule exactly. The two lines describing it are
wrong, and they are the only description a reader of `run.sh` gets. The trailer's own wording
carries the same error: `expect-fail inverted:` reads as an alarm and is printed on the
healthy path.

## A `.warms` entry cannot fail

```sh
test/run.sh:1257     while IFS= read -r warming; do
test/run.sh:1258       [ -n "$warming" ] || continue
test/run.sh:1259       ( eval "$warming" ) >>"$log" 2>&1 || true
test/run.sh:1260     done <"$warms"
```

`|| true` swallows every non-zero exit. A warm-up that breaks -- a renamed flag, a build that
no longer links -- leaves the case to pay the compile the mechanism exists to move into the
build column, and the run reports a slow case rather than a broken warm-up. *A failure is
loud* is this tree's rule, and the runner is where it is enforced for everything else:
`Die` is called 40 times in this file.

`eval` on an unvalidated line beside a case is also the widest door in the runner: a `.warms`
file is arbitrary shell, run before the case, with no declared grammar. The one entry that
exists is a `run.sh` re-entry:

```
test/harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols.warms:1   sh test/run.sh --audit-link
```

## What will be true

- [ ] The comment at `test/run.sh:34-35` says what `Record` does: a declared case that stays
      red at its declared count is inverted to PASS and named in the trailer; a declared case
      that goes green is FAIL and names itself on stderr.
- [ ] The trailer line reads as the healthy state it reports -- `expect-fail held:` -- and the
      green-with-a-declaration case gets its own trailer line, not only a stderr print.
- [ ] A failing `.warms` entry stops the run with `Die`, naming the case, the line and the log.
- [ ] `.warms` declares what it may contain rather than being `eval`'d: a runner subcommand and
      its arguments, refused if the subcommand is not one `run.sh` publishes.
- [ ] Proving test: `test/harness/claims/` gains a case that runs `run.sh` over a fixture whose
      `.warms` exits non-zero and asserts a non-zero exit naming the case. Negative control:
      `|| true` restored -> green, which is today's behaviour.
