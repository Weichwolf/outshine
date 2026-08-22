Type: bug
Area: scenario
Tags: mods, layering, loud-failure

**Every declared section is merged or spoken — never swallowed**

The emptiness proxies survived 1671's own standard ("nothing is swallowed"):

- **A player without `is` vanishes without a word.** The grammar requires nothing on
  `scenario/player` (src/scenario/ScenarioRead.cpp:82), so `<player view="chase"/>`
  parses. `MergeLayer` gates the replacement on `!layer.Played.Is.empty()`
  (src/scenario/ScenarioLayer.cpp:150) — the section merges to NOTHING and leaves no
  trace row. It is the only section with neither a Declared flag nor a spoken drop;
  the exact silence 1671 was filed against, reborn on the one singleton the closing
  proxied.

- **Unacted ignores the flags the same commit introduced.** The reader now sets
  `Motion.Declared` and `Time.Declared` (ScenarioRead.cpp:336,342), but the
  capability list still reads emptiness: `!scenario.Motion.Dial.empty()` and
  `!scenario.Time.Start.empty()` (src/clients/Engine.cpp:76-77). `<clock rate="2"/>`
  and `<physics/>` are declared, unacted, and unspoken — Carried stays mute where
  Ground already speaks by `.Declared` (Engine.cpp:75).

Demanded: `Player` gains `bool Declared` set at ScenarioRead.cpp:567 like its five
siblings; `MergeLayer` replaces the player on the flag and traces it; `Unacted`
speaks physics and clock from the flags. Proof: a layer's `<player view=.../>`
replaces and traces; a rate-only clock lands on Carried.
