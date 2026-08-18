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

## Measured: the refusal class is gone and it bought no verdict

[MEASURED] over the full corpus before and after, same 131-case population:

| | before | after |
|---|---|---|
| refusals naming `Material_<n>` | **5** | **0** |
| criteria met | 123 | 124 |
| red cases | 40 | 39 |

**The one case that changed verdict is `Triangle`, and that was a camera restore rather than this
fix.** So the honest statement is: *this removed the refusal class it targeted and moved no verdict*,
because the cases it unblocked are red for other reasons as well. **A refusal is not a failure** -- it
says a metric decided nothing -- so a repair that turns refusals into comparisons is expected to show
up in what is DECIDED and not in what passes.

## What it uncovered, and it is the fifth instance of one class

With `Material_<n>` resolving, three cases now refuse on Blender's duplicate-datablock suffix instead:

```
the file carries no material named baseColor texture dielectric.001
```

**The colour arm has stripped that suffix since `board:1373` and this arm never learned to.** Same
comparison, same string, two spellings -- which is `board:1380` again, and the fifth time in this run
of work. Stripped here too, exact name first so a file that legitimately names a material `Foo.001`
is not sent to `Foo`.

**A third class remains and it is NOT this defect**: a file that names two materials the same string,
[MEASURED] on `Material` and on `BlueTransWithMask`. A partition by name cannot separate those, and the
honest answer is the index -- which is a larger change than this item and is not smuggled into it.

## The suffix strip, measured, and a correction to what I said it would find

`.001` refusals: **3 -> 0**. The trailer is **byte-identical** to the run before it -- 318 PASS,
117 FAIL, criteria 124, 112 within -- which is the expected shape for a change that only moves
refusals, and it is stated rather than glossed.

**CORRECTION.** I wrote above that a file naming two materials alike is a third class and named
`Material` and `BlueTransWithMask`. `DielectricSpheresMat` then appeared and I read it as damage my
own strip had done -- a distinction Blender's suffix was carrying and I had merged away. **It is not.**
[MEASURED] `EnvironmentTest` declares `["MetallicSpheresMat", "DielectricSpheresMat",
"DielectricSpheresMat"]`: **the file really does name two materials the same string**, and the refusal
is honest. *My first check looked at `IridescenceDielectricSpheres` and three other files, found no
duplicates, and concluded the message was wrong about the file. It was wrong about which file.*

**Neither affected case changed verdict** -- `EnvironmentTest` and `IridescenceSuzanne` pass in both
runs -- so the refusal is a published note on a green case and not a hidden red.

- [x] **`FileMaterialNames` derives the same key**, and its comment is corrected
- [x] **The five cases are named before and after**, and the honest answer is that the refusal class
  went 5 -> 0 and no verdict moved
- [ ] **The rule is spelled ONCE** -- still three places, and `board:1380` is where that is decided
