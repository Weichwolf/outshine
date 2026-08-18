Type: feature
Area: corpus
Tags: oracle, instrument, khronos

**The ladder's second rung has a spelling**

`CLAUDE.md` states the order -- *fix the engine · reduce the oracle · patch the asset · disqualify* --
and **the harness could say only the first and the last.** A case whose reference genuinely cannot
answer a metric had two states left: red forever, or a threshold moved, which is the one thing this
tree refuses outright.

**A reduction names ONE metric of ONE case**, turns it into a reported number there and nowhere else,
and is announced by name in that case's log with its reason and counted in its report:

```
REDUCED linear_channels_differing -- This value passes through `pow(x, 2.4)` on BOTH sides and no
standard specifies that function to the last ulp ...
NOTE metrics this case declares its oracle cannot decide = 1 reductions
```

## What makes it a rung and not a hole

- [x] **THE REASON IS REQUIRED.** A reduction with no argument beside it is a disqualification wearing
  a softer word, and the reader refuses one
- [x] **The number is still measured and still printed at full value.** A reduction changes what a
  metric DECIDES and never what it says, so a residual that grows is still visible in the log
- [x] **`measured` carries the numbers the argument rests on**, as declared quantities with unit and
  origin, so a later round re-takes them rather than re-deriving the argument
- [x] **A reduction that names no metric this case reports is a REFUSAL** -- a stale name means the
  metric was renamed or the case was repaired and the reduction forgotten, and both must be loud
- [x] **It is per `(case, metric)`**, never per test, which is the rule `CLAUDE.md` already states for
  disqualification and which applies with more force one rung up

## The two it was built for, and both are the same shape

**`SimpleTexture/simple-texture`** -- `linear_channels_differing` demands bit-equality of a value that
passes through `pow(x, 2.4)` on both sides. **No standard specifies `pow` to the last ulp**, so two
conforming implementations differ. [MEASURED] worst **5 ulps**, worst picture channel **9.9012458e-06
codes** -- a thousandth of a code. We decode sRGB in software with the IEC piecewise formula, so the
residual is two libm implementations of one expression rather than two expressions.

**`SimpleTexture/four-texels-per-pixel`** -- the same metric under MINIFICATION, where the two sides
filter with different implementations. [MEASURED] 8569 of 12288 channels differ where the 1:1 sibling
differs by at most 5 ulps, **so the divergence is the filter and the sibling is what says so**.

**Both cases' own subjects still decide them**: `picture_p99_delta_code` passes at 1 and 0 codes, and
`worst_disagreement_px` at 0.

## Comments

**It was reached from three directions in one round.** `SheenWoodLeatherSofa` needs a per-case
instrument floor (`board:1417`), and these two need a reduction; all three are the second rung, and the
harness had no word for any of them.
