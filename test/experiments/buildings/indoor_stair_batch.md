# Joint indoor stairs and shared plate openings

[#1558](https://github.com/Weichwolf/outshine/issues/1558) composes the individually
checked [stair candidates](indoor_stairs.md) into one bounded final product.
Six different bodies represent seven compatible raw sources, with two final
shared plates. All12 whole endpoint interfaces and73 selected footway corridors
remain valid; the30 earlier XYZ contacts are exact. Three equal-height stairs
remain pending, including the original short request from#111.

## Source and product ownership

`indoor_stair_contacts.py` groups aliases only within the supplied snapshot and
common recipe. Require the same exact rational raw-grid path (allow reversal),
frame, chosen floor/height endpoints, width, attributes and construction limits.
The lexicographically first source owns one generated body; every original
source identity, frame, raw record and normalized axis remains in the ledger.
One tile-buffer duplicate shares its owner rather than creating another body.
This does not establish original OSM membership or globally stable entity IDs
across changing input snapshots. Native Source/World identity remains separate.

`prepare_stair` explicitly returns a candidate body. It supplies no independent
ready floor product. `build_stair` retains the existing single-source path and
qualifies its emitted plate plus final endpoint interfaces. Invalid frames and
end heights are rejected before body construction. `indoor_stair_inputs.py`
owns the common offline floor/port preparation for both comparison drivers.

`indoor_stair_batch.py` owns final composition. SourceFeature products contain
the seven represented identities in the common frame. Nonemitted sources remain
in the ledger with their own frame/axis and local reason, not mislabelled as
geometry in the output frame. Source aliases map to the same physical owner;
per-source attributes/axes remain distinct. Reservations belong to that owner.

## Whole geometry and valid partial output

Sort source groups deterministically and bound construction/output capacities.
Require each individual opening domain and initial/final contact to be valid.
For the current straight extrusions, convex hulls of actual mesh XY coordinates
are their complete body/headroom footprints. Positive-area separation proves
no interpenetration between these prisms. A positive overlap is conservatively
`joint_stair_prism_overlap_unplanned`, not a claim of a real3D collision or a
globally optimal accepted subset. Height-separated overlaps need future work.

Union candidate cuts per host plate before final construction. Recheck every
ready corridor, all tread headroom and both entire endpoint lines against final
plates, then compile/check actual plate meshes. A landing is an emitted stair
solid: the host need not underlie its entire pad after its own1mm opening guard.
The whole end interface must remain on the final host. This distinction avoids
mistaking the owned stair platform for missing floor.

If collective qualification fails, a bounded deterministic prefix construction
keeps valid candidates and records each rejected source. A counterexample has
two individually valid bodies with disjoint footprints but a real1mm opening
guard that removes the other's full-width port. The final result keeps one
valid connection. No candidate status or intermediate plate is released as a
joint ready product. Defaults bound16 bodies/64 construction candidates; pending
bindings do not consume construction work. These are local experiment limits,
not a whole-world content or device-budget acceptance.

## Equal-output cost comparison

- `eager`: construct and check each individual plate using the single-source
  procedure, then compile the final shared plates.
- `shared`: check individual domains/ports and defer plate triangulation until
  all cuts are composed. Qualify the same final bodies/plates/ledger/reservations.

Both have nine construction calls,15 footprint-pair checks,12 final port checks,
one successful final assembly and no fallback on the actual input. The eager
path qualifies six individual plus two final plate meshes; shared qualifies
only the two final meshes. Six bodies contain2464triangles/186976payloadBytes;
the two modified plates use54608payloadBytes and44.632922m² total opening area.
The single-source body, final floors and reservations remain byte-exact to the
qualified b9a2d61d4 artifact. Tests never regenerate pins or acquire sources.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -m unittest discover -s test/experiments/buildings -p 'test_indoor_*.py' -q
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/buildings/compare_indoor_stair_batch.py --output /tmp/outshine-research-indoor-stair-batch-comparison --rounds 3 --trace --plot
```

Use a new explicit temp directory. Same dependencies as the stair reference,
no new packages. The report separates source/DEM/ENU, expanded floor context,
alias preparation, warm composition, allocation requests/scoped peak, process
RSS and payload sizes. NPZ stores actual body and final floor buffers; local
figures show their actual side triangles, plate sections and top triangles.
No repository image publication. Copied bytes, finished asset-cache hits and
device/frame-p99 are not measured. Python CPU time is not native acceptance.

Merrell/Schkufza/Koltun2010, SIGGRAPHAsia,§6/Fig.11, inspected in the
[single-stair reference](indoor_stairs.md), motivates qualification after all
multi-storey access constraints. Exact-grid grouping, conservative prism tests
and deferred shared plate construction here are our own derivation.
Terrain/structural support, rails, walls/doors/access, original topology/native
Source→World admission and full-device/frame qualification remain open.
