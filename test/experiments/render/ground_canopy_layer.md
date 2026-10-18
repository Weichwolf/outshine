# Native coarse canopy material layer

Issue #451, goal #189, Draft PR #645. This is a limited Metallic-Roughness
micro-surface approximation above the retained substrate/litter. It does not
implement plant geometry, projected alpha/shadow coverage, leaf R/T, wind or the
resolved/coarse handoff. The inherited authored `swardClosure` is a fixed coarse
material fraction, not a derived directional gap probability.

## Composition and metric support

The existing prefix retains the same coarse RGB/roughness and offsets. Raw ground
and litter survive separately. The coat uses the original green/dry reflectances
and dryness, with the existing filtered `substrateDetail` procedure. Its centred
colour/roughness perturbations are weighted by the authored fraction; remaining
substrate/litter detail is weighted by its complement. Filtered-out detail returns
exactly the prefix mean. All terms enter the existing light/shadow response once.
There is no additional pass, texture, alpha or independent camera/sun product.

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

On Linux/RTX3060, nine variants with 10240 probes each pass: old/short/extra/zero
tails and zero support are bit-identical to legacy; other classes are unchanged;
far RGB/roughness equal the original prefix exactly and far bump is zero. Near
colour, roughness and relief differ for the two declared grass classes. The report
records actual commands, terminal results, binary/palette/shader hashes and deltas.
This is composition/format evidence, not an independent canopy transport oracle.
GPU timings, full current world images/costs and Metal/A18 remain unproven here.

The filter reuses Feibush, Levoy and Cook, SIGGRAPH1980, *Synthetic Texturing Using
Digital Filters*, locally under `doc/references/materials/siggraph/` (projected
footprint discussion/figures optically reviewed). No new filter is introduced.
The agreed shared contract and consumer replies are in #451; the older RGB-only
world comparison in `ground_canopy.md` is separate from this new shader's admission.
