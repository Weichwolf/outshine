Type: bug
Area: board
Tags: process, tests

**A closure's named proof stands in the tree before the item closes**

Two round-14 closures (commit b0a89e9e) name proving tests that do not exist:

- 1684 (closed): "Proof: unit test — … Assemble refuses with the coordinate in the message."
  The refusal exists (`src/clients/Assembly.cpp:239-246`) but `grep -rn 'coincide'
  test/unit/` finds nothing; even the neighbouring pre-existing refusal ("no mind stands to
  take it") is untested.
- 1683 (closed): "Proof: extend ALayerOverridesAnEarlierOneById — a stage-redeclaring
  layer's trace contains 'replaced', an id-less instance's trace names its kind." The trace
  lines exist (`src/scenario/ScenarioLayer.cpp:26-29,148-155`) but no test asserts
  "replaced the stage list", "replaced the output list" or "(id-less)" — grep over `test/`
  is empty.

The board rule is "active → closed: body names the proving test" — naming a test that was
never written is the same overclaim 1682 put on the record one commit earlier. The
mechanical bar independently demands it: behaviour a commit changed needs the test that
would catch the old behaviour returning.

Demanded: write both tests (coinciding-ends refusal with the coordinate in the message;
trace assertions for the three new spellings), then append the actual test names to the
1683/1684 closure notes.
