Type: feature
Area: core
Tags: scope

**Everything is a component that knows what it CAN, what it MAY, and what it INTERACTS with**

**The owner's direction, 2026-08-22, across six messages -- the composition model for the whole
engine:**

- content is assembled from components by RULES: load an asset AS a driveable-four-wheel, attach
  an intelligence, attach an assignment, hand it OSM navigation as a tool. Illustration (not to
  be implemented literally): `scene->add(glTF as four_wheel)->attach(ai)->give(osm_nav)->
  drive(route(munich, hamburg))`
- this holds for EVERY engine component: declarable in the scenario XML AND composable in code
  with the same simplicity -- one model, two surfaces
- top-down, every level knows what is available below it: each component states what it CAN
  (capabilities), what it MAY (permissions), and what it can INTERACT with (typed connections)
- **research first**: the field has excellent answers to the game-engine/world-simulator
  composition problem; do not implement naively

## What must become true

- [ ] the reference study is done and its findings recorded here: which shipped models answer
      capability/permission/interaction composition (ECS relationships, Unreal
      component/subsystem + Smart Objects + GAS, affordance models, capability security), and
      which pieces map onto outshine's existing seams (catalogue, scenario grammar, functions
      seam of board:1581)
- [ ] one composition model, two surfaces: scenario XML and C++ assemble the SAME graph
- [ ] a component declares capabilities, permissions and interaction types queryably -- the
      compiler or the declaration refuses an illegal wiring, never a runtime surprise
- [ ] the actor chain (board:1581) is expressed IN this model; Journey's fold into Sim is its
      first proof

Depends: 1581
