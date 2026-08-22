Type: feature
Area: core
Tags: scope

**Everything is a component that knows what it CAN, what it MAY, and what it INTERACTS with**

The owner's direction (2026-08-22): content is assembled from components by rules -- load an asset
AS a driveable-four-wheel, attach a mind, attach an assignment, hand it navigation as a tool. The
same assembly must be declarable in the scenario XML and composable in C++ with the same
simplicity. Every component states its capabilities, its permissions, and its typed connections.

## The reference design (researched, decided with the owner)

**Flecs is the reference** -- the entity-relationship model with traits, and the Flecs-Script
parity principle. Written ourselves; Flecs is the blueprint, not a dependency.

| piece | reference | mechanism |
|---|---|---|
| INTERACTS -- typed edges | Flecs relationships | a connection is a VALUE `(relation, target)` in one id -- no pointer, no string, hashable, contiguously queryable |
| MAY (structural) | Flecs traits | rules live ON the relation, once: `DrivenBy` is `Exclusive`, target `OneOf(minds)`, `Drives` requires `With(nav)`; an illegal edge is REFUSED AT ASSEMBLY |
| "as four-wheel" | Flecs prefab/IsA | a prefab is an entity; `IsA` instantiates its subtree; variants inherit; slots name the instance's own children (driver seat) without string lookup |
| MAY (situational) | Unreal GAS | set algebra over one central hierarchical tag vocabulary -- a `constexpr` catalogue here, so a misspelled tag is a compile error |
| CAN | GAS + Smart Objects | capability tags from the same catalogue; world objects ADVERTISE as data (activity tags, user filter, slots) and the agent's own runtime executes |
| shared interactions | UE Smart Objects / The Sims | slot state machine Free -> Claimed -> Occupied -> Free; without Claim two agents converge on one fuel pump |
| the vehicle grammar | CARLA / SUMO | kind + attribute delta + attach + agent + NAV AS A QUERYABLE SERVICE + route |

**The parity law** (why Flecs Script and Godot scenes hold while Unity baking rots): the data
format is a SERIALISATION OF THE SAME API OPERATIIONS against ONE graph. The XML reader calls the
assembly API the C++ client calls -- one checker, identical refusal text from both doors. XML can
declare exactly what the API can, and nothing of its own: no conditions, no control flow --
behaviour belongs to the mind and the assignment, never to the file.

**The boundary**: the render plan keeps its `constexpr` catalogue -- it already has this model
with stronger guarantees (unspellable beats refused-at-load). The component graph is the language
of the SCENE domain: vehicles, minds, tools, assignments, world objects. Same principle
everywhere; no universal graph where a LUT hangs beside an AI.

## What must become true

- [ ] the entity store holds values: ids, typed pairs, traits -- no pointers, no strings
- [ ] the tag catalogue is `constexpr`; capability and permission checks are set algebra over it
- [ ] traits refuse illegal assembly loudly, with the same text from XML and C++
- [ ] prefab/IsA expresses "glTF as four-wheel"; the F31 declaration becomes one
- [ ] world objects advertise slots with claim/use/release
- [ ] the actor chain (board:1581) is the first assembly expressed in this model: F31 `IsA`
      four-wheel, `DrivenBy` -> mind, nav as tool, assignment = route -- Journey's fold into Sim

## Anti-patterns the sources themselves name (banned here)

stringly-typed capabilities · content that ships a program · god actors · ECS-for-everything ·
two representations with a converter (the Unity-baking rot) · unreserved shared affordances

Depends: 1581

---

**Learned, slice 1 (store · traits · tags).** `src/scene/` stands as a dependency-free brick like
`src/corridor`: `Register.h` is the constexpr catalogue (four kinds; five relations each carrying
its rule -- Exclusive, Acyclic, target-kind mask, required source capability, required companion
relation; hierarchical tags with prefix algebra, wrong matches are compile errors), `Store` is a
generation-checked pool that refuses illegal wiring at assembly with a text naming the rule.
Proving tests: `unit/scene/TheRuleOnARelationRefusesTheWiringItForbids` (second driver, wrong
kind, inert source, assignment-before-nav, cycle -- all refused; removal frees the exclusive
seat) and `unit/scene/ATagIsAValueAndMatchingIsPrefixAlgebra` (capability flows down IsA by
query, not copy). `make` picked the layer up with no Makefile change -- board:1584's one
declaration at work. Open: prefab subtree instantiation, slots/claim, the XML door through the
same API, and the fold of the F31 declaration into a prefab.
