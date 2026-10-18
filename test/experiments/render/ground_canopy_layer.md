# Native coarse canopy material layer

Issue #451, goal #189, Draft PR #645. This is a limited Metallic-Roughness
micro-surface approximation above the retained substrate/litter. It does not
implement plant geometry, projected alpha/shadow coverage, leaf R/T, wind or the
resolved/coarse handoff. The inherited authored `swardClosure` is a fixed coarse
material fraction, not a derived directional gap probability.

## Composition and metric support

The existing prefix retains the same coarse RGB/roughness and offsets. Raw ground
and litter survive separately. The coat uses the original green/dry reflectances
and dryness. The existing filtered `substrateDetail` procedure supplies roughness
and relief. Colour instead mixes the declared green/dry endpoints with a bounded
world-fixed dryness field at the authored litter coarse wavelength. If p is mean
dryness and f the existing filtered field in [-0.5,0.5], local dryness is
p + 2*min(p,1-p)*contrast*f. Pure endpoints and zero contrast skip this extra field.
The coat adds its centred colour difference to the prefix; the remaining raw
substrate/litter detail uses the complement weight. There is no separate colour
brightness modulation on the coat. Filtered-out detail returns exactly the prefix
mean. Light/shadows apply once; no extra pass, texture, alpha or camera/sun product.

For positive density d, height h and width w, the prepared microrecipe uses pitch
1/sqrt(d), relief min(h,w,litter relief), coarse wavelength max(h,2*pitch), and fine
wavelength max(w,pitch). Height jitter scales only the bounded relief envelope.
These are explicit micro-surface authoring approximations, not a new botanical
coverage law. Zero density, height, width or fraction disables the layer and
preserves the raw substrate/litter. Invalid declared active inputs are refused
before publishing a replacement catalog; fractions are checked before float rounding.

## Float-buffer contract

The existing litter layer prefix is followed by six vec4-equivalent records per
row, including a zero fallback. No logical GPU binding or prefix offset changes.
The consumer checks actual buffer length and requires the complete exact tail;
legacy, short, extra/unrecognized and disabled tails retain the old path.

| Float offset | Meaning |
|---|---|
| 0 | Raw substrate RGB/perceptual roughness |
| 4 | Raw litter RGB/perceptual roughness |
| 8 | Green RGB/original density |
| 12 | Dry RGB/original height |
| 16 | Width, height jitter, dryness, coarse fraction |
| 20 | Prepared pitch, relief amplitude, coarse/fine wavelengths |

The shipped 14 rows plus fallback add 1440 B: 1276 B becomes 2716 B per palette.
Packing derives the recipe once; fragment work follows only enabled coat rows.
`ground-palette-layers-2` prevents reuse of a serialized product missing the tail.
The existing Metal auxiliary buffer-size slot25 becomes used by the runtime array
length query. Translation is not Metal execution or A18 acceptance.

## Evidence and reproduction

`ground_canopy_native.py --palette <native-f32-export> --binary <native_ground_litter>
--output <system-temp-directory>` compiles the real shared shader through the
existing opaque-float native GPU probe. NumPy and the pinned glslang are required;
`GLSLANG` selects its path. Backend/layer selection is inherited from the caller.
The input is an actual `GroundMaterialPalette` export, not a Python reconstruction.

On Linux/RTX3060, thirteen variants with 10240 probes each pass: old/short/extra/zero
tails and zero support are bit-identical to legacy; other classes are unchanged;
far RGB/roughness equal the original prefix exactly and far bump is zero. Near
colour, roughness and relief differ for the two declared grass classes. The report
records actual commands, terminal results, binary/palette/shader hashes and deltas.
This is composition/format evidence, not an independent canopy transport oracle.
Pure green/dry endpoints are exact without changing normals/roughness; unit-colour
mixtures stay inside the independent bounds and zero contrast is constant.
Two alternating 32-dispatch GPU batches measure 5.76–5.92 microseconds per legacy
and 7.26–7.30 per coat dispatch (10240 mixed probes). Batch uploads/readback are
measured separately, not added. Logical buffer requests grow by 2880 B across five
buffers, copied bytes by 1440 B; physical allocation/driver overhead is unmeasured.
These component costs do not establish whole-frame or A18 budgets.

The initial-coat native candidate66d94e611 builds coherently and produces paired
Körbersee/Tokyo/CentralPark images with unchanged coarse RGB and reported geometry.
The PDFs were optically reviewed; local grass response changes, broad world quality
remains incomplete. A private wavelength-only control restored excessive broad
relief; the bounded green/dry field improves spatial colour instead. Its native
CentralPark/Körbersee prototype and final-product PDFs are reviewed; final
shader/world evidence and remaining red gates are recorded in the PR. Tokyo still fails its fresh ten-second load and frame checks;
the first legacy Körbersee frame control is also red. Exact current commands,
pixels/costs and pending full-lint/Metal admission remain exclusively in PR #645.

The filter reuses Feibush, Levoy and Cook, SIGGRAPH1980, *Synthetic Texturing Using
Digital Filters*, locally under `doc/references/materials/siggraph/` (projected
footprint discussion/figures optically reviewed). No new filter is introduced.
The agreed shared contract and consumer replies are in #451; the older RGB-only
world comparison in `ground_canopy.md` is separate from this new shader's admission.


## Independent footprint comparison

`ground_canopy_footprint.py` takes the same palette/binary/output arguments as the
native composition probe, plus `--quadrature 3` (default nine samples) or `2`.
It checks 4864 actual GPU probes against independent 64×64 midpoint integration
of the float32 projected parallelogram, including rotated, narrow and sheared
footprints, exchanged/reversed axes, and a 256-position world-space motion path.
The reference includes the declared one-to-two-wavelength fade; 32×32 integration
checks its convergence. This tests the colour field, not geometric grass coverage.

The predeclared limit is one neutral sRGB display LSB. Nine samples pass with
0.15318 LSB maximum error, 0.02661 LSB RMS, 0.00333 LSB reference convergence,
2.99e-8 linear axis-symmetry residual and 0.00182 LSB motion-difference error.
Four samples fail at 1.55082 LSB; this expected failure remains nonzero and has no
success receipt. Unfiltered-point and uniform-mean negative controls fail at
7.72452 and 23.96493 LSB. No threshold or product shader was changed for this check.

Two alternating 32-dispatch batches of these 4864 fully coated probes cost
5.280 microseconds/dispatch for nine samples versus 4.800–4.864 for four. GPU batch
scopes including upload/readback are 0.219648–0.222464 ms versus
0.204352–0.206080 ms; these overlapping scopes must not be added. Both request
628024 logical buffer bytes and copy 314012 bytes. The cheaper four-sample
candidate fails the same quality requirement, so the existing nine-sample filter
is retained. These are component measurements, not world or A18 acceptance.

`ground_canopy_footprint_report.py --selected <nine-sample-output>
--alternative <four-sample-output> --output <system-temp-report.pdf>` plots the
retained measurements. The resulting PDF was also reviewed optically.
