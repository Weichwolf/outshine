Type: bug
Area: scenario
Tags: mods, layering, grammar

**A row's merge identity is the identity the grammar adjudicated**

The merge keys 1671 closed on contradict the identity verdicts 1655 recorded:

- **Door.** 1655's closing states "a door's identity IS its two ends" and made
  `from`+`to` Required. `MergeLayer` merges doors `ByIdField` on the OPTIONAL `id`
  (src/scenario/ScenarioLayer.cpp:105) — a mod restating the door between the same
  two rooms ADDS a second door between them, traced as `added door ''`.
- **Sound.** 1655 made `uri` Required ("a sound that names no source sounds
  nothing"); the merge keys on optional `id` (ScenarioLayer.cpp:107). A mod's
  `<sound uri="wind.ogg" gainDb="-6"/>` without an id duplicates the base's row —
  the asset precedent one line up (uri is the name, digest the pin) says the
  opposite. Adjudicate: id-if-declared, else uri, or make id Required for sound.
- **Vehicle.** 1655 adjudicated the vehicle element "a singleton identified by its
  position, exactly like world"; the merge treats Vehicles as rows keyed by the
  optional `name` with NO empty-guard (ScenarioLayer.cpp:42-45) — two unnamed
  vehicles merge only by the accident of `"" == ""`, while `ByInstanceId` and
  `ByIdField` both guard empties. One of the two recorded designs is wrong.
- **The normative comment lies.** ScenarioLayer.cpp:7-8 claims the identity is "the
  same attribute the grammar's Required column names" — false for door, sound,
  region, volume, bus, table and view, whose ids the grammar leaves optional.

Demanded: one recorded identity per collection, consistent with (or explicitly
amending) 1655's verdicts — door by (From,To), sound adjudicated, vehicle either
singleton-replace or name Required; empty-identity guards uniform; the MergeRows
comment made true; a proof per corrected key in ALayerOverridesAnEarlierOneById.

---

Closed: the merge keys match the 1655 verdicts -- a door merges by its TWO ENDS (its
identity), a sound by its required URI, the vehicle is the singleton it was adjudicated to
be (a layer declaring one replaces it whole, traced); the id-keyed rows guard empty ids
(id-less adds); and the normative comment describes the keys that exist. Proven in the layer
test (sound override by uri with the optional id absent from the assertion path).
