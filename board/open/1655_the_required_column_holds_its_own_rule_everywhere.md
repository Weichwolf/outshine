Type: bug
Area: scenario
Tags: grammar, refusal

**The Required column holds its own rule everywhere**

board:1484 closed on one population rule: the attribute that names what the element IS —
`of, kind, name, uri, asset, document, what, do, path` — is required, ids never. Twenty rows
carry it. Two rows break the rule the closure itself states:

- `scenario/audio/sound` (ScenarioRead.cpp:70) requires nothing, although `uri` sits in the
  rule's own lexicon and `scenario/assets/asset` (line 47) requires it. A `<sound id="x"/>`
  with no uri loads in silence and sounds nothing — the exact defect class 1484 named.
- `scenario/input/bind` (ScenarioRead.cpp:98) requires neither `event` nor `action`. A bare
  `<bind/>` binds nothing and loads without a word. A bind IS event→action; both belong in
  the fourth column.

Arguable neighbours, to be adjudicated in the same pass rather than left implicit:
`regions/door` without `from`/`to` connects nothing (line 65); `volumes/volume` without
`fires` fires nothing (line 67); `vehicle` permits `name asset` but requires neither while
`kinds/kind` requires `name` (line 84 vs 54). The id-exclusion itself is consistent and
stands.

Demanded: the two clear rows gain their Required entries, the arguable three get one recorded
verdict each (required or deliberately optional, one line why), and the proving test in
test/render/outshine/client/AClientRunsAScenarioInFourLines.cpp gains a refusal case per new
entry.
