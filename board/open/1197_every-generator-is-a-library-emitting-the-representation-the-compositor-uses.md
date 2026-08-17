Type: feature
Area: generators
Tags: scope, instrument

**Every generator is a library emitting the representation the compositor uses**

Owner's ruling: *every generator must have an interface and emit data in a glTF internal representation,
so the tests can use the generators like a library — compiled as libraries that all implement the same
glTF interface.* And the clarification that decides its shape: **the glTF internal representation is what
the compositor and the scene graph use.**

**So there is one in-memory model and not two.** A subject loaded from a file and a subject grown by a
generator are **the same type**, which makes `CLAUDE.md`'s *the compositor must never learn what produced a
part* **structural rather than conventional** — today it is a rule nothing enforces, and under this it has
no spelling to break.

## What already exists, so this is a convergence and not a new stack

- [ ] **`Gltf::Document` and `Gltf::Subject` are the representation.** The reader produces them, the
  flatten walks them, `Clients::Show` draws them. Nothing else needs inventing to name the target
- [ ] **`src/gltf/Emit.h` is the emitter and it is already held by a fixed point** — `Subject(Emit(S)) == S`,
  proven by `test/outshine/unit/gltf/EmittingASubjectIsAFixedPointOfTheFlatten.cpp`
- [ ] **`test/harness/outshine/render/prepare/GrowPart.cpp` is this feature in embryo**: it links the
  library, runs a generator and writes a `.glb`. It is one generator, one shape, an offline program and
  not an interface — but it demonstrates the whole path already runs
- [ ] **The generators are there**: `src/generators/draw/` grows the parts, `Forest`, `Buildings`, `Water`,
  `Ground`, `Infrastructure` compose them

## What must become true

- [ ] **One interface every generator implements**, taking `(kind, params, seed, budget)` and replying a
  part **plus its capability** — which is `board:0055`'s contract, still unticked and still the spine.
  **This ruling and `0055` are the same requirement seen from two ends**, and neither closes without the
  other
- [ ] **The reply is the internal representation**, not bytes. A `.glb` is a serialisation of it for the
  preparer's benefit and never the currency between a generator and a compositor
- [ ] **Each generator compiles as its own library**, so a test links the one it is about. That is what
  *the tests can use the generators like a library* asks for, and it is also what makes a generator's
  include set provable the way `test/outshine/unit/` proves a layer's
- [ ] **A test drives a generator directly** and scores what comes back, with no preparer and no Blender in
  the path for the parts of the claim that do not need an oracle

## What it would settle that is open today

**`board:1190`** wants the preparer to generate a material and an image; under this the fixture is a
generator like any other and the preparer stops being the place that knows how. **`board:1118`** wants
`grown.SHAPES` to carry a name per generator kind — that list is a registry of exactly this interface.
**`board:1186`** and **`board:1179`** both wait on a fixture nobody can build cheaply, and both are the
same prerequisite this feature supplies.

## What it costs, stated rather than discovered

**DECIDED by the owner: a generator replies a `Subject`, and that can be serialised to glTF where it is
required.** The question this section raised — `Document` or `Subject` — is answered, and the answer
removes the cost the question was about.

**`Gltf::Subject` is already the flattened, frame-side form**: it carries parts with their vertex ranges
and material slots, it is what `Clients::Show` consumes, and it is what the draw list is compiled from. So
**no interchange model stands in the hot path**. glTF's accessors, buffer views and node hierarchy are a
*serialisation* the emitter produces when something outside this engine has to read the part — the
preparer's `.glb` for Blender, an export, a fixture — and they are never the currency between a generator
and a compositor.

**That also makes the emitter's existing guarantee the feature's own.** `Subject(Emit(S)) == S` is proven
today, so *serialise where required* is not a path anybody has to trust: it is a fixed point with a test
on it, and a generator's reply can be written out and read back without becoming a different subject.

**Done when** every generator kind is reachable through one interface, replies the internal representation
with a capability, compiles as a library a test can link alone, and at least one test drives a generator
directly rather than through the preparer.
