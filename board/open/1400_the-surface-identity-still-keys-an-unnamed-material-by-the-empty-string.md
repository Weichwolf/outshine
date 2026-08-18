Type: bug
Area: harness
Tags: khronos, instrument

**The surface identity still keys an unnamed material by the empty string**

`board:1362` established the key for a material a file does not name: **`Material_<glTF index>`**, which
is what Blender's importer publishes and therefore the one string both sides can state. The colour arm
uses it. **The surface-identity arm does not**, and `FileMaterialNames` in the render harness still
says so in its own comment:

> *An unnamed material yields an empty string and therefore matches nothing: the correspondence is by
> name, and a file that named none of its materials has no correspondence to derive.*

That was true before the key existed. It is now a second spelling of a rule that has one.

## What it costs, and the message is the evidence

[MEASURED] over the corpus run, five cases refuse the identity comparison outright with variants of

```
surface identity: no pixel both sides cover carries an identity both of them can resolve,
so the oracle's materials are not this file's and the two partitions are not comparable
-- the file carries no material named Material_42
```

**The message names the key the other arm built.** Our side asks for `Material_42`, this table holds an
empty string at index 42, and the two arms of one comparison disagree about what a material is called.

## What must be true

- [ ] **`FileMaterialNames` derives the same key**, and its comment is corrected rather than left
  justifying the behaviour it had
- [ ] **The rule is spelled ONCE.** It is now in the colour arm, the preparer and this table -- three
  places for one string is the shape `board:1380` is about, and this is its fourth instance
- [ ] **The five cases are named before and after**, so the count is a measurement over the same
  population

## Comments

**This is the fourth time in one round that a rule has been built in one place and not the other**, and
every one of them was found by running rather than by reading. The pattern is not carelessness about
any single edit: it is that the rules have no single home, which is `board:1380`.
