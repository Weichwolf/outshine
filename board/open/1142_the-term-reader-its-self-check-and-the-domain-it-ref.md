Type: task
Parent: 1137
Area: render
Tags: instrument
Depends: 1141

**The term reader, its self-check, and the domain it refuses rather than reports**

The readback of `board:1141`'s two targets, and the statement of what the instrument is about — because a
probe whose domain is unstated is the *invariance too broad* face of a number about something else.

**THE SELF-CHECK IS AN IDENTITY AND IT COSTS NOTHING.** On a lit, opaque fragment the shader computes
`SceneHdr.rgb = diffuse + specular + emitted`, so the residual `SceneHdr - diffuse - specular` is the
emitted radiance, and it is checkable against what the material declares. **A pixel whose residual falls
outside the instrument's own floor is refused, not reported** — it is describing something else.

**THE FLOOR IS PUBLISHED BESIDE THE RESULT AND IT IS DERIVED.** The three terms are `Rgba16Float`: a
10-bit mantissa is 2^-11 = 4.88e-4 relative per value, and a difference of three of them is bounded by
about **1.5e-3 relative to the largest term**. A residual under that says nothing; a residual over it is
the finding. Stated as a value, not as a word.

**THE DOMAIN, EACH ARM WITH ITS OWN PREDICATE AND ITS OWN COUNT.**

| | |
|---|---|
| **the emitted arms** | write a declared zero and are excluded **by predicate**, the shape `SUBJECT_NO_SHADING_NORMAL` already uses. Today that is **27 of 35 cases entirely**, and the report says so rather than averaging over an empty set |
| **blended surfaces** | **refused, per case and not per pixel.** `SceneHdr` at a blended pixel is a composite of several fragments while the probe targets hold the last one written, so the identity fails and nothing distinguishes that failure from a defect. The predicate is the case's declared surface state |
| **back faces** | reported and split, never excluded. The facing is already on `SceneShadingNormal`'s `w`, and `board:1126` needed exactly that split |
| **more than one light** | the sum only. Sixteen lights times six channels does not fit and never will, so the instrument **cannot say which light** and states it. `lighting/point-light-intensity` declares six panels and is the case where that bites |
| **a pixel the host resects differently** | not a hazard here and that is why the terms were chosen over the inputs: nothing per-pixel is resected. It remains a hazard for `board:1139`, where it is detected by the identity comparison rather than assumed away |

**WHAT IT MUST REFUSE RATHER THAN REPORT**: any pixel failing the identity beyond the floor · any pixel on
a blended case · any pixel with no shading normal · a case with no lit fragment at all, which is a refusal
naming the case rather than a clean zero.

**Done when** the eight lit cases publish a diffuse and a specular field that reproduce their own
`SceneLinear` to the floor above, and the refusals are counted and named on the twenty-seven that do not
shade.
