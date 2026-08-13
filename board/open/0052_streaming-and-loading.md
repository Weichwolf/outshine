Type: feature
Area: world
Tags: instrument

**I.6 Streaming and loading**

- [x] Nothing preloaded; every tile on demand
- [ ] Every point on Earth a valid start — split out of the line above 2026-08-12 and un-ticked on measurement, not on principle: everything past `±85.05112877980659°` is now refused by name at the declaration (`clients/Scene.cpp`), which is right against the tile scheme and false against `CLAUDE.md`'s own sentence. **0.373 % of the Earth's surface**, 1.90 million km² in the two caps together (derived: the fraction outside both caps of a sphere is `1 − sin 85.05113° = 0.0037279`, × 4π·6371 km²), most of it Arctic Ocean and the Antarctic plateau — and it is the owner's call whether it is closed by a second projection or by striking the claim
- [x] Loading as an application phase with a progress fraction, never a renderer state (`ProgressStage`)
- [x] The renderer runs at full rate during loading
- [x] No ceiling and no timeout on the initial load
