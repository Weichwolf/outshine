Type: bug
State: open
Area: gltf, clients
Tags: corpora, measured, spec

# Valid glTF that DRAWS NOTHING is still valid, and the reader says so differently

The glTF-Validator corpus is scored and integrated: 263 cases, Khronos's own `.report.json` as
the oracle, 190 assets it errors on are REFUSED here and that is a real proof. The other 73 are
red and declared in `EXPECT_FAIL` with their count.

**They are not red because the reader is wrong about the spec. They are red because the reader
answers a different question than the validator asks.** Measured 2026-08-25 over the 73:

| what the reader said | cases |
|---|---|
| `no default scene to draw` | **54** |
| `buffer 0 has no uri and the file carries no binary chunk` | 5 |
| `an animation sampler's input does not decode` | 3 |
| `the default scene draws no triangle over 0 primitive(s), so there is nothing to render` | 2 |
| other | 9 |

glTF 2.0 does not require a `scene`, and the validator's fixtures are FRAGMENTS: a file that
declares one accessor and nothing else is a conformant glTF document. The validator judges the
DOCUMENT; `Engine::Declare` judges whether something can be STOOD UP. Both are legitimate and
the tree spells them with one verb, so a conformant fragment and a malformed asset come back
through the same door with the same shape of answer and only the prose tells them apart.

## What will be true

- [ ] A refusal carries WHAT KIND it is, not only prose: the document is malformed, or the
      document is fine and carries nothing this engine can stand up. `std::expected` already
      carries a reason; the kind is what it does not carry.
- [ ] With that kind in hand the corpus scores the second direction honestly -- a conformant
      asset may be declined for want of a scene, and may NEVER be declined as malformed -- and
      the 73 leave `EXPECT_FAIL` by being answered, not by being reclassified.
- [ ] The count in `EXPECT_FAIL` falls as they are answered. It is 73 at c8afa7e2.

## Measured 2026-08-25: the 190 refusals are not 190 detections

The refusal was removed to see what stands without it, and what came back re-prices the whole
item.

| `Subject::Build` | validator suite |
|---|---|
| as it stands | 263 PASS (73 of them inverted `EXPECT_FAIL`) |
| both refusals removed | 136 PASS, 58 FAIL, **69 SIGNAL** |
| only the scene refusal removed | 139 PASS, 56 FAIL, 68 SIGNAL |
| the scene refusal removed, `Bound()` guarded | 139 PASS, **124 FAIL**, 0 SIGNAL |

Two things fall out and neither was in this item before.

**The refusals are load-bearing over a crash.** `Subject::Bound()` read `Positions_[0]` with no
vertex behind it -- 68 of the 263 cases died on a null dereference the moment a document without
a scene was allowed to stand. That is fixed on its own merit: an empty body has an empty bound,
zero to zero, and the guard is in.

**And 124 of the 190 refusals refuse for the WRONG REASON.** They are not detections of the
error Khronos reports; they are the scene blanket firing first. Take it away and 124 documents
the validator errors on stand up here. The corpus has been crediting us with 190 refusals of
which at most 66 are the reader actually reading.

So the reach this item names is bigger than "carry a kind", and the order is now clear:

1. the reader detects the 124 errors it currently refuses by accident, one class at a time.
   **Started: accessor bounds, 124 -> 111.** glTF 2.0 says `accessor.min`/`max` are the ACTUAL
   componentwise extremes of the data, and the reader checked only that a POSITION accessor
   CARRIED them. `Document::BoundsHold` now reads the elements and refuses in both directions --
   a box around the data and a box inside it -- for every accessor that declares bounds, not
   only for mesh POSITION. Thirteen of the 124 fall to it. Proving test:
   `harness/outshine/fuzz/ScoreWhatAnAccessorBoundClaims`.

   **"NOTHING conformant is refused" is not proven, and one class is provably wrong.**
   `Document::BoundsHold` compares `accessor.Min`/`Max`, read RAW out of the JSON
   (src/gltf/Document.cpp:653,656), against elements `ReadElements` has already NORMALISED
   (`accessor.Normalized ? Normalise(raw, accessor.Component) : raw`). A conformant normalised
   accessor -- `componentType 5121`, `normalized true`, `min [255,255,255]` -- reads back as
   `1.0` and is refused as *carrying an element outside the bounds it declares*. The corpus
   does not catch it: of 729 prepared `.gltf`, exactly one case declares a normalised accessor
   with bounds (`khronos/validator/json-integer-written-as-float`) and it is already in
   `EXPECT_FAIL`. KHR_mesh_quantization and every meshopt pipeline declare POSITION exactly
   that way, so this refuses real assets.

   And the check goes GREEN where it cannot look (board:1857): three `return true` escapes --
   a component-count mismatch, a `ReadElements` that failed, a size that does not match --
   answer *bounds hold* for an accessor nobody read.

   The case has the same hole: it declares one `componentType 5126` triangle and no normalised
   accessor at all, so it cannot fail on the class the reader gets wrong. It is also SPEC grade
   sitting under `fuzz/`, which is INPUT grade -- a case filed where its grade is not what the
   folder says.
2. THEN the scene blanket comes off and a conformant fragment stands and draws nothing
3. and only then does a refusal need to say which kind it is

Doing 2 before 1 would turn 124 accidental correct answers into 124 wrong ones.

Step 2 also needs `Subject::Bound()` guarded -- it reads `Positions_[0]` with no vertex behind
it, which is what the 68 signals were. The guard is NOT in the tree, on purpose: while the scene
refusal stands the guard cannot be reached, and an unreachable guard is dead code with no test
behind it. It lands with step 2, proven by the same measurement that needs it.
`harness/outshine/fuzz/ScoreWhatTheReaderSurvives` finds that crash in 0.3 s the moment the
refusal comes off, which is what makes step 2 safe to attempt.

The survey (`test/CORPORA.md`) priced this exactly: FETCH low, REACH medium, "our refusal must
carry a code and a pointer comparable to the report's, which is the direction `std::expected`
already points". The fetch is done; this item is the reach.
