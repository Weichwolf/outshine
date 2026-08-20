Type: feature
Area: corpus
Tags: oracle, instrument

**A corpus case is lit, so the suite can notice that shading is missing**

At least one case of every geometry class is rendered under a declared light with the file's own
material row, so the render suite exercises the arm a player sees. What a case decides today is where
geometry LANDS; what nothing decides is whether it can be LIT.

## What it is

Every render case declares `scene.light.kind: none` and a material arm that EMITS. That is the right
recipe for the question those cases ask -- a flat emitter isolates geometry from shading, and Cycles at
one sample with zero bounces then has no integration left to perform, which is what makes the oracle
exact. **But 181 criteria over 182 cases never needed a vertex normal**, so an engine that could not
shade at all would be green.

[MEASURED] the viewer -- the first consumer in this tree that declares a light -- declined **16 of the
34** generated models on its first run, because they carry no NORMAL and nothing calculated the flat
one glTF requires (`board:1471`). The suite had rendered all 34 within the picture bound.

**This is `CLAUDE.md`'s own sentence about itself**: *the number was right and about something else*,
in the shape called DOMAIN TOO NARROW.

## What must be true

- [ ] **At least one case per geometry class is LIT** -- a declared key light and the file's own
  material row -- so a reader that cannot shade is red rather than absent
- [ ] **The oracle's reduction for such a case is stated**, since a shaded surface is no longer a closed
  form at one sample: `board:0087` already names which subjects the oracle cannot be reduced for, and a
  lit case either lands on that ladder or declares a recipe that can integrate
- [ ] **The existing unlit cases stay exactly as they are.** They answer a different question and answer
  it exactly; this is an addition and never a conversion

## What this feature may NOT do

**It may not light every case.** A flat emitter is what makes the geometry comparison exact, and turning
the corpus into a shading corpus would trade a question it answers well for one it answers roughly.

## Comments

Found by a person clicking through the viewer and asking *why do cases pass the suite when they do not
render in the viewer*. The answer -- because the suite never asked -- is the kind a green count cannot
give you.
