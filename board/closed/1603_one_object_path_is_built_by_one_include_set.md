Type: bug
Area: test
Tags: build

**One object path is built by one include set**

`BuildGroup` names objects `$OBJDIR/$(dirname|tr / -)-$(basename).o` (test/run.sh:266) — the
GROUP that built the object is not in the name, and `UpToDate` checks timestamps and `.d`
prerequisites, never the flags. Two arms of `GroupIncludes` now claim the same object with
DIFFERENT include sets:

- `src/sim/Rigging.cpp` (test/run.sh:215): `-Iinclude -Isrc/core -Isrc/corridor -Isrc/physics
  -Isrc/pilot -Isrc/sim` — the narrow set `unit/sim` builds with.
- `src/sim` (test/run.sh:231): the wide set carrying `-Isrc/data -Isrc/world -Isrc/world/tiles
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

---

**Closed (review 2026-08-22).** The object name carries the checksum of the group's include set
and standard (`setId`, test/run.sh:272-273), so the narrow unit build and the wide library
build of Rigging.cpp write different artefacts by construction; the sanitised and validated
arms build into their own OBJDIRs (obj-sanitised, obj-validated), so SAN and EXTRA_DEFINES
cannot collide with a plain object either. TheLayeringIsDeclaredOnce asserts the mechanism;
claims suite green this run (12/12 + reporter). Task 1609 closed.
