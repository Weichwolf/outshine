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
- [ ] **Every `[0]` on a container the format allows N of is either glTF's own rule or a defect**, and
  which one is written down per site. [MEASURED] there are 11 such sites in `src/`, and the ones read
  so far are legitimate: `Primitives[0].Targets` is glTF stating every primitive of a mesh agrees, and
  `Deck[0..2]` is a three-deck cloud model
- [x] **Every declared bound refuses or publishes when reached**, and the ones already checked are
  `kMaxSubjectLights` 16 · `kMaterialSlots` 1 · `kGroundSlots` 12 · `kMaxColourAttachments` 8 ·
  `kUvSets` 2 · `kTagSlots` 32 -- the last one was the defect and is fixed
- [x] **A scene is 0 or 1..N.** `Scenes_` is a vector, an out-of-range `scene` is a refusal naming
  both numbers, and `MultipleScenes` -- two scenes with `scene: 1` -- is green. Where the key is absent
  the reader takes 0, which glTF permits, and `DefaultScene()` answers which, so it is PUBLISHED
- [x] **A camera is 0 or 1..N**: `Cameras_` is a vector and a node naming one past the end is a
  refusal quoting the count
- [ ] **The sweep's result is a TABLE**, one row per site, so the next round reads what was decided
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

**Still to read**: the UI layer, the world streamer, the generators and the compositors. *Four rows of
the twelve above came from the glTF reader because that is where a FORMAT states what it permits; the
engine's own layers state their own multiplicities and each needs the same question asked.*

