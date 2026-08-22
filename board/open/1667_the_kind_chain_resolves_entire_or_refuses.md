Type: bug
Area: clients
Tags: assembly, hardening

**The kind chain resolves entire, or the assembly refuses — no default is dropped in silence**

The 1487 cut resolves an instance's traits once at stand-up, which is right. But the merge
has two silent-loss paths, both violating "refuse on existence" and the closing note's own
claim ("refusal on the seventeenth"):

1. **`src/clients/Assembly.cpp:122-127`** — `Entity chain[8]` with `depth < 8`: a kind chain
   nine deep silently drops the root's defaults. The 8 carries no origin, no refusal fires,
   and nothing at kind declaration bounds the depth. Demand: refuse at assembly when the
   chain exceeds the declared bound ("the kind 'x' inherits deeper than N"), the bound named
   `[SET]` beside the array.

2. **`src/clients/Assembly.cpp:131`** — `(void)resolved.Put(...)`: the refusals at the kind
   and instance declaration sites each check their OWN attribute list against `Traits::kMost`
   (16), but the chain UNION is never checked. Kind A declares 10, kind B inherits A and
   declares 10 distinct → the resolved instance silently loses 4 of A's defaults. The
   `(void)` is the tell. Demand: a failed `Put` in the merge is a refusal naming the
   instance and the budget.

Minor, same file: `AssembledCapacity` (Assembly.cpp:8-14) counts `declared.Instances` (a
`std::vector`) by hand-rolled loop — write `declared.Instances.size()`.

The proving test extends `test/unit/clients/AKindIsADefaultAndAnInstanceOverridesIt.cpp`:
a chain past the bound refuses by name; a chain whose key union overflows 16 refuses rather
than resolving short.
