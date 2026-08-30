Type: task
State: open
Area: door
Tags: store, interface
Parent: 1582

# The scene store passes the door, and Along/Whole go behind it

**Benchmark** — Unreal: `UWorld` is reached through the engine, and its stores are not public data. RAGE: the map is behind the streamer. **Both agree** — the store passes the door as a thing with verbs, never as members.

`Engine::Along()` and `Engine::Whole()` (Outshine.h) spell drive progress at the front door.
RAGE and Unreal both answer that question through the world, never through the engine object:
`GetWorld()`, `fwEntityStore`. The TARGET map already says the same -- `Engine +Scene() Store`.

The move is blocked on the store itself, and the reason is named: `src/scene/Store.h` publishes
its whole private layout in the header -- `struct Slot` with 14 members, `struct Pair`, `NoRefs`,
`kNoRef`, `LinkIn`/`UnlinkIn`/`ErasePair` (Store.h:70-133). Publishing that through `include/`
would put every slot of the graph in the public interface, which is a worse defect than the two
verbs it retires.

So: pimpl the store first, then `Scene()` lands, then `Along`/`Whole` are questions the client
asks the graph. Not before.

Proving test when it lands: a corpus case that reads drive progress through `Scene()` and never
names `Along`. Negative control: the same case against a door that still publishes `Along`.
