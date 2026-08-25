Type: bug
State: open
Parent: 1862
Area: clients
Tags: door, driver, measured

# Read -> Assemble -> Advance stands a picture, or it refuses at Read

The front door says: **outshine loads a scenario and runs it.** Run exactly that way, it renders
nothing and says nothing until the third call.

Measured 2026-08-25 at a3ebe3e0, the command the architect's brief prescribes:

```
build/outshine-driver --headless --into DIR --assets .../apps-driver-f31
DRIVING 0.00000,0.00000 -> 0.00000,0.00000, 1280x720, headless
NO DRIVE DECLARED
STOPPED after 0 frames: no scenario is standing, so there is nothing to advance
0 stills
```

`Engine::Assemble()` returned TRUE. `Engine::Advance()` then refused. Both are right about their
own state and the door is wrong: there are two arrival routes and only one of them stands a
picture.

| call | what it sets | stands a picture |
|---|---|---|
| `bool Engine::Read(std::string_view path) {` (src/engine/Engine.cpp:372) | `S_->Declared`, `S_->Carried` | no |
| `Engine::Declare(const Scenario &scenario)` | `S_->Standing` | yes |
| `bool Engine::Assemble() {` (:139) | the scene store, and the drive | no |
| `bool Engine::Advance() {` (:535) | `if (!S_->Standing) { ... "no scenario is standing" }` | refuses |

`apps/driver/src/main.cpp:126` calls `Read`, and `Declare` only when `--from/--to` were given
(:138). Without a route override the product cannot produce one frame. That is the whole of the
zero-still result, and it is not the route search: the route search is a different, later defect
(board:1862).

## What will be true

- [ ] `Read` stands what it read, or `Assemble` does — one arrival route, as the save path
      already argues in its own refusal at src/engine/Engine.cpp:450.
- [ ] `Assemble()` returning true and `Advance()` refusing "nothing is standing" is unspellable:
      the two agree by construction, not by call order.
- [ ] Proving case: a scenario that declares NO drive is read, assembled, advanced and captured,
      and the still carries the subject.
- [ ] Negative control: remove the stand from the arrival route -> `Assemble` refuses by name at
      the call that failed, never silently.
