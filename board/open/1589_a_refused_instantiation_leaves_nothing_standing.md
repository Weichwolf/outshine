Type: bug
Area: core
Tags: scene

**A refused instantiation leaves nothing standing**

`Store::Instantiate` (`src/scene/Store.cpp:249-266`) walks the prefab's subtree copying as it
goes and returns `kNoEntity` the moment any inner `Add` or `Link` refuses -- with everything it
built so far still standing in the pool. The refusal is loud; the state it leaves is not clean.

Reproduced against HEAD:

```
prefab = Add(Body); mindChild = Add(Mind); Link(mindChild, ChildOf, prefab);
inst = Instantiate(prefab);
// Alive(inst) == false, Error() == "IsA does not reach a mind"
// but the pool now holds TWO orphans: the partial instance (IsA -> prefab)
// and a dangling Mind copy linked to nothing
```

The trigger is not exotic: `kRules` (`src/scene/Register.h:52`) lets `IsA` reach only
`Role::Body`, so EVERY prefab carrying a mind or tool child refuses mid-walk -- and the
reference's own slot example (the driver seat, board:1583) is exactly a non-body child. Two
decisions, one knot:

- refusal is atomic: validate the subtree before the first `Add`, or roll back what was built --
  an assembly that half-happens is the anti-pattern the store exists to refuse
- `IsA`'s reach is decided at the groom of 1583: either the relation reaches all roles (an
  instance of a mind-child is a mind), or a non-body child under a prefab is refused at
  `Link(ChildOf)` time, at assembly, where the refusal can still be about the wiring

`unit/scene/APrefabInstantiatesItsSubtreeAndNamesItsSlots` proves only the sunny path; the
refused path is unproven and wrong.
