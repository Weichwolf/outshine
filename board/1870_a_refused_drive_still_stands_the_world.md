Type: bug
State: open
Area: clients
Tags: driver, door, product, measured

# A declaration that refuses does not take the picture with it

**Three quarters paid, measured 2026-08-25 at a3ebe3e0.** The same route that left zero stills at
1af2c00b now leaves a picture:

```
build/outshine-driver --headless --from 48.137,11.576 --to 48.200,11.600 --into DIR
REFUSED the network holds both ends but no chain of ways joins them -- 19406 nodes of 65615
were reachable from the start, so this is a network in pieces and not a search that gave up
NO DRIVE -- the picture is what stood without it
KEPT DIR/refused.png -- a failure is loud, and something is always drawn
```

The refusal reaches the door as a VALUE — `void Refuse(const std::string &why) override {`
(src/clients/Engine.cpp:37) — where it used to be prose the door grepped, so the doubled
`REFUSED REFUSED` is gone with the grep that caused it.

**What the still does not show is a world.** `refused.png` is the car on `fill="0.9"` white: no
ground, no horizon, no sky, no shadow. The frame survived the refusal; there was nothing in it
but the subject, because nothing calls the composition path (board:1805). The last box stays
open until a refused drive shows the world it refused to drive through.

## What will be true

- [x] A drive that cannot be laid leaves the scenario STANDING: the frame renders and the still
      is written.
- [x] `Engine::Drove()` answers false, and a client asks WHY without parsing prose.
- [x] No message carries a prefix twice.
- [ ] Negative control: a route between two disconnected coordinates -> stills are written and
      **they show the world** — ground under the car and a horizon behind it, not a subject on
      the fill colour.
