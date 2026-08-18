Type: issue
Area: render
Tags: oracle, khronos

**The engine declares which Fresnel it implements, and the declaration is the owner's**

**THE DECISION.** glTF Appendix B specifies Schlick's approximation and the core specification says in
the same breath that an implementation of the BRDF **MAY** vary. Cycles evaluates the **exact
unpolarised dielectric Fresnel**. The two differ by more than a rounding, and the difference is not
confined to the extension that exposed it -- it is on every dielectric in the tree.

[MEASURED] on `SpecularTest`, `F0 = 0.04`, `ior 1.5`, radiance divided by the environment's own 0.25 so
the number IS the reflectance:

| view cosine | oracle | exact Fresnel | Schlick |
|---|---|---|---|
| 0.8 - 1.0 | 0.04111 | 0.04073 | 0.04001 |
| **0.4 - 0.6** | **0.09118** | **0.08811** | **0.06900** |
| 0.1 - 0.3 | 0.30295 | 0.29163 | 0.29993 |

**Head-on and at grazing the two agree; the mid-angle band is where Schlick is 24 % low**, which is the
band most of a sphere and most of a curved surface occupies. It is the shape of the whole ~9 % deficit
`SpecularTest` reports over its 23 panels, and it is not a defect in either renderer -- it is two
different published answers to one integral.

## The options

| | |
|---|---|
| **A -- keep Schlick** | the format's literal formula; the corpus keeps a named residual on every dielectric, and no case that shades can ever reach bit-exactness against this oracle |
| **B -- evaluate the exact Fresnel** | a handful of ALU ops on a path that already computes a square root; the residual closes; and the engine then differs from Appendix B in a way that must be written down where a reader meets it |
| **C -- Schlick with the Fresnel's own `f90`** | a middle that fits a curve to a number and buys the mid-angle band nothing, since Schlick's error there is in its shape and not its endpoints |

**RECOMMENDATION: B.** The exact expression has no free parameter, it is the physics the approximation
approximates, and the specification permits it in the sentence that defines the approximation. *A cost
this engine can pay in ALU should not be paid in a residual nobody can close.*

## What is NOT decided by this and must not be folded into it

`specularColorFactor` tints F0 and leaves the grazing reflectance at unity, and the oracle agrees:
[MEASURED] on `M3.2_whiteFac`, `F0 = 0.00205`, the oracle reaches **0.294** at grazing where an exact
Fresnel derived from that F0 would give 0.121. A tinted F0 is not an index of refraction, so whichever
option is taken, the tint keeps its own `f90` and B applies to the ior-derived arm alone.

## Comments

Found while closing `board:1428`. The measurement is the same instrument in both: the oracle's normal
buffer and the manifest's own camera give a true `n.v` per pixel, and the environment radiance is read
off the one panel whose F0 the file pins at 1.
