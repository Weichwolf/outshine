Type: issue
Area: clients
Tags: layering

**The engine's state is a door, not a warehouse**

The owner read Engine::State (src/clients/Engine.cpp) and named it: practically everything in
it violates the target architecture -- and he is right. The inventory:

| member | verdict |
|---|---|
| `Render::Renderer Device` | the device lives INSIDE the facade -- the renderer belongs behind the plan, owned by the running world, not the door |
| `std::unique_ptr<Clients::Live> Standing` | Live is itself a red god-facade (the adjudication); folding it INTO Engine doubles the sin -- 1582 folds it away instead |
| `Extent Frame` | picture geometry in the door; belongs to the declaration/plan |
| `Scenario Declared + Carried + Asleep` | scenario bookkeeping (parked scenarios!) in the facade; Carried is honest telemetry, Asleep is a feature squatting in State |
| `Store Scene + Columns + Assembled` | the graph -- the one part that BELONGS: the door owns the one graph |
| `std::string Error` | fine -- a door reports its refusals |

Target: Engine = the thin door -- declaration in, graph owned, systems advanced, frame
rendered out; Device/Live/Frame dissolve with 1581 move (e) and 1582's fold. Grown during the
door work (Assemble/Scene/Columns landed here first); acknowledged before the reviewer had to
say it.

Depends: 1581
