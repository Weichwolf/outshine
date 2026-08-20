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
