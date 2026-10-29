# Existing building point/prefix reuse controls

Owner: [#1759](https://github.com/Weichwolf/outshine/issues/1759), Python only.
The concrete defect is spatial image error on actual native original buildings.
These controls reuse existing sampling, integration and triangle-ray kernels;
they introduce no representation family, renderer, codec or runtime policy.

## Exact operands and image comparison

Versioned `building-depth-samples` contains actual native Float32 readbacks of
1558 sources/38476 original triangles. SHA bindings preserve complete buffers,
clear values, camera/light state and crops. The centred128×96 crop retains the
same full1280×720 rays and focal length; it is not a smaller render profile.
All19 original geometry file hashes now match. The earlier scene-frame Python
reconstruction differed in six position/normal hashes and is not an authority.

The source512 input binds the old795a1cf4 capture/d485bf1c stored field. It must
not be mixed with later e6fa0391 data. Exact PBR channels and capture-lit RGBA
are provided; an independently perspective-relit source is not a native contract.
Source PBR uses the orthographic capture viewing direction, directional white
light100 and zero environment. OriginalSS64 remains a finite sampled reference.

## Repeated filtering is a separate error

The unchanged32px query integrates16×16 source cells per output pixel.
Existing Float64 prefix reconstruction and independent direct block means
agree within6.16e-13 maximum RGBA. On the exact same512 capture-lit source:

| Comparison | RGB relative L1 | Coverage relative L1 |
|---|---:|---:|
|Native normal pixel integral versus Python integral |0.0361% |0.0330% |
|Native normal pixel integral versus originalSS64 |5.8937% |3.3000% |
|Native pixel integral averaged over64² jitter views versus originalSS64 |14.2974% |8.4219% |
|Native jitter64 versus Python repeated integral64 |0.0355% |0.0332% |

`Filtered` rotates the camera64² times; `radianceSample` still integrates one
whole pixel at each view. Averaging these already integrated pixels smooths
the field again. The existing Python reconstruction with those same camera
rotations reproduces the effect. Its explicit rectangle-footprint approximation
has maximum excess area3.28e-5;16² and64² results agree to rounding scale here.
An independent one-cell impulse test recovers the separable triangular response.

Recommendation to the native owner: keep supersampling the original and point
opponents; compare the already integrated prefix's normal `.linear` output to
that original reference. The extra error is not evidence of more geometric
deformation. Remaining5.89%/3.30%, native source density, material/view reuse and
reference convergence still need separate evidence. No native code is changed.

## Canonical128 source and finite point/visibility controls

Canonical original/sample capture masks match at all2687 occupied cells; RGB-L1
is0.0288%. Holding that captured sample radiance without query relighting gives:

| Query/reference axis | Held-prefix RGB-L1 | Coverage-L1 |
|---|---:|---:|
|128px yaw0/4 |95.6788% |42.4742% |
|64px yaw0/4 |48.2634% |20.8852% |
|32px yaw0/4 |22.0210% |8.8650% |
|64px yaw4°/4 |60.4814% |30.1987% |
|32px yaw0/64 |24.4157% |9.8998% |

These are different operands from the512 native relight above. Same-source
block means agree within3.64e-12. Original4² versus64² changes23.0398%RGB and
8.8137%coverage. A whole-source flat-projection geometric L1 bound0.221357px
at32px excludes discretization, visibility, materials and filtering entirely.

Depth and normal select same-cell native point correspondences without RGB.
Explicit32/64/128-operation Float32 neighbourhoods are sensitivity checks,
not formal backend bounds. Strict quarter-cell interior cohorts contain
1088/138/2/103 points for128/64/32/64-yaw4; RGB responses are respectively
0.3033%/0.2630%/0.2319%/0.7314%. In particular, two32px interior points cannot
bound whole-image BRDF error. Boundary cells retain texture-nearest ambiguity.

Existing CPU first-hit rays use the exact original Float32 triangles. Finite
32px probes find1/692 hidden-at-capture hits with2² samples and24/10782 with8².
These are indicators, not a converged area or radiance bound. Independent native
base-centre comparisons record6/2/1/0 mask disagreements across the four views;
32px common-hit depth residual reaches2.52m. They prohibit treating the CPU
probe as an exact native visibility oracle. No mismatch is discarded.

SIGGRAPH first: Shade et al.,
[Layered Depth Images,1998](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
pages3–5/equations3–9/Figure3 inspected visually. Warp, resampling and
disocclusion are separate problems. No new LDI family is implemented.

## Reproduce and costs

Use `.venv` Python/NumPy, Matplotlib for private figures, and existing
Trimesh/Rtree for original-ray diagnostics. Set
`PYTHONPATH=lib/src:test/experiments/render`, BLAS/OMP threads1:

```sh
python -B -m compare_building_sample_reuse --output /tmp/outshine-research-reuse/points
python -B -m compare_building_sample_prefix --output /tmp/outshine-research-reuse/prefix512
python -B -m plot_building_sample_prefix --study /tmp/outshine-research-reuse/prefix512 --output /tmp/outshine-research-reuse/error.png
python -B -m compare_building_sample_visibility --output /tmp/outshine-research-reuse/visibility
python -B -m unittest test_process_memory test_building_sample_prefix test_building_sample_originals test_building_sample_points test_building_sample_reuse test_field_view_sampling test_field_view_integrals test_field_composition test_field_view_projection
```

Freeze sources first; outputs must be fresh private system-temp paths.72 focused
checks pass. Actual native channel/error figures were generated and opened.
No README/doc image publication or substitute gallery.

The512 diagnostic holds4194304B source and8421408B Float64 prefix. One normal
lookup processes1190 pixels/4760 prefix evaluations/19040 logical fetches;
64² repeated filtering visits5017600 pixels, with no new prefix construction.
Separate Memray1.20 instrumentation including Python allocators records441
prefix-build allocations/52.51MB requested/16.85MB tracked peak,2308 single-
lookup allocations/7.01MB requested/0.687MB peak, and735552 repeated16²-lookup
allocations/1953.07MB requested/1.706MB peak. Requested bytes are not copies.
Full copies are unmeasured; retained bytes and heap peaks are not added to RSS.

Linux peak RSS uses current-image `/proc/self/status` VmHWM. `getrusage` retains
earlier executable peaks across exec, reproduced by an explicit regression test.
The initial inherited5.35GB reading is invalid for this study; a clean full
recheck measured93.30MB. See [getrusage notes](https://www.man7.org/linux/man-pages/man2/getrusage.2.html)
and [kernel proc documentation](https://docs.kernel.org/filesystems/proc.html),
which also describes asynchronous RSS accounting. RSS is an OS estimate, not
an exact heap measure. Darwin retains the existing byte-unit rusage path.
Reports preserve actual commit, counts, load, nice level, CPU/wall and OS peak.
No equal-quality alternative speedup, GPU/device/p99/UMA or pose admission.
