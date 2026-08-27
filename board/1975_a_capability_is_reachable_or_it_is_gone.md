Type: issue
State: active
Parent: 1953
Area: architecture

# A capability the tree holds is reachable from a declaration, or it is gone

`test/run.sh` declares `STRANDED=6`: six sources no suite links, so nothing they hold is proven.
Four of them are included by NOTHING at all -- 440 lines that no declaration and no test can
reach -- and they are not stubs:

| source | lines | what it can do | why it is unreachable |
|---|---|---|---|
| `src/scenario/Mod.cpp` | 75 | reads a named overlay from a root and lists its scenes | `namespace outshine::SceneLegacy` -- a scenario system `include/Scenario.h` replaced |
| `src/scenario/Scene.cpp` | 255 | the legacy scene, stage, standpoint and studio | the same namespace |
| `src/scenario/Animation.cpp` | 144 | the legacy animation track | the same namespace |
| `src/scenario/Tables.cpp` | 91 | `TableBook`: named data tables, numbers and text by table/row/column | `Scenario::Tables` IS declared and no host reads one (board:1862) |
| `src/audio/BusGraph.cpp` | 167 | buses, sounds, a master and voices | `Scenario::Sounds`/`Buses` ARE declared and nothing outside its own two files names it |
| `src/engine/RegionForge.cpp` | 107 | background region growth against a lease | held by nothing since the sim was rebuilt (board:1946) |

**The two answers are not the same and the rule decides which.** `SceneLegacy` is a SECOND
SPELLING of a truth the tree already tells: `include/Scenario.h` plus `ScenarioRead` is the
scenario, and CLAUDE.md says delete on the day you replace -- that day was passed, and what was
kept is 945 lines of scenario system nothing can reach, including a `SceneWeather.h` whose only
mention of itself is its own header. The other three are DECLARED CAPABILITIES: a scenario can
write `Tables` and `Sounds` today and the engine accepts them and does nothing, which CLAUDE.md
calls worse than a refusal.

- [ ] the `SceneLegacy` cluster is deleted -- `Mod`, `Scene`, `Animation`, `Fields`, `Stage`,
      `Standpoint`, `Studio` and `SceneWeather` -- and nothing in the tree misses it
- [ ] `Tables` reaches `TableBook` from the door, or `Assemble` refuses the declaration by name
- [ ] `Sounds`/`Buses` reach `BusGraph` from the door, or `Assemble` refuses them by name
- [ ] `RegionForge` is reached by the world that streams, or it is deleted
- [ ] `STRANDED` falls to what remains and the number is declared, not discovered
