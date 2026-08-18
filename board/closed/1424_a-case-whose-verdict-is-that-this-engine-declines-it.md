Type: feature
Area: corpus
Tags: oracle, khronos, scope

**A case whose verdict is that this engine declines it**

`limits-probe` has been in this harness's criterion vocabulary since it was written -- *the asset states
it is not expected to render correctly everywhere* -- and **nothing consumed it**. So a case whose whole
subject is a feature this engine declines had no way to pass except by the engine changing its mind.

**`SpecGlossVsMetalRough` is that case.** Its criterion is *tests if the
`KHR_materials_pbrSpecularGlossiness` extension is supported properly*; Khronos **archived** that
extension, and an obsolete extension is one this engine does not implement by the owner's own ruling.
The file names it in `extensionsRequired`, so a conforming reader **MUST** refuse it. **The refusal is
the correct behaviour and there was nowhere to score it as such.**

```
DECLINED KHR_materials_pbrSpecularGlossiness -- ... requires extension
'KHR_materials_pbrSpecularGlossiness', which this reader does not implement
```

## What keeps it from being a way to pass by failing

- [x] **`declines` names what**, and the engine's refusal must CONTAIN that name. A corrupt file, a
  missing camera and a short buffer all refuse too, and none of them is what the case is about
- [x] **The name is REQUIRED by the schema** on this criterion kind, so a `limits-probe` that says only
  *this will not render* does not parse
- [x] **It is printed**, so a reader of one log sees exactly what was declined and by which words

## Comments

**This is the fourth kind of answer the corpus can now give**, beside a number, a stated invariant and
a self-description -- and it is the one that says *the engine is right to say no*. Without it the only
honest states for such a case were red forever or a capability nobody wanted built.
