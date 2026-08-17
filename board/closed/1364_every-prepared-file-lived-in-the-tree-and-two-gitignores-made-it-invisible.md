Type: bug
Area: harness
Tags: instrument, corpus

**Every prepared file lived in the tree, and two .gitignores made it invisible**

`CLAUDE.md` states the constraint outright — *every artefact goes to the system temp directory, never
into the tree; a repository is what is declared and what is built from it.* **The preparer wrote into
the case's own directory**, so `test/` carried **158 MB** of fetched buffers, images, converted
`.blend`s and oracle `.exr`/`.raw` products.

**Nothing was committed and that is why it survived.** `test/khronos/glTF/.gitignore` and
`test/outshine/render/.gitignore` each open with `*`, so `git status` stayed clean and no diff could
ever show it. *A rule stated in prose, satisfied by a `.gitignore` and contradicted on disk is a rule
the tree does not carry.*

## The repair is one mapping, spelled three times and derived from the case path

> `<system temp>/outshine-prepared/<the case's path with its separators flattened>`

| where | what it is |
|---|---|
| `test/harness/shared/corpus/prepare.py` | `prepared_directory()` — the default `--dest`, and it copies the manifest in beside what it produced so the runner's *one directory* contract holds |
| `test/run.sh` | `PreparedCase()` — what the runner is handed and what the prune is pointed at |
| `test/harness/shared/PreparedRoot.h` | what the C++ tests walk |

**Three spellings and not one, because they are three languages.** Each derives the leaf from the case
path by the same rule, so none stores a table and a case added tomorrow needs no entry anywhere.

## What it cost, measured

| | before | after |
|---|---|---|
| `test/` on disk | **158 MB** | **2.6 MB** |
| files under the case trees that are not a manifest or a `.gitignore` | **265** | **0** |
| the two published counts | `criteria 31 met of 37`, `18 within` | **identical** |

**Identical is the right result here and it is worth saying why.** `CLAUDE.md` warns that a change
meant to alter the picture cannot reproduce it to six decimals — this one was not: it moves where bytes
live and touches no declaration, so an unchanged picture is the evidence that it did only that.

## Three things it broke, each of which was the point

**Six tests walked the case trees for products** and reported an **empty population** rather than a
failure — `EveryOracleWasPreparedByThisPreparer`, `EveryRenderNamesItsIndices`,
`TheOraclesExrReadsAsItsRaw`, `ADerivedCameraIsTheFramingRuleAndNotAQuotation`,
`AGeneratedBasisIsTheOneTheExporterWrote`, `TheTriangleProjectsToTheOraclesArea`. *A green test over
nothing is the shape this repository keeps finding, and the relocation surfaced six of them at once.*

**Two board items cited a prepared path as if it were tracked** — both naming the triangle case's
*scene.glb*. `EveryPathCitedInADocumentResolves` caught them the moment the leftover artefact was gone:
**the citations had been resolving because a product happened to sit on disk**, never because the tree
carried the file. `CLAUDE.md`'s own rule applies — something to be built is named in prose — and both
now are.

**The vendor walked up from the DESTINATION to find `test/harness`.** Once the destination left the
repository there was no parent holding it, and every case refused. It resolves from the manifest now,
which is the one thing about a case that is always inside the tree.

**Held by `test/outshine/harness/APreparedFileNeverLandsInTheTree.cpp`**, which asserts the population
before the predicate over it: a case tree that had been emptied would otherwise report a clean pass.
