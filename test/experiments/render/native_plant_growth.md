# Native spatial branch growth — #1

PlantGrowth transfers the bounded Runions-style competition used by the
[mature scaffold study](../vegetation/oak_scaffold.md). Seed positions/preceding
parent IDs and a source attractor cloud enter before any wood or leaf meshing.
Coordinates stay in the reference's Z-up growth space. The seed prefix remains
exact; each appended node points to an earlier node. Unreached source indices
remain in original order, with explicit consumption and termination statistics.
No fixed-depth recursive path is imposed on the spatial skeleton.

Each iteration finds source owners within influence distance, consumes nearby
attractors, sums unit directions per owner, and proposes one growth step per
noncancelled owner. Occupied endpoints are rejected. Earlier free proposals win
proximity conflicts; a rejected middle point does not suppress a later valid
point. Capacity truncates the accepted source-ordered proposals. A final query
consumes attractors reached by the last permitted growth iteration.

## Work and memory

The private index uses the established balanced median KD-tree procedure:
std::ranges::nth_element, alternating coordinate splits, exact squared-distance
tests, inclusive radius boundaries, and earliest source ID on exact ties.
Queries borrow positions and allocate nothing. Tree recursion has logarithmic
depth. Worst-case nearest queries can still visit every node; no unconditional
linear-work or globally optimal algorithm claim is made.

Existing system-library suitability was checked first. Installed nanoflann2.0.1
contains unconditional throws and has no no-exceptions switch; it was not added
to Outshine's no-exceptions build. No external header is bundled or exception
policy weakened. The native finite comparisons below include the existing
SciPy KD-tree and independent brute-force procedure.

Preflight validates dimensions, finite coordinates, representable IDs, ordered
noncollapsed seed edges and requested storage before allocating. All growth uses
nine arrays, requested bytes `89*node_capacity + 4*attractor_count`: positions,
parents, remaining IDs, sums, proposals, source IDs, free flags and two index
permutations. The byte-count formula follows actual array element sizes in code.
The allocation-tracking case verifies exact requests on this standard library;
this excludes allocator overhead, vector headers, stack, input arrays and RSS.
Finished vectors retain capacity; world product packing/cache integration remains
separate. Heap queries/rebuilds allocate nothing after the initial arrays.

## Actual verification

```sh
JOBS=4 sh test/run.sh outshine/src/generators/flora/PlantGrowth
PYTHONPATH=lib/src:test/experiments/vegetation python \
  test/experiments/render/native_plant_growth_reference.py \
  --binary /tmp/<native-consumer> --output /tmp/<fresh-cohort>
PYTHONPATH=lib/src python test/experiments/render/native_plant_growth_plot.py \
  --inputs /tmp/<fresh-cohort> --output /tmp/<skeletons>.png
```

Three cases pass normal, ASan/UBSan and container-hardening arms: exact index
versus complete search including ties/duplicates/boundaries, greedy conflict
chains, all terminal cases, seed preservation, invalid/cyclic/collapsed inputs,
allocation-free preflight/refusal and exactly nine complete-growth allocations.
Product units and independent native consumer pass full-flag clang-tidy. Case
analysis retains only existing Check.h/libc++ declaration findings (#603).

Twenty-four actual Python/native cases include twelve small clouds, eight
termination/duplicate controls and four full adult-study inputs, seeds682/718,
with trunk-only and six-primary-limb seeds. All node/parent rows, unreached IDs,
consumption, nearest-query counts, rejected endpoint counts, iteration counts
and reasons agree. Position differences are actually zero in this cohort; the
comparison's declared1e-9m absolute roundoff threshold is not a surface-error,
screen-space, radiance or motion bound. Small cases additionally match Python's
independent brute queries exactly. No existing tolerance or oracle was changed.

| Seed682 actual array/work inventory | Trunk only | Six main limbs |
|---|---:|---:|
| Final nodes / unreached attractors |5217/130|5472/122|
| Iterations / nearest queries |37/97425|31/65519|
| Native distance checks, including candidate conflicts |4319492|3573782|
| Full-pair checks for the same nearest queries |134033590|142519979|
| Additional partition comparisons |2548723|3111114|
| Requested product+scratch arrays B |1474560|1474560|

The last paired run under concurrent nice10 compilation/analysis took native
51.67–66.08ms versus Python121.79–149.44ms across the four adult cases. These are
individual component observations on this host, not p99, world/device budgets or
an isolated language-speed comparison. Earlier lighter-load observations were
43.58–49.27ms native; unchanged work counters distinguish load from algorithm work.

The actual two-view1600×1800 native skeleton diagnostic was opened: six main
limbs remove the overly dominant straight stem; the ellipsoid envelope and
angular secondary branches remain visible. Line widths use the study's authored
support-power taper and a minimum diagnostic subpixel width. This is a projected
skeleton, not a shaded mesh/leaf/Metal/world image or admitted adult species.
Source/binary identities, original inputs, complete rows, receipts, build and
terminal logs remain under `/tmp/outshine-native-plant-recipe-39bf9e07d/growth/`.

The existing world trees are unchanged. Next integrate foliage-host selection,
selected shoot poses and budgeted native products from this arbitrary-depth
skeleton; neither all90000 fine leaves nor full per-instance geometry is an
acceptable default. Current cached prototypes, near/far transitions and wind
still need their own actual world-image and cost evidence.
