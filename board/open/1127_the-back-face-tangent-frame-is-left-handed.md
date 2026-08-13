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
