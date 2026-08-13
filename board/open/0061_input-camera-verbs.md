Type: feature
Area: clients
Tags: oracle

**I.14 Input, camera, verbs**

- [x] Free camera with a declared stance, eye riding the DEM (`Sim::Look`)
- [x] Orthographic camera for a bird's eye (`demo/ortho`)
- [ ] `Sim &Simulation()`'s non-const overload dropped — it makes moving the eye without the camera basis spellable
- [ ] **Scenario selection in the interactive client**: the declared scenarios offered as a choice, the chosen one named in a resumable address — a URL where the host has one, an argument where it does not — so a chosen scenario survives a restart and can be handed to someone else, and the client loaded with it directly. A scenario settable in three places and declared in none is what cost three rounds on 2026-08-11
- [ ] **A client with an input medium exists, and a declared interactive scene runs.** `Scene::Kind` already has an interactive arm and `b83285f` deleted the only target that could execute one: the deleted walk client refuses it (`scene_is_interactive`), so the arm is unreachable and the walker — the stance integrator behind the verb — went with it, compiled with no caller and then deleted (the bug tasks in `board/`). This is not the character controller below it and not a verb; it is the seam without which none of the verbs under it can ever be shown to work, and `CLAUDE.md` § *Setup* already asserts it exists (*"the interactive client is a test; so is the frame oracle"*)
- [ ] Walk, with the character controller under it
- [ ] Run, crouch, jump, climb, vault
- [ ] Swim, with the medium under it
- [ ] Drive, fly, ride — one physics system, three propulsion declarations
- [ ] First and third person
- [ ] Footstep response to the contact material under the foot
- [ ] Interaction: open, carry, use, sit
- [ ] Input rebinding as a declaration
