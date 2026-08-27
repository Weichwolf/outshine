Type: issue
State: active
Parent: 1953
Area: engine

# The door's implementation is a coordinator, not a bag of state

`Engine::State` in `src/engine/Engine.cpp` carries **42 data members and 3 methods**, and the file
around it is 1447 lines with 46 free functions. Board:1974 dissolved `Live` on one test -- do the
members share a lifetime and a reason to change? -- and `Engine::State` fails it in seven places:

| concern | members |
|---|---|
| the picture | `Device` `Standing` `Frame` `Shown` |
| the declaration and its input | `Declared` `Carried` `Asleep` `LayerTrace` `Under` `Views` `Bound` `Pump` `Pumping` `Volumes` |
| the scene | `Scene` `Bodies` `Drives` `Kinds` `Stood` |
| the world it draws from | `Wire` `GroundTiles` `Stack` `Shipped` `Making` |
| the simulation | `Drive` `Freestanding` `Surface` `Drove` `OwedS` `MostSteps` `Steps` |
| the measures it publishes | `Numbers` `Standing_Placed` `Fired` `Error` `Offered` |
| the face | `Face` |

**And one name carries two verbs.** `State::Places(const char *, double, const char *)` publishes
a MEASURE; `State::Places(const Physics::Body &, const double[3])` places a BODY. Overloading is
for input flexibility -- the same act reached different ways -- not for two acts that share four
letters. `Rides()` calls the second and reads as though it called the first.

- [ ] the measures are their own thing: a ledger that holds what the engine published, keeps the
      declaration's own numbers apart from the frame's, and answers `Numbers()`
- [ ] `Places` names one act. The other says what it does
- [ ] the simulation's seven members are one thing the engine ticks
- [ ] `Engine::State` holds subsystems and coordinates them, and its member count says so
- [ ] proof: `apps/driver` drives and keeps its stills, `outshine/door` stays 27 of 27
