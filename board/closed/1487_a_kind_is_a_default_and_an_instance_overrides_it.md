Type: task
Parent: 1480
Area: world
Tags: scope

**A kind is a default and an instance overrides it, at run time**

`Kinds` and `Instances` are read and carried and **nothing stands them up**. This is the row every game
noun is made of -- an item, an NPC, a door, a container, a placed wall -- so it is the one whose absence
costs the most.

## What must be true

- [ ] **An instance's attributes are its kind's, overridden by its own**, resolved once at stand-up and
  not looked up per read
- [ ] **`inherits` chains and a cycle is a refusal naming it**
- [ ] **An instance HOLDS instances**, and that is the same relation as standing in a region -- an
  inventory and a world placement are one mechanism, so a cup on a table and a cup in a bag differ by
  one field
- [ ] **An instance is 0 or 1..N of its kind**, and a kind with no instance is not an error
- [ ] **An attribute is a VALUE and the frame path carries no string** -- `CLAUDE.md`'s own rule, so the
  declaration's strings are interned at stand-up and a tick sees a key
- [ ] **A programme on a kind ticks at its declared rate**, which is `board:1475`'s scheduler and this
  item's dependency rather than its content
- [ ] **The count is bounded and the bound is declared**, because a world that spawns is a world that
  grows

---

Closed -- the row every game noun is made of stands up through the one assembly door:

- RESOLVED ONCE: a Traits column (interned uint32 keys, doubles, kMost [SET] 16 with refusal
  on the seventeenth) holds the kind chain's defaults overridden by the instance's own,
  merged at stand-up -- a read walks no chain and carries no string.
- INHERITS CHAINS, and a cycle CANNOT EVEN BE SPELLED: a kind may inherit only what is
  declared before it (the order is the declaration's), refused by name otherwise -- stronger
  than detecting the loop, on top of the store's Acyclic trait.
- HOLDING IS ONE MECHANISM: <holds what=.../> and in=... are both ChildOf -- an inventory
  and a placement differ by one field, and removal cascades by the owned-by-target rule the
  prefab work already carries.
- 0..N of a kind, a kind with no instance is not an error; capabilities (<may do=.../>)
  come from the constexpr catalogue and a typo refuses by name; a non-numeric value refuses
  ("a tick carries no string"); the count is bounded by the store the capacity accounting
  now includes kinds and instances in.
- The programme tick rate is 1475's scheduler, as this item itself adjudicated.

Proving test: test/unit/clients/AKindIsADefaultAndAnInstanceOverridesIt.cpp -- override
resolution to the bit, capability flow down IsA, both holding spellings, three refusals by
name. Gate 133/133.
