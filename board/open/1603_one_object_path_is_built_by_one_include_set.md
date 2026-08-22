Type: bug
Area: test
Tags: build

**One object path is built by one include set**

`BuildGroup` names objects `$OBJDIR/$(dirname|tr / -)-$(basename).o` (test/run.sh:266) — the
GROUP that built the object is not in the name, and `UpToDate` checks timestamps and `.d`
prerequisites, never the flags. Two arms of `GroupIncludes` now claim the same object with
DIFFERENT include sets:

- `src/sim/Rigging.cpp` (test/run.sh:220): `-Iinclude -Isrc/core -Isrc/corridor -Isrc/physics
  -Isrc/pilot -Isrc/sim` — the narrow set `unit/sim` builds with.
- `src/sim` (test/run.sh:226): the wide set carrying `-Isrc/data -Isrc/world -Isrc/world/tiles
  -Isrc/core/io` — what `tools/driver*` and `BuildLibrary` build the SAME
  `src-sim-Rigging.o` with (in `BuildLibrary`, find-order means the `src/sim` dir group
  compiles Rigging first and the narrow file arm is then skipped as up-to-date, so the narrow
  truth NEVER applies to the archive).

So build order decides which layering claim was enforced: a stale wide-built object satisfies
`unit/sim`'s link even when the narrow set would refuse the include. That is the exact
file-beside-directory ambiguity board:1598's close claimed to have dropped — one instance was
dropped, this one remains. (`src/scenario/ScenarioRead.cpp` beside `src/scenario` shares one
arm and is benign — same flags — but the same mechanism.)

Demanded: the object path carries the group (or the compile command joins `UpToDate` the way
`Fresh` already hashes the link command for binaries), or the file arm dies and Rigging's
narrowness is proved by a compile-only subject like the ones `unit/compile` already hosts.
