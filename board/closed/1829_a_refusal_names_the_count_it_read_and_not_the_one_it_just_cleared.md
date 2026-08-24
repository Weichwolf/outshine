Type: bug
Area: render
Tags: refusal, diagnostics

# A refusal names the count it read, and not the one it just cleared

`SubjectDraw::SetMesh` gained named refusals this hour. Two of them clear the counter FIRST and
then interpolate it into the message, so the message reports zero every time.

```cpp
src/render/stages/SubjectDraw.cpp:470       Resident_.NIdx = 0;
src/render/stages/SubjectDraw.cpp:471       error = std::string("the mesh declares ") + std::to_string(Resident_.NVerts) +
src/render/stages/SubjectDraw.cpp:472               " vertices and " + std::to_string(Resident_.NIdx) + " indices but carries no " +
```

`Resident_.NIdx` was set from `mesh.IndexCount` at `:440` and zeroed at `:470`, so a mesh
declaring 30 000 indices and handing over no index run refuses with

```
the mesh declares 10000 vertices and 0 indices but carries no index run
```

**"0 indices but carries no index run" is not a contradiction the reader can act on** -- it
reads as agreement. The number that carries the information is the declared one, and the
refusal below it, five lines away, already does it right by reading `mesh.IndexCount` instead
of the member:

```cpp
src/render/stages/SubjectDraw.cpp:504       Resident_.NIdx = 0;
src/render/stages/SubjectDraw.cpp:507               std::to_string(mesh.IndexCount);
```

The same defect stands at the visibility refusal, older than this hour:

```cpp
src/render/stages/SubjectDraw.cpp:551     Resident_.NIdx = 0;
src/render/stages/SubjectDraw.cpp:552     error = "the subject's " + std::to_string(Resident_.NIdx / 3u) +
src/render/stages/SubjectDraw.cpp:553             " triangles built no visibility structure, so no light could be occluded by them";
```

`0 / 3 == 0`: "the subject's 0 triangles built no visibility structure" is a sentence about a
mesh that had triangles.

## And the argument beside the number is not a label

```
-- a declaration that names geometry it does not hand over draws nothing, and drawing
   nothing is not what it asked for
```

`board:1821` bounded a `Sink` label at 100 characters because prose in `src/` rodata is the
banned comment wearing a string's clothes. The bound was scoped to `Number("` and `Say("`
(`test/harness/claims/TheSourceCarriesNoCommentary.cpp:220`), so an `error =` assignment is
unmeasured. This one is 118 characters of argument after the fact is already stated.

## What will be true

- [ ] Every refusal in `SetMesh` names the counts it was HANDED (`mesh.VertexCount`,
      `mesh.IndexCount`), never a member the same block has already cleared. The clearing moves
      after the message, or the message stops reading members.
- [ ] The refusal states what is missing and stops; the argument for why that is a refusal
      lives in this item and in the commit.
- [ ] The commentary walk measures `error =` and `error +=` literal runs in `src/` against the
      same bound it applies to `Number("` and `Say("`, and publishes the longest one live.
- [ ] Proving test: a unit twin under `test/unit/render/stages/` -- there is none for
      `SubjectDraw` today -- hands `SetMesh` a declaration of N indices with `Indices == nullptr`
      and asserts the returned reason contains `std::to_string(N)`. Negative control: the
      current order restored -> red, showing "0 indices".

**Closed.** All three sites read the DECLARED counts and clear the state afterwards:

```cpp
src/render/stages/SubjectDraw.cpp:470   std::to_string(mesh.VertexCount) ... std::to_string(mesh.IndexCount)
src/render/stages/SubjectDraw.cpp:475   Resident_.NIdx = 0;          // after the message, not before
src/render/stages/SubjectDraw.cpp:551   std::to_string(mesh.IndexCount / 3u)
```

| | |
|---|---|
| before | `the mesh declares 3 vertices and 0 indices but carries no position run` |
| after | `the mesh declares 3 vertices and 3 indices but carries no position run` |

The first reads as agreement -- zero indices and no index run are consistent -- so the reader
learns nothing from the sentence that exists to tell them what is wrong.

Proving test: `test/render/outshine/frame/ADrawCostsWhatTheSweepSaysItCosts`, which now asserts
the refusal contains `3 vertices and 3 indices`. Negative control, run: the message reading
`Resident_.NIdx` again after the clear -> `0 indices` and the case red at :138.

Worth recording: this defect was in a log line I read myself an hour earlier and did not see.
The message said `3 vertices and 0 indices` while the case had set `IndexCount = 3` five lines
above, and I took it as confirmation that the refusal worked.
