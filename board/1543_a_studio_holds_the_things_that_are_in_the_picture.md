Type: feature
State: active
Area: clients
Tags: scope
Depends: 1536

**A studio holds THE THINGS that are in the picture, and one is its degenerate case**

`Clients::Studio` carries `const Gltf::Subject *Geometry` -- **one pointer**. `Live::Build` copies
`*Declared_.Built` into a single `Geometry_`, `Live::Stand` points the studio at it, and
`GltfStudio::Place` refuses with *"the studio declares no subject"* when it is null. So the picture
holds exactly one thing.

**That is why the driver's window draws a road with no car in it.** The corridor is the one subject;
the F31 cannot also be one. `board:1536` gave a draw its own model transform, which is the half that
lets two things stand in different places -- this is the other half.

## What must be true

- [ ] **`Studio` holds 1..N placed subjects**, each with the `Model[16]` a draw already carries.
      *A shape is 0 or 1..N*, and the current shape assumes exactly one
- [ ] **The road, the car and a building stand in one picture** -- which is the first frame anybody
      would call a game
- [ ] **A subject enters and leaves without the others being re-stood.** `Live::Restand` replaces the
      whole geometry today, which is right for one thing and wrong for a street
- [ ] **Headlights and the cabin's lights ride on the car**, as the `Gltf::PlacedLight`s that
      `Studio::Lights` already gathers from `Geometry_.Lights()` -- the mechanism is built and has
      one subject to gather from
- [ ] **The picture bound does not move** for a scene of one subject at identity

## Comments

**The lights are the part that is already solved and worth saying so**: `Stand()` walks
`Geometry_.Lights()` and pushes each `PlacedLight` into the studio, then adds the declared key on top.
A car with headlights is a car whose subject carries two spot lights -- no new concept, no new
interface. What is missing is only that the car cannot be in the picture at the same time as the road.

**Measured, not recalled**: `src/clients/GltfStudio.cpp:325-336` refuses on a null or empty subject;
`src/clients/Live.cpp:216-226` builds the studio from one `Geometry_` and gathers its lights.
