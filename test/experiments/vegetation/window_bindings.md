# Reuse pattern bindings at their existing boundary

Owner: [#615](https://github.com/Weichwolf/outshine/issues/615), under #182.
The [rejected architecture study](spruce_interwhorl.md) exposed128.6million
positive allocations in complete Python execution. This small optimization
removes a measured source of unnecessary recipe serialization. It does not
improve or approve that study's still sparse spruce morphology.

## Work removed and contract retained

`spruce_growth_product.windows` already visits the finite patterns before any
window work. Retain each capacity there instead of serializing the same pattern
for every active axis. `conifer_windows.bind_patterns` resolves only occupied
orders, once each, in first-encounter order. Its original per-window template,
capacity validation, phase rotation, family dtype and tail weighting remain.
No global cache, new lifetime, shared-library/native API or alternate generator.

For the actual46,686axis/76,020window plan:46,685nonempty axes, three physical
patterns and three occupied orders. Canonical-key construction falls from
122,708calls to six per complete window build. Empty binding plans do not
touch the catalog; unused orders need no catalog entry. Capacity disagreement
still rejects before phase/family work. Original per-pattern preflight remains.

The four new independent controls check lazy/mixed/empty orders, scalar phase
matrices and weights, every-window capacity refusal, unchanged input fields and
key-work counts across30physical axes. They are not elapsed-time unit gates.

## Actual full comparison and costs

`window_binding_cost.py` obtains both original own helper files from the declared
Git revision, wires the frozen binding into the frozen product builder, and reads
the complete #610axes/windows. Both old and new builders must match **every**
saved decoded field and dtype exactly. The unbound original plan is captured at
the existing function boundary for isolated binding measurements, not approximated
by undoing rotations. No growth, needle geometry, new catalog or content reduction.

Producer`c162cf32a`, reference`b1894f63705f078d63ecd0ac4c77eeb1866040fe`.
Linux x86_64/4logical CPUs/16GiB, Python3.13.5/NumPy2.5.4; existing Memray1.20.0.
The source and installed controls overlapped part of this diagnostic run. Times
are observed host measurements, not isolated device or whole-build admission.

| Work | Original | Reused bindings |
|---|---:|---:|
| Complete cached-axis→windows, one observed call [s] |17.572644|13.085218|
| Binding only, median of3ordinary calls [s] |1.059152|0.062559|
| Binding positive allocation events |7,526,261|228,626|
| Binding cumulative allocated bytes |888,196,958|38,174,913|
| Binding tracked peak bytes |12,196,717|12,190,541|

All ordinary samples and input/code hashes are in the report. Memray tracks
Python allocators around `bind_patterns` only; its profiled results also match
the uninstrumented results exactly. These are not allocation totals for complete
window generation or the full plant. Peak live memory barely changes: the large
output arrays remain required. The optimization removes churn, not all work.
Process peak208.199MiBincludes decode, original/new outputs and profiling.

Unbound input arrays10,414,740bytes; complete bound output11,326,980bytes, of
which6,385,680are new retained frame/family/weight arrays. Unchanged fields are
shared. Internal copied bytes remain unknown; no hardware-wide optimum claimed.
The remaining NumPy/pose/RNG work from #610is not solved by this change.

All complete window fields match the existing cached product. Axes, physical
recipes, finite banks and renderer are unchanged, so the existing actually opened
#610render inputs remain valid. **No new pixel/render-speed/visual improvement
claim**, no render repetition and no new images/server/gallery. #182still needs
fine-shoot hierarchy and projected gap/coverage work.

## Verification and reproduction

At the original producer50source/50installed affected controls pass. After
merging the upstream #607format-only modelling cleanup, a fresh wheel is built
from a private/tmpcopy of lib/ and all current package files compared to source;
the affected source/installed controls pass50/50again (205module files exact).
Fresh installed generation of all76,020windows matches every cached field and
dtype,12.600sone observation; all41loaded reference modules resolve inside that
wheel environment. Six real cached twig details also remain byte-exact.
No optional glTF requirement is
restored. Ruff/format, functions<=120lines, lint-docs and diff are affected checks;
native gates remain with integration owners.

Use `.venv`, `TMPDIR=/tmp`, `PYTHONDONTWRITEBYTECODE=1` and
`PYTHONPATH=lib/src:test/experiments/vegetation`:

```sh
.venv/bin/python test/experiments/vegetation/window_binding_cost.py \
  --product /tmp/your-complete-raw-product \
  --catalog /tmp/your-original-catalog \
  --reference-commit b1894f63705f078d63ecd0ac4c77eeb1866040fe \
  --output /tmp/your-binding-comparison
```

`--profile` optionally uses Memray. Obtain raw inputs with the existing
self-contained source and #610product commands; the adaptive capacity product
is a different representation and is not silently substituted. Frozen own
reference files, complete input pins, output report, raw profiles and terminal
logs stay under`/tmp/outshine-vegetation-window-bindings-proof-b1894f637/`.
Local SIGGRAPH/physical reference context and Ghidra scope remain in the linked
study; no vendor algorithm or new mathematical result is claimed for ordinary
per-call reuse. Keep morphology, allocation work and frame/device quality separate.
