Type: issue
Area: gltf
Tags: hygiene

**The gltf headers hold the mechanical bar**

`[[nodiscard]]` is the house rule on every value-returning query, and the gltf door — the
one content surface — misses it across its public readers:

- src/gltf/Document.h:25-56 — `Error()`, `Path()`, `Version()`, `Accessors()`, `Meshes()`,
  `Nodes()`, `MorphWeightsFirst/Count/Total()`, `DefaultScene()` and the rest of the getter
  block carry no `[[nodiscard]]` (the `Read*/World*/View*` bools do).
- src/gltf/Subject.h:110-129 — `Error()`, `PositionsM()`, `Uv()`, `HasUv()`, `Normals()`,
  `Tangents()`, `Colours()`, `Lights()` likewise.

While in there: `Document.h:22-23` takes `const std::string &path` where `std::string_view`
says what is meant (boundaries speak span and string_view — 1621 is the tree-wide item,
this names the gltf instances).

Demanded: nodiscard on every value-returning query in src/gltf headers; string_view at the
path parameters. Purely mechanical, no behaviour change, the gate proves it compiles.
