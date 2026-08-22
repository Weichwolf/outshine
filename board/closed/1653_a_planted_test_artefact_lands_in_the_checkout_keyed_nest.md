Type: bug
Area: test
Tags: tests

# A planted test artefact lands in the checkout-keyed nest

1649/1650 adjudicated the class this evening: a fixed name in the shared temp root is a
neighbour's to overwrite between write and read, and the fix — the checkout-keyed nest,
exported as OUTSHINE_NEST by run.sh — exists precisely so nothing else has to re-derive it.
The same delta that closed 1650 reintroduced the pattern one commit earlier:

- test/render/outshine/client/AReadAndALoadRefuseTheSameMalformedFile.cpp:12-15 (84c4298b)
  plants `${TMPDIR}/outshine-malformed.scenario` — fixed name, shared root. Two checkouts'
  gates race between the plant and the TWO reads (Read, then Load); a neighbour's
  fopen("wb") truncation window between them makes Read and Load see different bytes, and
  the identical-refusal CHECK — the very point of the test — flakes on a defect that is not
  this tree's.

Pre-existing population, same class, gathered here so the sweep is one sitting:

- test/unit/core/TextTargetSaysWhatItCannotWrite.cpp:13
- test/unit/gltf/ALightCrossesWithItsPlaceAndItsUnits.cpp:76
- test/render/outshine/frame/TheFrameCostIsPublishedAgainstItsOwnFloor.cpp:268
  (`${TMPDIR}/outshine-frame` — a whole directory two checkouts share)
- test/render/outshine/scenario/TheChaseCameraSeesTheCarItFollows.cpp:37

Demanded: every test artefact plants inside OUTSHINE_NEST (run.sh exports it since 1650;
a claims test already CHECKs the export before writing — the same form here). Standalone
invocation without the export refuses or keys the name by pid — a fixed name in the shared
temp root is unspellable, in tests as in the gate.

---

Closed: all five sites plant inside OUTSHINE_NEST, with a pid-keyed name (or pid-keyed
directory created on demand) as the standalone fallback -- a fixed name in the shared temp
root is gone from the tests as from the gate. Adjudicated exception: ChaseCamera's
PreparedCar READ of ${TMPDIR}/outshine-prepared stays -- the prepared corpus is shared BY
DESIGN (content-keyed, expensive, read-only here); the class is about writes. Its two
screenshot writes moved into the nest. The named-only hosts (frame, scenario) are
syntax-proven under their own include sets; the full runs are the sporadic proof by rule.
Gate 129/129 warm.
