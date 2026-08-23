Type: task
Parent: 1480
Area: clients
Tags: scope

**A device event reaches the client as a declared action**

`Input` bindings are read and carried and nothing binds. **The client must never see a keycode** -- the
binding is the scenario's, so remapping is a scenario edit and not a code change, and a scenario for a
gamepad and one for a keyboard are two declarations of one game.

## What must be true

- [ ] **A device event maps to a NAMED action** and the client is handed the name
- [ ] **An action nothing binds is a refusal at stand-up**, not a key that does nothing at run time
- [ ] **A binding is 0 or 1..N per action**, because a game binds `forward` to `W` and to a stick
- [ ] **An axis is not a button**, declared, so a trigger's 0..1 is not a press
- [ ] **The host surface is one interface** and outshine calls nothing else
- [ ] **Input to photon is MEASURED**, because `CLAUDE.md` names latency as the feature nobody declares

---

Progress -- five of six boxes stand: src/scenario/InputMap is the one surface. A device
event maps to a NAMED action (the client never sees a keycode); the event catalogue is
constexpr and the engine's (a scenario selects, cannot add; a foreign event refuses naming
the catalogue); 1..N bindings per action (forward answers to KeyW and to the stick); one
event bound twice refuses ("one press has one meaning"); AN AXIS IS NOT A BUTTON, declared
in the catalogue row; Requires() refuses an unbound ask at stand-up. Proving test:
unit/scenario/ADeviceEventReachesTheClientAsADeclaredAction.cpp. Remaining: the SDL event
pump wiring in the window host and the input-to-photon MEASUREMENT (the frame suite's
slice).

---

Sharpened (review 2026-08-23, round 18): before the SDL pump wires in, the runtime shape
must honour values-over-strings — `InputMap::Row` carries `std::string Event/Action`
(src/scenario/InputMap.h:29-33) and `ActionOf` (InputMap.cpp:69-74) does a per-device-event
linear scan of string compares. The catalogue is already constexpr and indexable: Build
should intern the event to its catalogue index and the action to a small id once at
stand-up, so the per-event lookup is an integer compare and the client-facing name is
resolved off the event path. Also: `Build(const std::vector<Binding>&)` → span (1621).
