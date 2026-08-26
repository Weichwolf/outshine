Type: task
State: open
Parent: 1953
Area: door

# The door holds only structs that earn their place

`include/Scenario.h` carries 38 structs. Audited one at a time against RAGE and Unreal, seven do
not earn their place, worst first.

| what stands | why it is wrong | what the benchmarks do |
|---|---|---|
| **`Mind`** -- `Tier`, `Uses`, `Programme`, **`Prompt`**, **`Model`**, `Meanwhile`, `Hz` | a language-model prompt in the engine's public declaration. It is neither mechanics nor optics, and it pins one technology into a door that outlives it | Unreal: `AAIController` plus a Behavior Tree ASSET. RAGE: a `CTask` tree. Neither names a model or carries a prompt |
| **`Kind::Capabilities`** and **`Instance::Holds`** as `std::vector<std::string>` | TARGET's own diagram says *CAN -- capability tags, constexpr catalogue (typo = compile error)*, and the catalogue EXISTS in `src/scene/Register.h` as `tags::DoesSteer` and friends. The door hands strings, so a C++ client's typo is a runtime refusal where the diagram promises a compile error. The XML path is right: `CapabilityNamed` refuses at assembly | both resolve names to typed handles at load and never carry the string inward |
| **`Setting`** and **`Attribute`** | `{std::string Name; std::string Value;}` -- the same struct declared twice, forty lines apart, one for generators and one for kinds. A second spelling of one truth, literally | -- |
| **`Door`** as a top-level struct | a SUBJECT. A door is a body on a revolute joint with a drive, which is board:1965's vocabulary | RAGE and Unreal both build doors out of constraints; neither has a door type in the engine |
| **`Drive`** -- `FromLat/Lon`, `ToLat/Lon` | a route request named after a vehicle verb. TARGET's own diagram calls it *PATHFINDING -- two coordinates in, corridor out: walk, drive, fly, rail*, so `drive` is one MODE of a route and not the noun | Unreal: `UNavigationPath`. RAGE: a path request with a mode |
| **`Surface`** -- `Document`, `Style`, `Programme`, `Patch`, `Z` | a UI panel wearing the name the renderer uses for materials (`SurfaceTable`, `ResolveSurfaceTable`, `SubjectMaterial::Row`). Two meanings, one word, in one door | -- |
| **`Light`** -- `Lux`, `ElevationDeg`, `BearingDeg` | it is the SUN, and `PunctualLight` sits beside it in the same door carrying `LightKind::Directional` and intensity in the same unit glTF uses for it, lux. The placement form is right for a sky body; the NAME says something the type next to it already means | glTF: one punctual light with a kind |

None of these is urgent and all of them are permanent: a public struct is what nobody can change
afterwards, which is why the door is where an error costs the most.

- [ ] `Setting` and `Attribute` are one struct
- [ ] the capability catalogue is a door enum, so a C++ typo is a compile error and XML still refuses
- [ ] `Mind` carries no model and no prompt
- [ ] `Door` is an assembly of a body, a joint and a drive (board:1965)
- [ ] `Drive` is a route with a mode; `Surface` is a panel; `Light` says it is the sun
