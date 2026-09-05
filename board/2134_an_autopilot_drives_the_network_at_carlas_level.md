Type: feature
State: open
Area: actor, engine
Tags: architecture, owner, last
Depends: 2133, 2127, 2130

# An autopilot drives the network at CARLA's level -- and it is the LAST item on this board

**Benchmark** -- Neither Unreal nor RAGE ships a driving agent as engine: RAGE's traffic AI is
game code above `CVehicle`, Unreal's is a plugin or the project's own. The reference for THIS
question is CARLA, whose `Traffic Manager` and behaviour agents are the published, readable
standard for an autonomous driver over an OpenDRIVE-style network -- waypoints, lane changes,
traffic lights, collision avoidance, a global and a local planner. **The choice is mine**: the
agent is a SUBJECT a scenario declares, built on the engine's network (board:2133), its physics
(board:2127) and its snapshot (board:2130), and it lands after all three.

## Where it stands, measured 2026-09-04

The tree's previous autopilot -- about 2700 lines reached from `Advancing.cpp` and nowhere
else -- was removed in board:2117 as not fit to keep. Nothing plans, steers or paces a vehicle
today, and nothing should until the network it would drive on exists.

## The solution, when its turn comes

| CARLA | here |
|---|---|
| global route planner over the OpenDRIVE map | `Route(from, to, Vehicle)` over board:2133's graph |
| local planner: waypoint buffer, PID lateral and longitudinal | a lane follower on the edge's centreline with the class's speed limit, on the physics step |
| behaviour agent: cautious / normal / aggressive | one declared parameter set per scenario, never a hard-coded profile |
| traffic manager: many vehicles, hybrid physics | bodies far from the eye are advanced on the graph without contact; near ones on the physics -- the same rung idea as LOD |
| collision avoidance and traffic lights | a lookahead over the graph's occupancy, which `OccupancySink` already models for placement |

## What will be true

- [ ] A scenario declares a vehicle and a destination and the vehicle arrives, on the road,
      obeying direction and lane
- [ ] Ten vehicles at once hold the frame budget; a hundred at the graph rung
- [ ] The driver is deterministic: the same declaration replays the same drive frame for
      frame, which is RAGE's replay and this tree's invariant
- [ ] Negative control: remove the graph and the driver refuses to be declared

## What will show I was wrong

If the physics step cannot hold ten contacting vehicles at 16.7 ms, the graph rung has to
carry more of them and the crossover is measured before the traffic manager is written.

## Cited 2026-09-05: CARLA is the baseline for the driver, never for the picture

Decided with the owner: CARLA and its Digital Twin Tool are the templates for the OSM half
(board:2101, board:2133) and for the driver (this item); the look stays measured against RAGE
and Unreal. The `Traffic Manager` is adopted as a baseline and corrected where a measurement
of this tree says so -- a reference is a floor, never a ceiling.

## Cited 2026-09-05: what the autopilot drives on

Board:2133 decided it: the driving surface is the map's C1 elevation profile queried
analytically, errors well under a centimetre by the owner's requirement; the road mesh is
the picture and never the contact.
