Type: issue
Area: scene
Tags: hot-path, allocation

**The store's runtime verbs never allocate**

1721 replaced the recursion with explicit stacks and 1722 made LITERAL refusals aliasing —
both right — but the runtime verbs still buy heap on the tick:

- src/scene/Store.cpp:69 — `Remove` builds a fresh `std::vector<Entity> felling` per call.
  Despawn is a runtime verb in a world of thousands of actors; the work stack is bounded by
  `Capacity()` and belongs to the store, reserved once in `Open`.
- src/scene/Store.cpp:374 — `Instantiate`'s `raising` list likewise; presence
  materialisation evaluates prefabs at runtime rungs, so this is not assembly-only.
- src/scene/Store.cpp:215-243 — `Permit`'s composed refusals
  (`std::string(Named(how)) + ...`) allocate on every refused `Link`/`Relink`. Relink is
  "take the wheel" — a runtime seam. The refusal texts are a finite catalogue over
  (relation, role) and can be precomposed at compile time or written into one member buffer.

Demanded: member scratch (stack + refusal buffer) opened with the pool; a proof that
`Remove`, `Instantiate` and a refused `Relink` perform zero allocations after `Open`
(counting allocator or the harness's sanitiser hook).
