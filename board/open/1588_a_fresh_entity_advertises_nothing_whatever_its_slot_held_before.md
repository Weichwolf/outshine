Type: bug
Area: core
Tags: scene

**A fresh entity advertises nothing, whatever its slot held before**

`Store::Add` (`src/scene/Store.cpp:48-51`) resets `GivenCount` and `PairCount` and nothing else;
`Remove` clears nothing. `Offers`, `SeatCount` and the `Seats` array survive the death of the
incumbent, so the next entity standing in the reused slot inherits a dead object's advert whole.

Reproduced against HEAD:

```
Store s; s.Open(4);
Entity pump = s.Add(Role::Body);
s.Offer(pump, kRefuel, 2);
s.Remove(pump);
Entity fresh = s.Add(Role::Body);        // reuses index 0, generation 1
s.Offering(kOffers, found, 4)  == 1      // the FRESH entity is reported offering refuel
s.Offer(fresh, kRefuel, 1)     == false  // "this object already advertises, and one object is one offer"
```

Both halves are wrong: `Offering` (`Store.cpp:126`) reports an advert nobody made, and the refusal
at `Store.cpp:115` speaks a false sentence about an entity that never offered. The slot state
machine of board:1583 slice 2 is sound; the pool hygiene under it is not. `Add` must hand out a
slot in its ground state -- given, paired, offered, seated all empty -- and the proving test is
the reuse cycle `unit/scene` does not yet walk: Offer, Remove, Add, Offer.
