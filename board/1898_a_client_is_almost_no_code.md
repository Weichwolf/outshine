Type: issue
State: open
Area: door, apps
Tags: interface, measured
Depends: 1896

# A client is almost no code, and the line count measures the door

A client's length is a measurement of the interface, not of the client. When a client needs much
code, outshine's door is too complicated and the door is what wants fixing.

Measured at HEAD:

| client | lines | what it should be |
|---|---|---|
| `apps/driver/src/main.cpp` | 223 | read a scenario, stand it, step it, keep stills |
| `apps/viewer/src/main.cpp` | 349 | read a directory, build a scenario, swap the subject on a callback |

The viewer's own shape, as the intended flow states it: read the directory, build the html/js,
build the scenario, attach the first glTF, SDL init, register callbacks, show, replace the glTF
in the scenario on callback, quit on quit. That is roughly 100 lines. The 249 over it are:

- the scenario is REBUILT INLINE on every case switch, about 25 lines duplicated in the failure
  path, because the door offers no "swap this asset" verb and `Shows(surfaces)` is reached only
  through a full re-declaration
- lighting constants -- 40000 lux, 42 deg elevation, 150 deg bearing -- are spelled in the CLIENT
  at two sites, so a client that wants a lit scene must know how to light one
- the fixed-timestep loop is hand-written in the client: accumulate, clamp to a most-steps bound,
  discard the surplus. Every client that wants a stable step must write that same loop

The driver's 223 have their own version of the same three.

Each of those is a door defect with a name: no asset-swap verb; no shipped default lighting a
scenario can select from the catalogue; no stepping the engine can drive when the client hands
it the wall clock.

Proving test when it lands: `apps/viewer/src/main.cpp` under 120 lines and `apps/driver` under
100, with the viewer still switching cases and the driver still keeping stills along the drive.
Negative control: the same count taken at this commit.
