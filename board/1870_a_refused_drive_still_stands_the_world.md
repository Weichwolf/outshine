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

**What the still does not show is a world, and the background is not the fill either.**
Measured at 817ea333: `refused.png` and all nine `along*.png` are RGBA with `alpha == 0`
everywhere the subject is not -- the car occupies 38 k of 921 600 pixels and the other 96 % of
the frame is transparent, not `fill="0.9"` white. The declared fill
(`apps/driver/src/f31.scenario:4`) never reaches the picture; what a viewer shows behind the car
is its own background. No ground, no horizon, no sky, no shadow, and no fill.

## What will be true

- [x] A drive that cannot be laid leaves the scenario STANDING: the frame renders and the still
      is written.
- [x] `Engine::Drove()` answers false, and a client asks WHY without parsing prose.
- [x] No message carries a prefix twice.
- [ ] The DECLARED fill is what stands behind a subject when nothing else does. A still whose
      background is `alpha = 0` has drawn nothing, and *something is always drawn*.
- [ ] Negative control: a route between two disconnected coordinates -> stills are written and
      **they show the world** — ground under the car and a horizon behind it, not a subject on
      the fill colour.
