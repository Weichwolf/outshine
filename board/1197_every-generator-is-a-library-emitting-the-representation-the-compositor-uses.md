Type: feature
State: open
Area: generators
Tags: scope, contract

# Every generator is a library emitting the representation the compositor uses

One in-memory model, not two: a subject loaded from a file and a subject grown by a generator
are the SAME type, which makes *the compositor never learns what produced a part* structural
rather than conventional. The pieces exist — `Gltf::Document` and `Gltf::Subject` are the
representation, `src/gltf/Emit.h` is the emitter and is held by a fixed point
(`Subject(Emit(S)) == S`), and the generators are there — so this is a convergence, not a new
stack.

## What will be true

- [ ] ONE interface every generator implements, taking `(kind, params, seed, budget)` and
      replying a part PLUS its capability.
- [ ] The reply is the internal representation, never bytes: a `.glb` is a serialisation for
      something outside this engine and never the currency between a generator and a compositor.
- [ ] Each generator compiles as its own library, so a test links the one it is about and a
      generator's include set is provable the way a layer's is (board:1582).
- [ ] A test drives a generator directly and scores what comes back, with no preparer and no
      Blender in the path for the parts of the claim that need no oracle.
