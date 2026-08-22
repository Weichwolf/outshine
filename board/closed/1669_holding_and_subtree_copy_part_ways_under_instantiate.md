Type: issue
Area: scene
Tags: architecture

**Holding and subtree-copy part ways under Instantiate — one relation, two cascades, and
the collision is adjudicated**

1487 decided "holding is one ChildOf" — inventory and placement as one mechanism is sound.
But `Store::Instantiate` (`src/scene/Store.cpp:342-358`) CASCADES over ChildOf: it copies
every ChildOf child of what it instantiates, recursively. That cascade was written for
prefab subtrees (named slots, the component model's design). Under the 1487 cut the same
relation now also means containment between INSTANCES, and the two meanings diverge:

- `Instantiate(room)` on a room instance copies every occupant standing `in` it — cloning
  a garage clones the cars parked there. For a prefab slot that copy is the point; for a
  placement it is wrong.
- `CopyOf(instance, prefabChild)` (`Store.cpp:362-372`) resolves a child by its IsA target;
  a HELD item whose kind coincides with a slot's prefab is indistinguishable from the
  structural part.
- Removal cascades owned-by-target (the 1487 closing note cites it): dropping a table
  removes the cup on it — defensible for inventory, but the same rule then also deletes
  everything standing `in` a removed region.

Today's assembly order hides this (holds/in links land after all instances stood, and only
prefab→instance instantiation runs), but `Instantiate` is PUBLIC
(`include/outshine/Store.h:51`) and accepts any standing entity, so the collision is one
client call away.

The adjudication this item demands, either way written down: (a) a second relation
(`Holds` vs `ChildOf`) so composition and containment stop sharing a spelling, or (b) the
one-relation design is kept and `Instantiate`'s cascade is DEFINED over it (copy-the-room-
copies-its-contents as the decided semantic, with the removal rule stated for regions), and
a unit proof in `test/unit/scene/` pins whichever cascade is chosen.

---

Closed, adjudicated: HOLDING IS ITS OWN RELATION. Relation::Holds joins the catalogue
(exclusive -- one holder; acyclic -- a bag cannot hold itself; NOT owned-by-target --
removing a holder FREES its contents, destruction is a choice nobody made). ChildOf stays
the prefab subtree's bone with its cascade. The collision the reviewer proved is gone by
construction: Instantiate copies ChildOf and never Holds. The five-noref initialiser trap
this change tripped (a sixth relation head zero-initialised to a VALID ref) died with it --
the head arrays fill from kNoRef by count, never by a hand-written list. Proving test:
unit/scene/AHolderKeepsWhatItHoldsAndAPrefabDoesNot -- the second garage holds nothing, the
car outlives the first. 136/136.
