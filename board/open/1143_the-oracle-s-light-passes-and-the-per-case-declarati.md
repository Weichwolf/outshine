Type: task
Parent: 1137
Area: corpus
Tags: oracle, instrument

**The oracle's light passes, and the per-case declaration that makes them affordable**

Our side's term split (`board:1141`) is one side. Cycles publishes the same split — `Diffuse Direct`,
`Diffuse Indirect`, `Glossy Direct`, `Glossy Indirect`, `Emission`, `Environment` — and until it is read,
*the oracle has a highlight we do not* cannot be distinguished from *the oracle has a transport term we do
not implement at all*. **That distinction is the whole value**: our engine computes direct punctual light
and nothing else, so an excess landing in `Glossy Indirect` or `Environment` is not a lobe defect and no
probe of our BRDF would ever find it.

**THE RECONSTRUCTION IS DOCUMENTED AND MUST BE VERIFIED ON THE PINNED HOST BEFORE ANYTHING RESTS ON IT.**
Blender's rule is `combined = sum over type of (Direct + Indirect) * Color + Emission + ...`. The
*Color* passes are the trap: with a **Principled BSDF** — which is what the glTF importer builds — the
colour is reported as already folded into the Direct and Indirect passes and the Color pass comes back
white (blender/blender #87028, open since 2.79). If that holds on the pinned build, `glossyColor` and
`diffuseColor` carry **no information here and must not be requested** — six channels saved — and
`glossyDirect` is directly comparable to our specular sum **including the F0 tint**, which is exactly the
quantity `board:1136`'s warm-highlight reading needs. **Verify it by reconstructing one case's beauty from
its passes** before a conclusion is drawn from either arm.

**THE DISK COST IS THE REASON THIS IS NOT SIMPLY FOUR MORE ROWS.** `QUANTITY_PASSES` is global: eighteen
channels [MEASURED] at 293 MB a case and 19.9 GB across the corpus, on a 50 GB disk. Four light passes is
twelve channels on **all thirty-five** cases, and only **eight** of them shade at all. The file's own rule
is *a channel arrives when a test reads it* — a per-**test** statement enforced per-**corpus**.

**So the quantities become part of the recipe, per case.** The recipe is already in the oracle key, so a
case that adds a pass invalidates **that case** rather than the corpus. The one-time cost is honest and
must be published rather than estimated: changing any file under `test/harness/shared/corpus/prep/` moves
`render_code_digest()`, which is in every product's key, so **installing the mechanism re-renders
everything once** — that is `board:1120`'s design working as intended, and the round that does it
publishes the measured wall time rather than a guess.

**Two channels already held and unread go the same way**: `materialIndex` and `objectIndex` are produced
for all thirty-five cases and read by nothing (`board:1138` is the reader). Once quantities are per case,
they are declared by the cases that read them.

**Done when** the eight shading cases carry the light passes their comparison needs, the twenty-seven that
do not shade carry none, the Principled colour-pass behaviour is a measurement in this item rather than a
citation, and the invalidation cost is published as a measured number.
