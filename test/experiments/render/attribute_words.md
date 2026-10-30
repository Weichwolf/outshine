# Exact original attribute rows

Owner #1855, parent #1797. This extends `source_fragment_inputs.md`; geometry,
original vertex/part/material order and the existing interpolator stay unchanged.
It reuses the four-word palette encoding from #904, without a native shader change.

## Representation and lifetime

`attribute_words.prepare` owns immutable bytes for Float32 rows of width 1..4.
The adaptive choice is a single constant row, a UInt16/UInt32 indexed palette, or
direct rows when the complete encoded palette would be larger. Palettes pad
narrow rows to four words; their actual sizes include the old header and padding.
Rows are compared as UInt32 words: signed zero, HDR, alpha and distinct NaN payloads
are preserved. This layer applies no quantization or material interpretation.
The material reader's existing finite-attribute requirement remains in force.

`vertex_colour_palette.unique_rows` sorts opaque 16-byte keys instead of structured
four-word comparisons. Table ordering changes; header/index/table layout and exact
decoding do not. `view` validates the existing format before borrowing its spans;
`decode` still offers the old complete decoded array for existing callers.

`compact_fragment_inputs` reuses immutable geometry and part/material metadata.
The completed compact product retains no plain attribute snapshots. Cold `prepare`
does include validation and construction of the temporary plain snapshot; this is
not a cache-hit path. No publication, IO, cache policy or worker is introduced.
Existing callers may keep the plain product; selecting this experiment is explicit.

Reads resolve requested original corner indices only. Constant rows also
materialize those requested values in C order: a broadcast view had changed the
consumer's floating-point reduction order despite identical input words. Palette
reads select actual columns before gathering for the same reason. The regression
test checks both layout and bit-identical interpolation, without relaxed tolerances.

Limits are 262144 rows and 8 MiB per attribute; the material wrapper has an explicit
total capacity. Malformed layouts/indices and exceeded capacities raise before
returning a product. Preparation may allocate bounded intermediate buffers before
refusal; callers retain the original product. Unknown geometry and interpolation
failures keep the existing original-group fallback.

## Actual comparison

`compare_attribute_words.py` uses the whole fixed #1838 packet: 19 originals,
1558 sources, 38476 triangles and 78374 vertices. It checks every original word,
both actual World poses and all raw hit outputs against the plain representation.
It retains direct, forced UInt16/UInt32 and adaptive controls in the comparison.
The same-run adaptive preparation control substitutes the previous tuple sorting;
only that control uses the old sort. Both restore every word exactly.

Actual attribute payload including 24 B of shared part/material metadata:

| Representation | Bytes |
|---|---:|
| Plain/direct | 3448480 |
| Adaptive | 983208 |
| Forced UInt16 palette | 1139976 |
| Forced UInt32 palette | 1766984 |

Adaptive uses three UInt16 palettes and one 16 B constant colour. It reduces this
attribute payload by 71.49%. All 1695316 B of fine geometric payload remain;
combined numeric payload is 2678524 B instead of 5143796 B, before Python objects.
Original/reference input buffers remain alive in the comparison process.

Seven CPU rounds and separate Memray traces measure cold preparation and selected
queries. Preparation includes plain snapshot construction. Queries include existing
selection/lifting/interpolation, but exclude geometry queries, source loading,
correctness checks, images, imports and already held buffers. Scoped peaks measure
incremental allocation, not process RSS or device residency. Allocation/requested
bytes do not measure all copy traffic. Explicit work includes immutable source
snapshots, padded palette rows and index/value serialization during preparation;
query work records 609/606 requested corners, 44 value bytes per corner and the
additional palette-index gather bytes. There is no whole attribute decode per query.

The frozen comparison ran successfully on the Linux i5-1135G7 at normal priority.
Adaptive preparation took 55.1 ms versus 280.1 ms with tuple sorting; plain
preparation took 1.55 ms. Adaptive peak was about 10.0 MB versus plain 4.78 MB.
Query CPU times stayed around 1.2--1.3 ms with similar incremental peaks; no general
CPU speedup is claimed. Exact costs, commit, load and owner acceptance live in
the linked PR. Four opened raw-normal/vertex-colour panels have zero differences.
They are input diagnostics, not PBR renders; the existing native mask discrepancy
of one sample per pose and previous coverage-error failures remain unchanged.

## Scope and sources

The local SIGGRAPH starting point was Deering, *Geometry Compression* (1995),
`doc/references/transport/siggraph/1995-geometry-compression.pdf`, including optical
inspection of page 3. Its quantization/normal coding is not this exact-word method.
The concrete storage precedent is #904; its existing mixed native/A18 timings do
not support a general frame improvement. Existing NumPy/Trimesh and optional
Memray are reused; no new dependency or native implementation.

Native contract #1853 still owns original normal packing before interpolation,
placement, flat/smooth facade inputs and primitive-bound screen footprints.
Whole original geometry and source corners remain available for that consumer.
No new native format/API, GPU, IO/cache-hit, UMA, World-PBR or p99 claim follows.
Unknown World revisions stay null; #1797 and the broader research goal stay active.

```sh
export PYTHONPATH=lib/src:test/experiments/render
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
python -B -m unittest test_attribute_words test_vertex_colour_palette test_source_fragment_inputs
python -B test/experiments/render/compare_attribute_words.py --cost --output "$fresh_temp"
```

Use the shared fresh-hosttemp output rule; no oracle or published image is changed.
