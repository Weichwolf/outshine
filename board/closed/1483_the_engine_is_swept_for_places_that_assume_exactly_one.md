Type: task
Parent: 1382
Area: core
Tags: scope

**The engine is swept for places that assume exactly one**

`board:1482` found two by asking the format what it permits and the tree what it spells, and both were
SILENT -- a wrong answer that looks right, which is the worst shape a shortfall takes. This is the rest
of that sweep.

**The rule**: *a shape is 0 or 1..N; code that assumes exactly one of something is a defect waiting for
the second.* **The test is not whether a bound exists** -- a bound somebody chose on purpose is what
`CLAUDE.md` asks for -- **it is whether reaching it is NAMED**.

| | | |
|---|---|---|
| **named refusal** | the bound is in the sentence | correct, and most of this tree |
| **published shortfall** | the count and what it dropped are answered | correct where the picture survives |
| **silent truncation** | neither | **the defect class** |

## What must be true

- [x] **Every place that reads a numbered attribute set reads all of them or refuses by name** --
  `JOINTS_n` is done, `TEXCOORD_n` refuses, `COLOR_n` has no further semantics; the sweep must state
  which others exist rather than assume these are all
- [x] **Every `[0]` on a container the format allows N of is either the format's own rule or a defect**,
  and which one is written down per site. [MEASURED] 11 such sites in `src/`, and every one is
  legitimate -- see the table
- [x] **Every declared bound refuses or publishes when reached**, and the ones already checked are
  `kMaxSubjectLights` 16 · `kMaterialSlots` 1 · `kGroundSlots` 12 · `kMaxColourAttachments` 8 ·
  `kUvSets` 2 · `kTagSlots` 32 -- the last one was the defect and is fixed
- [x] **A scene is 0 or 1..N.** `Scenes_` is a vector, an out-of-range `scene` is a refusal naming
  both numbers, and `MultipleScenes` -- two scenes with `scene: 1` -- is green. Where the key is absent
  the reader takes 0, which glTF permits, and `DefaultScene()` answers which, so it is PUBLISHED
- [x] **A camera is 0 or 1..N**: `Cameras_` is a vector and a node naming one past the end is a
  refusal quoting the count
- [x] **The sweep's result is a TABLE**, one row per site, so the next round reads what was decided
  rather than re-deriving it

## What this may not do

**It may not add a bound to make a site look considered.** The defect is silence, not the number: a
site that reads exactly one because the format states exactly one is correct and gets a row saying so.

## The table, so far, and it is what was READ rather than what was assumed

| site | multiplicity | what it does when reached | verdict |
|---|---|---|---|
| `JOINTS_n` / `WEIGHTS_n` | N | reads all; a half-declared set refuses naming which | **was silent, fixed in `board:1482`** |
| `Heap` tag slots | 32 | overflow lands under `other`, reserved at slot 0 | **was silent, fixed in `board:1482`** |
| `TEXCOORD_n` | 2 bound | a texture reading past it refuses with the bound in the sentence | named |
| `COLOR_n` | 1 read | `COLOR_0` is the only set the core format gives semantics to | correct by the format |
| morph targets | N | loops `Targets.size()`, baked on the CPU so the device sees none | correct |
| scenes | N | out-of-range refuses; absent `scene` takes 0 and `DefaultScene()` answers which | published |
| cameras | N | a node naming one past the end refuses quoting the count | named |
| animations | N | a declared SET is played, and two driving one claim refuse naming both | named |
| animation channels | N | every channel of every declared animation; an undrivable one is counted and quoted | published |
| `Primitives[0].Targets` | 1 read | glTF states every primitive of a mesh agrees, and a file that disagrees refuses | correct by the format |
| `Deck[0..2]` | 3 | a three-deck cloud model, which is the model and not a bound | correct |
| punctual lights | 16 | a longer list refuses naming both numbers | named |
| colour attachments | 8 | `static_assert` and a refusal | named |
| `.back()` / `.front()` sites | -- | 11 read: stack tops, ring closure, growth points | none is an exactly-one |

### The engine's own layers, read after the reader's

| site | multiplicity | what it does | verdict |
|---|---|---|---|
| `GeneratorSet::Add(rank, …)` | 0..N | a vector, ranked | correct |
| `SourceSet::Add` | 0..N | a vector, ranked, and a rank clash is a named refusal | correct |
| **`RegisterDeclared`** | **fixed 3** | `StarBands`, `TerrariumDem`, `VersatilesVector` in an `if` chain behind one boolean | **a fixed N where the architecture promises a declared one** |
| flex `lines[0]` | 1..N | the final `push_back` is unconditional, so there is always a line; `lines[0]` is guarded by `!wraps`, and CSS gives a non-wrapping container exactly one line | correct |
| flex `lines.size() - 1` | 1..N | cannot underflow for the same reason | correct |
| `ChunkMesh` / `BuildingMesh` `[0..2]` | 3 | a vector's x, y, z -- a component index and not a count | not this class |
| `TreeSpecies` colour `[0..2]` | 3 | the same | not this class |

## THE ONE FINDING OF THE SECOND HALF, and it is not a silent truncation

**`Data::RegisterDeclared` spells three provider nouns in an `if` chain gated by `WithUpstreams`.** The
architecture says *external data behind a provider interface, ranked, absence hands over*, and
`board:1480` says a scenario declares which providers with what pin and what rank. **The set below that
promise is fixed at three and chosen by a boolean.**

*It is not this item's defect class*: nothing is truncated and a rank clash refuses by name. It is a
feature not yet reached, and it already has its row -- `Engine::Carried()` publishes `1 providers` for a
scenario that declares one, which is the runtime saying it read the declaration and did not act on it.
**So the sweep's finding is recorded where the work is, and this item does not open a second one.**

## What the sweep concludes

**Two silent truncations in the whole engine, both in `board:1482`, both now named.** Twenty-one sites
read; nineteen were already correct, and the distinction that matters held everywhere it was tested: a
bound somebody chose is not a defect, and a bound nobody is told about is.

## What proves it

**`test/unit/gltf/AVertexRidesEveryJointSetTheFileDeclares.cpp`** -- the `JOINTS_n` row: eight
influences land on the mean of eight joints and four on the mean of four, 2 m apart.

**`test/unit/core/EveryByteTheHeapTakesLandsUnderATagOrUnderOther.cpp`** -- the tag-slot row: 64 tags
into 32 slots, every byte counted, `other` reported like any other row.

**`test/unit/gltf/ASecondUvSetReachesItsOwnReferenceOrIsRefused.cpp`** -- the `TEXCOORD_n` row, and it
was already there: `TEXCOORD_1` over a subject carrying one set is refused naming both, and
`TEXCOORD_2` is refused *whatever the subject carries*, with `2 uv sets` in the sentence. **A second
test for that claim was written this round and DELETED unread into the suite**, because the claim
already had a place and two would drift.

## Comments

**Five `Covers` strings still named a work item** -- `Covers("board:1177")` and four like it -- which is
the rule `board:1474` reversed and the comment purge missed, because they are string content rather
than comments. Each now names the CAPABILITY it covers. *A test says what the engine can do; which slip
of paper asked for it is the board's business.*

