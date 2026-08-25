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

The survey (`test/CORPORA.md`) priced this exactly: FETCH low, REACH medium, "our refusal must
carry a code and a pointer comparable to the report's, which is the direction `std::expected`
already points". The fetch is done; this item is the reach.
