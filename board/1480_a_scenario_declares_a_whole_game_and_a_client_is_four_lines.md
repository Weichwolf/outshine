Type: feature
State: open
Area: scenario
Tags: scope

# A scenario declares a whole game, and a client is four lines

**Benchmark** — Unreal: a game is a project of assets plus C++/Blueprint; the "client" is the editor and a launcher. RAGE: a game is a build. **Neither ships a four-line client** — outshine does because its content surface is one format and its declaration is one file, and that is the claim this item holds the door to.

The owner's bar: a complete game is practically four lines of client code and one scenario —
which may be as small as a Tetris board or as large as Fallout 4. The client says where to
render, loads the scenario, runs the loop and cleans up; everything else the scenario declares.

```cpp
outshine::Engine engine;
engine.RenderTo(surface);
engine.Read("fallout.scenario");
engine.Run();
```

What a game of that size declares, one row per layer the architecture already names — a row a
scenario leaves out is a layer that does nothing:

| layer | what the scenario declares |
|---|---|
| providers | which upstreams, each pinned, ranked, and what absence hands over to |
| world | the sphere, its fields, the epoch and decay dial (board:1611) |
| generators | which kinds are registered and each kind's parameters — may be none |
| compositors | which are on and each one's budget |
| renderer | the plan: outputs, stages, transfer, exposure, precision |
| content | assets and their placements, materials and variants |
| surfaces | markup, style and script: HUD, menu, terminal — or a whole game that IS the surface |
| actors | kinds, capability surface, mind or model binding, spawn rule, tick rate |
| physics | one system and its dial |
| clock | epoch, time of day, rate, weather |
| input | bindings from a device to DECLARED ACTIONS — the client never sees a keycode |
| state | what survives a save |

A scenario of that size is not one file: an ordered list of packs, each naming what it mounts,
a later record overriding an earlier one — the shape both references converged on independently.

## What will be true

- [ ] The four lines above compile against `include/outshine/` alone and run a declared game.
- [ ] Every row above is reachable from the declaration AND from the assembly API, with the same
      refusal text (board:1583), and ADVANCED by the door (board:1862).
- [ ] An ordered list of declarations composes, and an override that names nothing refuses.
