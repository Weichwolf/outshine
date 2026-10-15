# Weighted-height library acceptance

Owner: [#265](https://github.com/Weichwolf/outshine/issues/265), under #20/#50.
Frozen implementation: `720d417005a2b982ce8f0bf17076ccd757ffaf94`.
This completes the installable stage of the [PR275 experiment](camera_height_comparison.md).
Native integration/device acceptance remain #11/#48.

## Public contract and migration

`outshine_ref.transport.weighted_heights.fit_weighted_heights` uses existing
`HeightBudget`/`HeightFit` and an owned immutable `Mesh`. It fits positive weighted
vertex height priors under whole affine face-gradient cones. Fixed vertex IDs bind
to the explicit horizontal reference datum. Original XY/indices are exact; complete
support selection and source/frame/kind remain with the caller. A feasible prior
returns directly. Other candidates project pins and conservatively contract toward
the valid reference, then pass unchanged whole-face/contact limits. Finite candidates
worse in weighted RMS than that reference are rejected with a concrete reason.

The new module owns the only quadratic assembly/contraction implementation.
`camera_height_solve.py` now calls it; no experiment imports enter the kernel.
The minimax API, shared Plan/Reservation contracts, sources and runtime stay
unchanged. No new package or SDK dependency. [API, units, weights, ownership and
budgets](../../../lib/weighted_heights.md). Generic sloped reference meshes and
arbitrary unequal pinned heights are deliberately outside this verified flat-reference
operator; the existing other height/port methods retain their contracts.

## Source, installed wheel and numerical equivalence

- 376 library tests pass (existing Buildings environment includes optional extras).
- 52 affected controls pass independently from source and installed wheel, including
  eleven new public ownership/analytic/validation/budget/candidate tests.
- All 170 runtime package files equal frozen source bytes, excluding unpackaged
  documentation README files. The imported function's physical source/installed
  paths are recorded, so an editable checkout cannot impersonate wheel testing.
- Eight fresh default-profile workers (four source/four wheel) and their independent
  full saved-mesh replays pass. Each retains exact XY/topology and all complete
  rail-bed supports, satisfies unchanged whole-face/contact limits, and at least
  halves the weighted flat-reference RMS on these same full real inputs.
- On the entire Feldkirch input, source and wheel with the same 8-iteration work
  cap produce byte-identical complete NPZ meshes. This verifies fixed-work
  execution; it is separate from the normal 8 s /100-iteration quality profile.
- The legacy prototype and library pass **exactly equal** P/q/A/b data, CSC
  indices/indptr, shapes/dtypes and cone sequences to Clarabel on Feldkirch.
  The method's solver problem did not change during promotion.

At a wall-time limit, equal code/input does not imply equal iteration opportunity.
Wien/Tokyo default source/wheel heights are exactly equal. CentralPark/Feldkirch
have the following measured differences; all actual outputs remain pinned and
independently checked. No default-profile deterministic-output claim is made.

| Place | Source status / iterations | Source / wheel vertex RMS m | Source-wheel max height delta m | PR275-source max height delta m |
|---|---|---:|---:|---:|
| Wien | Solved / 26 | 2.088273 / 2.088273 | 0 | 0 |
| Tokyo | InsufficientProgress / 12 | 3.421947 / 3.421947 | 0 | 4.26325641e-14 |
| CentralPark | MaxTime / 18 | 4.230734 / 4.232128 | 0.17927609 | 1.42108547e-14 |
| Feldkirch | MaxTime / 21 | 18.894348 / 18.890097 | 0.392678857 | 0.392678857 |

On the same finite triangles, height differences attain extrema at vertices;
these measured deltas therefore also bound the difference of the finite affine
height fields. They are not bounds on the DEM or on a C1 field. All 60,554 original
hardware footprints remain fully supported in all eight outputs; preserved old
windows/hardware are also checked in actual ENU scenes. Normal geometry tolerance
remains 1+1e-7 normalized grade and 1e-7 m contact error. Source/wheel equality checks
did not replace or relax those physical gates.

Wheel SHA256: `af191f22cfcdb1a56bc572733911366f36321d93567caf027df1c316af2974f5`.
Same original four prepared input SHA256 values as the PR275 report; no input
refresh. Library/experimental implementation hashes are included in every worker
receipt. Runtime files are not imported from private experiment trees.

## Current costs with owned output

Python 3.13.5 /NumPy 2.2.4 /SciPy 1.18.1 /Clarabel 0.11.1; BLAS/OMP threads 1.
Default solver 8 s /100 iterations, input caps 131072 vertices /160000 faces as explicitly
chosen by these experiments. Address space 2 GiB, CPU 60 s and 90 s parent observation
guards; preparation/one-step overruns are separate from requested solver time.
Measured host reports an Intel i5-1135G7 with four logical CPUs; host/guest
identity and memory observations are retained in `machine.json`.
Minimal `Popen`/`os.wait4` supervisors record terminal state, CPU, actual child RSS
and before-load snapshots. Foreign CPU work is recorded; measurements keep normal
priority, builds/rendering run nice10. These are loaded-host offline observations,
not native cache loading, frame/GPU time, total OS memory or device capacity.

| Whole worker | Wall s | User+system s | Child peak MiB |
|---|---:|---:|---:|
| source-Feldkirch | 9.916 | 9.834 | 399.7 |
| source-Wien | 8.213 | 8.157 | 347.6 |
| source-Tokyo | 7.912 | 7.869 | 499.7 |
| source-CentralPark | 10.016 | 9.953 | 488.9 |
| current-minimax-Feldkirch | 9.915 | 9.830 | 528.0 |

The new positive method returns an owned full XYZ/Uint32 Mesh; the older prototype
returned only heights. Current cost profiles include that output work. The current
minimax Feldkirch run again provides no valid mesh; its lower output-copy count
must not be interpreted as equal successful throughput. Prior three-repeat method
comparisons remain in the PR275 report; these current rows are individual fresh
observations, not a new broad timing claim.

Separate one-call Memray 1.20.0 profiles trace Python/native allocators for the whole
fit after imports/input decode: wrappers, sparse preparation, solver, ownership,
projection and whole-face audit. Profiling changes timing-limited iteration
opportunity. Actual matrices are observed at the Clarabel call without copying.

| Work/storage in Feldkirch profile | Minimax | Weighted library |
|---|---:|---:|
| Constraint rows | 494376 | 299438 |
| Constraint stored nonzeros | 1285552 | 895676 |
| Hessian stored nonzeros | 0 | 97469 |
| Constraint CSC B | 15816508 | 11137992 |
| Hessian CSC B | 389884 | 1559508 |
| Positive allocation events | 5748829 | 4861775 |
| Cumulative requested B | 986688155 | 793880987 |
| Tracked heap peak B | 468836372 | 334390627 |

Positive realloc requests count; cumulative bytes are not unique/live heap. CSC
counts include stored zeros. Known explicit Mesh/frame/weight/candidate ownership
copies for these wrapper paths are at least 6711668 B for failing minimax and
14142160 B for successful weighted fitting, excluding contact-ID work and backend
copies. These are source-level lower bounds, not observed total memcpy traffic.
The weighted path's larger known output-copy work buys actual owned valid output;
sparse-row/heap/allocation observations are separate. Internal native copy totals
remain unknown. No global optimum or algorithm/hardware superiority is claimed.

## Actual pictures and retained quality

Four fresh complete 1280×720 Blender 4.3.2/Workbench renders from the clean library
outputs were format/dimension/hash checked and all opened against the PR275 gallery.
Camera matrices, complete indices/material slots, all 503616 hardware vertices and
old verified Vienna/CentralPark window vertices remain exact. Current gallery PNGs
stay because this library promotion has no clearly visible improvement; private
render evidence is also copied unambiguously into both checkouts under
`build/shots/reference/transport/weighted-library/`. No showcase server.

| Place | Changed RGB pixels /921600 | Mean absolute RGB error /255 |
|---|---:|---:|
| Wien | 0 | 0.000000000 |
| Tokyo | 0 | 0.000000000 |
| CentralPark | 0 | 0.000000000 |
| Feldkirch | 25367 | 0.102255136 |

Feldkirch's small raster variation follows the timing-dependent candidate, not a
new geometry recipe or demonstrated improvement. Whole geometric support remains
proved. C0/C1 distinction, terrain exterior, sloped rails/turnouts, traffic,
source/frame/kind binding, LOD, native costs/devices and full world completeness
retain the limitations in the API/experiment documents.

Reproduction uses the README camera-height commands, the existing pinned four
inputs and explicitly chosen external outputs. All source/wheel reports, equality
checks, allocation traces, process/load snapshots, selected outputs and final
scenes are retained in `/tmp/outshine-transport-weighted-evidence-720d41700/`.
Original inputs/rail products from PR275/260 remain available. No private output
or environment is added beneath the checkout's build tree. SIGGRAPH/paper
reasoning and official Clarabel formulation are linked from the original method
report; promotion introduces no new algorithm or foreign implementation.
