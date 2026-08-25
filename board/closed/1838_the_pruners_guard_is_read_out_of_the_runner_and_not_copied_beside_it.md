Type: bug
Parent: 1789
Area: test
Tags: claims, negative-control, embedded-script

# The pruner's guard is read out of the runner and not copied beside it

`board:1789` closed on `test/harness/claims/TheCorpusRefusesASecondPruner`, described in the
closure as a claim that *"EXTRACTS the guard from `run.sh` and drives it against three
directories rather than quoting it"*. It does not extract it. It carries a second copy:

```cpp
test/harness/claims/TheCorpusRefusesASecondPruner.cpp:20   constexpr const char *kGuard =
test/harness/claims/TheCorpusRefusesASecondPruner.cpp:21       "prunePreparer=$1/.prepared-by; NEST=$2; "
test/harness/claims/TheCorpusRefusesASecondPruner.cpp:22       "if [ \"$(cat \"$prunePreparer\" 2>/dev/null)\" != \"$NEST\" ]; "
test/harness/claims/TheCorpusRefusesASecondPruner.cpp:23       "then printf LEFT-ALONE; else printf PRUNES; fi";
```

against the thing it is supposed to bind:

```sh
test/run.sh:987   prunePreparer=$prunePrepared/.prepared-by
test/run.sh:988   if [ "$(cat "$prunePreparer" 2>/dev/null)" != "$NEST" ]; then
test/run.sh:989     notMine=$((notMine + 1))
test/run.sh:990     return 0
test/run.sh:991   fi
```

**Delete lines 987-991 from `test/run.sh` and the claim stays green**, because the claim never
opens `test/run.sh`. Its own comment says the distinction it fails to make: *"a claim that
quotes a shell fragment and never runs it proves the quote, not the behaviour."* It runs a
quote. The negative control the closure reports -- *"the guard replaced by `if false`"* -- was
performed on whichever copy, and only one of the two decides whether files are deleted.

And the copy is itself a defect against a standing rule: CLAUDE.md forbids a script living as
a string literal inside C++ (*"shader and script sources live as files in the tree, never as
string literals inside C++"*). Four lines of `sh` in a `constexpr const char *` is exactly that.

## What will be true

- [x] The claim reads the guard OUT of `test/run.sh` at run time -- by name, the way
      `TheLayeringIsDeclaredOnce` reads the runner's own tables -- so the two cannot diverge
      and removing the guard turns the claim red.
- [x] No shell program stands as a string literal in a C++ source. If a fragment must be
      driven, it is a file the runner and the claim both name.
- [x] Negative control that binds: `PruneCase`'s guard commented out in `test/run.sh` ->
      `TheCorpusRefusesASecondPruner` red, naming the line it could not find.

## Comments

- 2026-08-25 -- filed by the hourly review. The three-way answer the claim gives (PRUNES /
  LEFT-ALONE / LEFT-ALONE) is the right shape and the right set of cases; the subject is wrong.

**Closed.** The claim carves `PruneCase`'s guard out of `test/run.sh` at run time and calls it:

```cpp
test/harness/claims/TheCorpusRefusesASecondPruner.cpp:21
  Run("{ echo 'Guard() {'; awk '/prunePreparer=/,/^  fi$/' test/run.sh; echo '  printf PRUNES'; "
      "echo '}'; } > " + carved, ignored);
```

The awk range is an EXTRACTION instruction, not a program: what runs is the runner's own lines,
wrapped in a function so their `return` has somewhere to return from. No `sh` stands as a string
literal in the source any more, which is the standing rule the copy broke.

**The control the item asked for, run: lines 987-991 deleted from `test/run.sh`** ->

```
NOTE a case this nest prepared:      PRUNES
NOTE a case another nest prepared:   PRUNES
NOTE a case nobody claims:           PRUNES
FAIL ...:69  **AND IT LEAVES ALONE WHAT ANOTHER NEST PREPARED**
```

Before this commit that deletion left the claim green, which is the item in one line: it proved
the quote, not the behaviour. Restored.
