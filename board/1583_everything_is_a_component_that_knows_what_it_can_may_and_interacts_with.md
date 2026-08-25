Type: feature
State: open
Area: core
Tags: scope, component-model

# Everything is a component that knows what it CAN, what it MAY, and what it INTERACTS with

Content is assembled from components by rules — load an asset AS a driveable four-wheel, attach
a mind, attach an assignment, hand it navigation as a tool — declarable in scenario XML and
composable in C++ through the SAME calls against the SAME graph.

The reference design is decided (Flecs: relationships, traits, script parity; GAS tag algebra;
Smart-Object slots; CARLA's vehicle grammar) and written here, never depended on:

| piece | mechanism |
|---|---|
| INTERACTS | a connection is a VALUE `(relation, target)` in one id — no pointer, no string, contiguously queryable |
| MAY, structural | traits live ON the relation: `DrivenBy` is exclusive, `OneOf(minds)`, `Drives` requires `With(nav)` — an illegal edge is REFUSED AT ASSEMBLY |
| "as four-wheel" | prefab + `IsA`: a prefab is an entity, instantiation copies its subtree, variants inherit, named slots reach the instance's own children |
| MAY, situational | set algebra over one hierarchical tag vocabulary (board:1764) |
| CAN | capability tags from the same catalogue; world objects ADVERTISE as data |
| shared interactions | slot state machine Free -> Claimed -> Occupied -> Free; without Claim two agents converge on one fuel pump |

Banned by the sources themselves: stringly-typed capabilities, content that ships a program, god
actors, ECS-for-everything, unreserved shared affordances.

## What will be true

- [ ] `Store` carries entities, typed pairs, traits and tags, and the XML reader is a
      SERIALISATION of the assembly API against that one graph — no second representation, no
      converter.
- [ ] An illegal assembly refuses at ASSEMBLY with the same sentence from either door.
- [ ] A prefab instantiates, a variant inherits, a slot is claimed and released.
- [ ] Proving test: one scenario declared twice — once in XML, once in C++ — produces the same
      graph, byte for byte.
