Type: bug
State: open
Parent: 1890
Area: engine, compositor, world

# A ring gets the tiles it asks for, and waiting for them does not take away the one it had

`Engine::State::Composes` asks for a 3 by 3 ring and gets ONE tile:

    MEASURES tiles the ring laid = 1 tiles
    MEASURES tiles it is still waiting for = 8 tiles

so the composed ground is a single tile that lands east 287..1102 m of a car standing at 0. It is
a fragment, and the picture shows the horizon where the fragment is not.

## Waiting for the other eight makes it worse, measured

`Composes` retries only while `LayPatchwork` FAILS:

    src/engine/Engine.cpp:324   while (!laid && ... < Declared.Ground.PatienceS) {
    src/engine/Engine.cpp:328     laid = LayPatchwork(Stack.Pool(), over);

and a ring with one tile and eight pending is not a failure, so the loop never runs. Changing the
condition to `(!laid || laid->Pending > 0)` -- wait until the ring is whole -- gives:

    the ground did not compose: no tile of the 3 by 3 ring around 48.139430, 11.575722 meshed
    -- 9 pending

**Thirty seconds of retrying takes the ring from one tile to none.** `TilePool` meshes on worker
threads (`TilePool::Work(int slot)`) and a caller that spins on `Mesh` as fast as it can starves
them or re-queues underneath them. A wait that makes the thing it waits for less ready is not a
wait.

`TilePool` already carries `BytesBlocking` beside `Bytes` for exactly this shape on the byte
path; the mesh path has no twin.

## What will be true

- [ ] A ring is asked for once and waited for in a way that lets it arrive -- the pool is given
      the chance to finish, or the caller blocks on the pool rather than polling it.
- [ ] A ring that cannot be completed inside its declared patience lays what it HAS and says how
      many are missing, rather than refusing everything or spinning.
- [ ] Proving case: a stub `TileMeshes` that answers Pending for N calls and Ready after, and a
      composer that comes back with every tile of the ring inside the declared patience. Negative
      control: the poll as it stands, which comes back with fewer tiles than a single call would
      have given.
