Type: bug
Area: render

**How near the engine draws is declared, not fixed**

`Renderer::kNearM` was 0.05 m and nothing could override it, while the `Placement` handed to the same
renderer carried `ZNearM` from the framing rule. **Two determinations of one quantity, and the constant
won** -- so a subject framed nearer than 5 cm was cut away entirely and the picture was a refusal.

**[MEASURED] `MetalRoughSpheresNoTextures` is 5.6 mm in radius and the rule frames it at 40 mm.** Every
vertex of it lay in front of the fixed plane: *vertex 0 sits 0.042035 m along the view axis, inside the
engine's fixed near plane of 0.050000 m*. **A subject smaller than a matchbox could not be rendered at
all**, which is a statement about the engine and not about that asset -- a coin in a hand, an inventory
item and a scope reticle are every one of them inside 5 cm.

## Why it costs nothing to move

**The projection is a reversed-Z infinite one**, where depth precision follows `1/z` and is nearly
uniform in floating point. That is the whole reason it was chosen, and it is what lets the near plane
belong to whoever knows how near their world is instead of being a compromise nobody can move.

## What must be true

- [x] **The consumer declares it** and the renderer's constant is the DEFAULT rather than the answer:
  `SetNearM` refuses a value that is not above zero, so a consumer that declares nothing renders
  exactly what it rendered before
- [x] **The pre-flight refusal reads the same number**, or the check and the projection would disagree
  about which plane cut the subject
- [x] **`ReadDepth`'s range conversion follows it**, because that division's numerator IS this plane

## Comments

**It was found by a case that could not render at all, not by one that rendered badly.** The refusal
named both numbers, which is why the cause was one grep -- *a refusal that names what it refused* is
the rule that paid for itself here.
