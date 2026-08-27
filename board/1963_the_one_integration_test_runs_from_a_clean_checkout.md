Type: bug
State: active
Parent: 1953
Area: client

# The one integration test runs from a clean checkout

**Benchmark** — Unreal: an automation test runs from a clean sync. RAGE: builds from a clean branch. **Both agree** — a test that needs a warm machine is a test of the machine.

`apps/driver` is outshine's one integration test and the picture is judged on the stills the driver takes itself on it.
It cannot run. `apps/driver/src/f31.scenario` declares `scene.gltf`, and no such file is in the
tree:

    REFUSED scene.gltf: cannot be opened

So the client that CLAUDE.md calls the proof of the door proves nothing from a fresh clone, the
owner's screenshots come from a fixture only one machine has, and board:1953's own closing
condition -- *`apps/driver` renders through the rebuilt path and its picture is judged on the stills it takes itself* --
cannot be met by anyone reading this repository.

**Both benchmarks ship a runnable scene with the engine.** Unreal's templates open and play on a
clean install; RAGE ships its map. Neither expects the reader to supply the content that
demonstrates the engine.

The car does not have to be the car. What the drive needs is a vehicle-shaped glTF with the parts
the declaration names -- a body and four wheels -- and the Khronos sample assets do not carry one
that fits (a substitute refused with *the studio declares 2 emitted radiances over a subject of 1
drawn primitives*). So the shipped drive gets a vehicle outshine GENERATES, which is the honest
answer here: the generator tier exists, a generated car is content = data, and it costs the
repository nothing to carry.

- [ ] `make && ./build/outshine-driver --headless --frames 60` runs from a clean checkout
- [ ] the drive it runs is declared in the tree, assets included, and needs no fetch to start
- [ ] the owner's screenshots come from that drive
