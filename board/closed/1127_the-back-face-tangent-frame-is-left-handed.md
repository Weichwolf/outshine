Type: bug
Area: render
Tags: khronos, instrument

**The back-face tangent frame is left-handed, and no case exercises it**

`mappedNormal` in `src/render/stages/SubjectDraw.cpp` carries the comment *a back face turns the whole
frame around and not only the normal … which is what the sign below does to all three.* **It negates
two axes, not three.**

```
float3 n   = normalize(in.n) * side;
float3 raw = in.t.xyz * side;
float3 t   = normalize(raw - n * dot(n, raw));
float3 b   = cross(n, t) * in.t.w;
```

`side` flips `n` and `t`; the cross product is bilinear, so `cross(-n, -t) = cross(n, t)` and **`b` comes
out unnegated.** The back-face frame is `(-n, -t, +b)` where Khronos's sample viewer produces
`(-n, -t, -b)` — it flips `t`, `b` and the geometric normal explicitly rather than deriving `b` after the
flip. A left-handed frame mirrors the map's y axis, which is exactly what the comment says it prevents.

**It is proven by algebra and it is not the defect `board:1126` is about.** Measured on both tangent
assets: **0 shaded fragments are back-facing**, so the branch never runs there. `doubleSided: true` made
it *reachable*, and reachable is not exercised — a distinction that cost a round's worth of attention
until the count was taken.

**So no case in the corpus can catch this**, which is why it is filed separately rather than repaired
quietly. **Done when** the frame negates all three axes, and a case exists whose shaded back-facing
population is non-zero — otherwise the repair is unfalsifiable and the next round has no way to know it
held.

## Comments

**CLOSES ON THE FIRST CLAUSE. THE SECOND IS STRUCK AND RE-HOMED TO `board:1147`, WHICH IS STRICTER.**

**The algebra is now proven twice and the second proof is on the device.** `NormalFromMap.h` states the
basis once in C++ and once in MSL, and the two are spelled **differently on purpose**: the C++ half
negates the three axes one at a time, which is the format's own sentence; the MSL half builds the
front-facing basis and carries a single sign out to the composed normal. `normalize(t*sx + b*sy + n*sz)`
is linear in all three axes, so negating the basis and negating the result are the same value — and
**exactly** the same value, because a sign flip is exact in IEEE-754 and `|-v| = |v|` bit for bit. So the
tie's measured worst `|front + back| = 0` is a **derived prediction met**, not a lucky zero.

**Why an instrument sharper than the one asked for satisfies an acceptance written before it existed.**
The clause asked for falsifiability and said so in its own words — *otherwise the repair is unfalsifiable
and the next round has no way to know it held*. A corpus case was the only falsifier visible when it was
written. The shader tie is a better one on all three axes that matter here:

- **It is the suite the placement rule names.** *What would fail this test?* — the shader is wrong on a
  real device. No asset, no camera, no oracle. `test/render` answers *wrong pixels*; this is not that, and
  a case that seems to fit two is testing two things.
- **Its mutant is the defect itself, not an invented one** — the two-axis frame this item was opened
  about, measured at **158.286753°** over **480 of 800** back-facing samples and **0** front-facing, so
  the instrument is shown to see the exact thing it exists to catch, in the exact population.
- **A corpus case would have measured three things summed**: this item's basis, `board:1126`'s open
  front-face disagreement, and an oracle back-face convention that is **unestablished in this tree**
  (`board:1148`). That is the *input set too wide* face of a number about something else, and the number
  would have arrived with no way to attribute it.

**Why the clause is STRUCK rather than declared met, and this is not a split difference.** The clause names
a **population**, and the population is genuinely absent: **0 back-facing shaded fragments of 1 328 002**,
counted over all 35 cases from the shading normal's sign channel. Calling the tie *met* would delete the
only place that gap is written down — and the gap is **wider than this item**, because all **nine** lit
fragment entry points take `[[front_facing]]` and none of them has ever been reached from behind, mapped
or not. So the state call and the scope call are separate and both are unambiguous: **this item's
statement — the back-face tangent frame is left-handed — is now false of the tree, so it closes**, and the
population requirement moves to `board:1147` covering all nine arms instead of one. Scope moved; none was
given up.

**What the repair raised, and it is the kind that does not show in a metric.** The two-axis error moved
from *forbidden by a comment* to **unspellable**: the old form negated `n` and `t` at their declarations
and derived `b` afterwards, so the third axis could silently not turn; the new device form has no place to
negate an axis individually at all. The comment that used to carry the rule is now the shape of the code.

**And the picture not moving is a prediction met, not a null result.** All 105 rendered artefacts are
bit-identical across the repair, which is exactly what **0 of 1 328 002** predicts — an arm no fragment
enters cannot change a pixel. *Identical is a finding* applies to a change that alters the picture **by
design**; this one alters a branch the corpus never takes, and the count says so before the pictures do.
