Type: task
Parent: 0082
Area: harness
Tags: instrument

**The test tree is organised by vendor and role**

Owner's instruction. `test/` is split by **who authored the corpus** and **what role a directory plays**,
the Khronos cases carry **the corpus's own model names**, every corpus has **its own harness**, the
harnesses and the shared tools live under `test/harness/`, and **`test/run.sh` runs everything or one
suite named by its path**.

```
test/harness/shared/          Check.h · Prune · Millis · SubTexelPrecision
             shared/render/   the scoring instrument both corpora compile in
             shared/corpus/   prepare.py · manifest-schema.json · prep/
             khronos/glTF/    ScoreEveryKhronosCase.cpp · prepare/fetch.py
             outshine/render/ ScoreEveryGrownCase.cpp · prepare/{grown,fixtures,GrowPart.cpp}
test/khronos/glTF/            27 model directories, cases only
test/outshine/                unit · shader · frame · harness · host · mods · render (cases only)
```

**A corpus directory carries only its declaration**, and its `.gitignore` got shorter for it: the old one
had to carve out `!/harness/*.cpp` because a source file lived among the cases, and that exception exists
nowhere now.

**How a corpus is obtained belongs to that corpus's harness.** The shared job graph imported `fetch` and
`fixtures` as siblings, so the code that converts, patches and renders every case also knew that one
corpus is downloaded from Khronos and another is grown by this engine. The harness serving a case is now
resolved **by position** — the deepest directory under `test/harness/` matching a prefix of the case's own
path — so adding a vendor adds a directory and there is no list it could be missing from.

## Three assumptions the move exposed, and the third is the one that mattered

| what encoded a depth or a population | how it is found now |
|---|---|
| the manifest schema, at `<case>/../../../corpus/…` | repo-relative — **a depth the schema does not know cannot be got wrong** |
| the preparer's `REPOSITORY`, three `dirname`s up | the parent holding `src/assets` **and** a `Makefile` |
| two harness tests walking `<root>/<theme>/<case>` | recursively, **by what a case carries** |

**The third would have corrupted a measurement rather than breaking a path.** Those walks did not error on
the new layout — **they found nothing and reported no problem.** `ACachedRenderStillNamesItsIndices` exists
to catch a corpus-wide loss of index mappings, and a two-level walk would have made it green over an empty
population.

And the preparer's `REPOSITORY` resolved to test/src/assets/world/species (a path that does not exist, which was the defect), which refused every grown
subject **by name** — a refusal that reads exactly like a bad manifest and is a bad path.

## The hole the split opened, closed before it was committed

**The oracle cache digest globbed one directory**, with a comment stating its own point: *anything under
this directory can change what lands on disk, so everything under it is digested*. Moving `fetch.py` and
`grown.py` to their vendors left them able to change the bytes — **`grown.py` produces the subject
itself** — while no longer being digested, so a change there would silently have **hit** the cache.

The population is now `test/harness/` entire, recursively: **15 files → 17**, and the harness test hashes
byte-identically to the preparer.

**Done when** — met: the tree is as above, `sh test/run.sh <suite>` selects by path and refuses a prefix
matching nothing, and the failing set is the same 20 Khronos cases it was before the move.

## Comments

**2026-08-17** — The reorganisation cost four full-suite cycles, and three of them were spent on the same
class of defect: **a path that encoded where a file happened to sit.** None of the three was a rule anyone
had broken; each was an assumption that had been true for as long as nobody moved anything. *A tree that
has never been reorganised cannot tell you which of its paths are load-bearing.*

**2026-08-17** — A fourth survivor of the same class, found months later and by accident: `test/run.sh`
told an unprepared test to *run test/corpus/prepare.py* — unbackticked here, because it is exactly
the dead path — which stopped existing in this
reorganisation. Two other lines in the same file already named the new path, so it was not a missed rule
but a **third copy of one fact**. It cost nothing here because a human reads that line — but it is the
same defect as the schema walk-up and the preparer's `REPOSITORY`, and it survived four full-suite cycles
and every citation check, because **a path inside a `printf` is not a citation and nothing reads it**.
