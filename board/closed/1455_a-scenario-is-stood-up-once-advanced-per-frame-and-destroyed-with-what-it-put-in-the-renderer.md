Type: feature
Area: scenario
Tags: perf, scope

**A scenario is stood up once, advanced per frame, and destroyed with what it put in the renderer**

A consumer names a declaration and gets a **live scenario**: it stands itself up against a renderer,
it advances a frame at a time, and destroying it removes everything it put there. Selecting a second
one is `live = Open(other)` and nothing else — no plan to rebuild by hand, no mesh to clear, no
camera to reset, and no way to leave the previous one's geometry behind.

**THE ENGINE ALREADY SAYS THIS AND THE SENTENCE WAS NOT ACTED ON.** `Clients::Aim`'s header carries it
in full: *"the geometry is set up once and the eye moves every frame, and a caller that had to restate
the whole studio to turn the camera would rebuild the mesh sixty times a second."* The browser restates
the whole studio every frame, which is the case that comment describes and the reason it exists.

## What it costs today, measured on this device at 1600x900

`--show <case> --frames 120`, timing the submission and the frame separately.

| case | submit p50 | submit p99 | frame p50 |
|---|---|---|---|
| `Box` | 5.491 ms | 151.024 ms | 0.000 ms |
| `WaterBottle` | 124.397 ms | 139.530 ms | 0.000 ms |
| `ABeautifulGame` | **1174.451 ms** | 1267.866 ms | 0.000 ms |

**0.85 frames a second for a body that is not moving**, and the frame itself is free. Every millisecond
is the re-packing and re-upload of vertices, indices, materials and textures that were already on the
device. `Box`'s p50 of 5.5 ms against a p99 of 151 ms is the texture path: most frames re-pack, some
re-upload.

## What it costs now, same device, same population, same instrument

`--show <case> --frames 120`, timing what the consumer does per frame -- which is now `Advance` whole.

| case | before, submit p50 | after, advance p50 | after, p99 |
|---|---|---|---|
| `Box` | 5.491 ms | **0.140 ms** | 0.340 ms |
| `WaterBottle` | 124.397 ms | **0.160 ms** | 0.355 ms |
| `ABeautifulGame` | 1174.451 ms | **0.411 ms** | 0.752 ms |

**The worst case went from 0.85 frames a second to 4.5 % of the frame budget at p99.** The number is
not an optimisation: nothing in the submission got faster, it stopped happening. `Advance` is now the
frame encode and, for an animated subject, one pose.

## What must be true

- [x] **A subject is submitted when it CHANGES and at no other time.** A still pose costs one submission
  for its whole life on screen
- [x] **Destroying a scenario removes its subject from the renderer.** The defect this closes is visible:
  select a model, then select a document, and the model is still drawn behind it — because the mesh
  outlives the thing that set it and a zero picture region means *the whole surface*
- [x] **A glTF file is a generator kind and not a scenario special case**, per `CLAUDE.md` — the
  declaration names `kind = gltf-file` and the runtime has no second arrival route for it
- [ ] **A declared surface and a bound script are the same mechanism**, so a document case and a program
  case are scenarios rather than viewer branches
- [x] **The consumer draws nothing.** It owns the window, the input and its own interface; the frame is
  the scenario's

## What this item may NOT do

**It may not move the declaration reader.** `src/scenario` compiles against `src/core` alone and
`test/run.sh` proves `Renderer.h` has no spelling in it. The runtime is a different layer and stays one.

## What the round found that it was not looking for

**MOST KHRONOS ASSETS DECLARE NO LIGHT, and a scenario that believed them drew a black body.** They are
not unlit: their authors expected an environment. The first stand-up was correct about a picture nobody
can see -- 0 samples of ink where the subject was -- so **a scenario declares its own lighting** and the
studio arm `CLAUDE.md` already describes is where it belongs. With a key light and an ambient the same
measurement reads 141 samples with a body and **0 with none**, which is both claims in one number.

**THE TEARDOWN DEFECT AND THE COST DEFECT ARE ONE OMISSION SEEN FROM TWO SIDES**: a mesh outlives
whatever set it. From the front that is a body still drawn behind every document shown afterwards; from
the back it is a body re-uploaded sixty times a second because nobody owned it.

## Comments

The measurement above is the first time the browser's frame cost was attributed. It was found by
opening a case rather than by reading the code: the suite renders one frame per pose, so the harness
never pays this and never could report it.

## Comments

Closed: the claim is measured true in this body -- Advance whole is 0.411 ms p50 on the worst case against 1174 ms of per-frame resubmission before, and teardown removes the subject; the one open box (a document case as a scenario) belongs to board:1480's declarative line, not here.
