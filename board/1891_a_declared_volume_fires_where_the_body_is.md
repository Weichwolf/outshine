Type: bug
State: open
Parent: 1862
Area: clients, scenario
Tags: measured, drive, origin

# A declared volume fires, because it stands where the bodies do

`TriggerField` is reached: `Engine::Declare` stands one from `scenario.Volumes` and
`scenario.Events`, refusing by name if it does not stand, and `Engine::Rides` probes the driven
body against it every tick and drains what fired.

**Nothing fires.** Measured 2026-08-25 with a scenario declaring one box at the origin with
`extentX/Y/Z = 1e7` and `when="enter"` -- a volume the body cannot be outside of -- over the
136 m Munich drive:

```
CARRIES 1 trigger volumes
CARRIES 1 declared events
(no volume fired)
```

The body's position is corridor-relative and the volume's is the scenario's own origin, so
`Inside(door, atM)` answers against two different spaces. This is the SAME seam as board:1890:
the ground ring is anchored on its ECEF origin, the volume on the declaration's, and the drive
on the corridor's.

The grammar refused two spellings on the way, correctly and by name -- `<volume>` requires
`fires`, `<event>` requires `name` -- which is the door doing its job on a shape nothing had
ever declared before.

## What will be true

- [ ] A volume declared at a place the body passes FIRES, once per crossing, and the fired
      event reaches something that acts on it. Today it reaches `Carried()`, which is a report
      and not an actuator.
- [ ] And it reaches it WITHOUT allocating on the tick path. `Carried.push_back("a volume fired
      event " + std::to_string(...))` (src/clients/Engine.cpp:907-908) builds a string and grows
      a vector nobody drains, inside `Engine::State::Rides`, which runs at 60 Hz. It is
      invisible today only because nothing fires; the hour a volume works, the drive allocates
      per tick and the vector grows for the length of the route.
- [ ] The space a volume is declared in is NAMED: the scenario's origin, a region, or the body
      it is attached to (`Volume::In` exists and nothing reads it).
- [ ] Proving case: a drive through a declared box reports exactly one enter and one exit, at
      the along-distances the box's extent implies. Negative control: move the box off the
      route and the count is zero.
