Type: issue
Area: scene
Tags: static-assert, capacity

# The felling stack's bound is a compile-time fact

1731 reserved `Felling_` and `Raising_` at `capacity` entries
(src/scene/Store.cpp:36-44) and the counted-allocator test proves zero heap on
the verbs as the rule table stands. But the bound is a THEOREM about kRules,
nowhere stated: `Remove` pushes one entry per incoming edge of every
`OwnedByTarget` relation (src/scene/Store.cpp:87-94). The reserve of
`capacity` suffices only because

1. exactly ONE relation carries `OwnedByTarget` (ChildOf,
   include/outshine/Register.h:77-84), and
2. that relation is Exclusive, so each entity has at most one owner and is
   pushed at most once.

Add a second owned relation — or make an owned relation non-exclusive — and an
entity is pushed once per owner, the stack outgrows its reserve, and
`push_back` buys heap mid-tick again, with TheRuntimeVerbsBuyNoHeap none the
wiser unless its fixture happens to build that graph.

Demanded: beside kRules, a `static_assert` in the shape of
`EveryAcyclicRelationIsExclusive` (include/outshine/Register.h:100-106) that
pins `every OwnedByTarget relation is Exclusive` and derives the per-entity
owner count the reserve multiplies by — so the day the table grows, the bound
either still holds by proof or the build refuses.

---

Closed -- the theorem is a compile-time fact beside the rules it depends on:
EveryOwnedRelationIsExclusive() static_asserts (naming the reserve that must widen before
it is relaxed), kOwnedRelations is derived constexpr from kRules, and Open reserves
capacity * kOwnedRelations -- a second owned relation now widens the reserve by
construction and the assert refuses a non-exclusive one. Proven by the build and
TheRuntimeVerbsBuyNoHeap standing unchanged.
