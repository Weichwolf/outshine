# Terrain skirt index locality

Issue #771, following actual Vulkan work measurement #756. The current33-node
native grid interleaves its four independent skirt edges segment by segment.
Compare this with complete edge runs, a continuous perimeter walk, and the
installed meshoptimizer0.22 vertex-cache optimizer. This experiment uses the
system `libmeshoptimizer-dev` package; no vendor implementation is bundled and
no new production library is needed for a fixed loop order.

```sh
python3 test/experiments/render/terrain_index_locality_checks.py
python3 test/experiments/render/terrain_index_locality.py \
  --output /tmp/outshine-terrain-index-model
```

`--native-indices /tmp/original-native-indices.u32` additionally checks a complete
original C++ grid export, including the unchanged interior prefix. Inputs are
native-endian uint32 values. The measured Linux export contains6912indices,
27648bytes and SHA25696ff4df0b83b6e39b6ce32620edb28ba890e4e24b40d63fe15f4aa8a4e2d8494.
The native compile/link commands and original source hash accompany that export.
It matches the Python recipe exactly. The optional export is a diagnostic input,
not a required private path or persistent test oracle.

All four candidates retain the exact multiset of oriented original triangles,
including each triangle's first vertex. Skirts contain256triangles,768indices,
3072index bytes and260referenced vertices. The complete vertex buffer remains
1221float3values/14652bytes. Interior index prefixes, shader inputs, winding,
coverage, height, LOD and buffer sizes are outside this permutation.

| Model, one tile's skirt draw | Interleaved | Edge runs | Perimeter | meshoptimizer0.22 |
|---|---:|---:|---:|---:|
| FIFO16 transformed vertices |262|264|261|261|
| LRU16 transformed vertices |324|264|261|261|
| Independent32-triangle batches |318|272|272|272|
| meshoptimizer FIFO/cache32, warp32, group32 |342|280|277|277|

The perimeter candidate happens to match this installed optimizer's index bytes
exactly. This is an observed comparator, not a global optimum or cross-version
promise. Controls include smaller grids, lost/duplicated/reversed triangles and
independent cache/batch examples. Models disagree in magnitude and do not prove
RTX or A18 invocation counts, elapsed time, shader bandwidth or frame acceptance.
A native GPU comparison with unchanged images must decide whether to retain it.

Primary reference: [Sander, Nehab and Barczak, SIGGRAPH2007](https://gfx.cs.princeton.edu/gfx/pubs/Sander_2007_%3ETR/tipsy.pdf).
Figures1/2 were inspected visually: locality can reduce repeated transforms;
render-time effects depend on the actual bottleneck. This experiment changes only
triangle order, rather than adopting the paper's optimizer or its overdraw method.
