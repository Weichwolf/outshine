# A rebuild never happens inside a frame

State: open

Goal 3 asks for 0 of 120 frames over 16.7 ms. Measured 2026-09-04 on Kaiserberg:

```
  rebuilds since the world stood            4
  and what the last rebuild took       2210.8 ms
  the step's own time, most            2219.0 ms
```

**The rebuild IS the frame.** `Engine::State::Updates` ends with `Grounds(false)`, called every
frame, and when the eye crosses into another tile that call meshes the terrain, the buildings, the
streets and the water inline. p99 across the places follows exactly:

```
  Shibuya 8443 ms   Kaiserberg 2088   Koehlbrand 2019   Heidelberg 1427
  Venice   999      OldTown     719   Jura         10   CentralPark   4   ZurichPlan 6
```

The four worst are four rebuilds landing in four frames. No amount of making the rebuild cheaper
fixes this: 2.2 s cut by ninety per cent is still 220 ms in one frame, which is thirteen frames
missed.

## CLAUDE.md already decided this and the code does not obey it

> **FOUR THINGS RUN INDEPENDENTLY -- SIM · VIDEO · AUDIO · IO -- and what passes between them is a
> SNAPSHOT.** The simulation owns the world and hands the renderer a delta; the renderer draws a
> frame behind and never reaches back.

A rebuild is the simulation's work. It is being done on the frame path, which is the defect the
invariant names: the renderer is reaching back into work instead of being handed a result.

## What Unreal does, what RAGE does

Both stream asynchronously and swap when ready, and NEITHER blocks a frame on it. Unreal's
`FIoDispatcher` and its level streaming build off-thread and register the result; RAGE's streaming
threads sit beside `sysTaskManager` for the same reason -- CLAUDE.md cites both in the invariant.
The old geometry keeps drawing until the new geometry exists, which is also why neither engine
pops: the swap happens at a frame boundary, complete.

## What will be true

- `Grounds` runs off the frame path, and the frame draws the last completed world
- a rebuild's cost never appears in `the step's own time`
- 0 of 120 frames over 16.7 ms on all nine places, which is the goal's own bar

## What will show I was wrong

`the step's own time, most` on each place. Today 2219 ms on Kaiserberg against a 16.7 ms budget.
If moving the rebuild off the frame leaves p99 high, the cost is somewhere else and this item
misread it.
