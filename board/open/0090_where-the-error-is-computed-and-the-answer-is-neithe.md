Type: feature
Area: render
Tags: oracle, instrument

**Where the error is computed, and the answer is neither of the two obvious ones**

- [ ] **The linear f32 tap on both sides, never the PNG.** `ReadSceneLinear` and `oracle.raw` are both float32 and both already read (`test/harness/shared/render/Parity.cpp`); a PNG carries a transfer function **and** a quantisation of its own, and the quantisation is the very term this section has to derive. Putting the instrument's own rounding inside the measurement is how a floor becomes invisible
- [ ] **But the difference is taken on a PERCEPTUAL axis, and that is the piece neither candidate offered.** A raw linear difference is the wrong unit in both directions: **absolutely**, a black dot at 0.0 beside a neighbour at 0.04 is a tiny number and a glaring defect; **relatively**, near black it diverges without bound and 0 against 1e-7 reads as infinite error and is invisible. The axis is the case's **own declared display transform** — `RenderPlan::Display()`, which § I.26.12 already requires every case to declare — applied to the f32 values and **not quantised**:

```
      delta_code = | T(ours) - T(oracle) | * 255      T = the case's declared transfer, unquantised
```

- [ ] **`delta_code` is a real number, not an integer, and that is what makes it a floor rather than a bucket.** Because nothing is quantised, **the output-quantisation term I derived for `filter-bounded` vanishes** — it was `≤ 1 code` of rounding on each side and there is no rounding now. *That tightens `filter-bounded` from ≤ 2 codes to its weight term alone, and it is a consequence rather than a preference: the earlier bound was partly a property of the instrument, and the instrument changed*
