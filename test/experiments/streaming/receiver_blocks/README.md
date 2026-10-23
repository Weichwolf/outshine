# Conservative receiver blocks

This isolated CPU procedure replaces individual reconstruction of opaque depth
samples with outer bounds of screen blocks. It consumes an already prepared
root depth image and does not acquire sources, generate assets or change shaders.
The replay convention is explicit: depth `d` maps to camera clip `z = 1 - 2*d`.
The inverse matrix must belong to that same convention, pose and depth image.
It must not be confused with an arbitrary inverse GPU view-projection matrix.

The mathematical light coordinate is a ratio of affine functions of camera NDC.
For fixed x/y and positive denominator its derivative in z has constant sign.
Its extrema therefore occur on the two z faces of the NDC/depth box. Outward
interval evaluation over x/y at each face bounds both faces. The existing
sequential CPU evaluation has its own uniform forward-error bound, prepared once
per image. Each operation contributes `u*max_abs + eta`, with `u=2^-53` and
`eta=2^-1074`; positive error arithmetic rounds upward. For division the propagated
error is at most `(E_a + max_abs(a/b)*E_b)/min_b`, plus local rounding. The source
operation graph is retained; there is no guessed world-space epsilon.

Native preparation requires IEEE binary64, round-to-nearest, gradual underflow,
strict sequential compiler flags, positive homogeneous denominators and the
current affine shadow projections. Error domains are stored and checked before
reuse. Unsupported prepared input remains unknown. `Append` rejects malformed
extents; callers retain demand on false. Unsupported arithmetic/projection fills
the receiver mask instead of suppressing demand. The mask must cover the same
fixed root/light query; lifecycle/generation binding remains the caller's job.

Large projected blocks subdivide; one-pixel leaves use the unchanged scalar
sequence. A monotone receiver bitmask allows another shortcut: if its complete
conservative rectangle is already marked, no child can add a needed cell.
This online containment read makes the deferred difference-array method from
#1029 unsuitable for this consumer. Marking more receiver cells is conservative;
these bounds are never used as opaque occluders.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 test/experiments/streaming/receiver_blocks/run.py \
  --compile-database /tmp/native/compile_commands.json \
  --frame /tmp/root-capture/frame-0.json --output /tmp/receiver-block-check
```

The empty output directory must be under the system temporary directory. The
native compilation database supplies complete PreparedRoadManifests flags.
The bound/mask test links only C++/JSON; camera export additionally consumes the
prepared checkout's base math headers, not its engine archive. Builds/tests use
nice 10. Receipts record source/input/compiler-data hashes and terminal results.
Checks cover exact rational bounds, nominal scalar results, out-of-domain queries,
unsupported rounding, float64 successor/predecessor bit patterns, online rectangle
containment, sparse/empty/discontinuous frames and unchanged repeated frames.

`adaptive.py` compares Cartesian, projective-corner and prepared-face bounds on
an explicit capture. `probe.py` checks selected blocks point by point. An explicit
native reference descriptor may replace repeated Python scalar reconstruction;
frame/depth/mask hashes are checked before use. Python times are not native times.
The original Cartesian approach remains a measured negative control; it loses
coordinate correlation and causes excessive subdivision.

Background: SIGGRAPH 2015 GPU-driven rendering, depth hierarchy/min-max discussion
on pages 51–52 of the repository paper. That GPU pyramid is not this CPU receiver
procedure, and its published device times do not transfer to this experiment.

All results remain unqualified for page suppression. Equal/superset masks of an
existing receiver hypothesis do not prove its source geometry, raster precision,
transparent/functional/reflection completeness or temporal resource lifetime.
The finite replay and identical display buffers do not prove worldwide behavior,
A18 execution, faster world loading or the 1280×720/60 p99 budget.
