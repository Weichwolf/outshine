# Native source height identities — #1189 / #1003

`BuildSurfaceHeightTopology` consumes the prepared native planar arrangement.
Every used `(domain, planar vertex)` first gets a separate height identity.
Qualified ports list their domains in established order and carry their prepared
polygon, including holes. Only identities at the same planar vertex within that
region's explicit join distance can merge. Coincident unlisted layers remain
independent. The first present domain/root determines the representative, matching
the existing Python layout; successive ports preserve that rule transitively.

Domain/layer/access qualification precedes this operator. A geometric overlap
alone does not qualify a port. Original source IDs/facts remain with its caller.
Output source faces retain both height-variable indices and original planar-face
indices. Primary domain and original planar vertex are explicit per variable.
No height, gradient, source acquisition, outer-profile binding or stair loft is
inferred here. The existing height program consumes the resulting identities next.

Source incidence is accumulated per planar vertex. Sorted compact source keys
avoid a dense domain×vertex table. A point hierarchy selects port candidates;
only those undergo exact polygon-distance tests and ordered disjoint-set joins.
Source vertices, source faces, input planar vertices, port points and candidate
work have separate local limits. Refusal exposes no partial topology. Cancellation
is observed before work and between port candidates; no global GEOS state changes.
World, GPU, frame time and global optimality are outside this comparison.

```sh
make all db
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  python3 -B -W error test/experiments/transport/native_surface_height_topology.py \
  --arrangement /path/to/native-planar-arrangement-replay \
  --output "${TMPDIR:-/tmp}/outshine-height-topology-replay"
make suite SUITE=outshine/src/generators/road/SurfaceHeightTopology
```

First produce the arrangement directory using `native_planar_arrangement.md`.
Use fresh output paths and the existing experiment environment. The probe retains
complete compilation-database flags, source hashes, original inputs and terminal
results. Original and three rigid controls compare exact coordinate words,
primary domains, original planar vertices and every source/global face index
against the unchanged Python layout algorithm. Only its prepared geometry and
landing inputs are injected at that operator boundary; the union algorithm stays
unchanged. Stacked layers, ordered/transitive ports, highest domain bit, invalid
input, cancellation and refusal budgets have separate native controls.
The driver infers `--native-body` from its arrangement result, using the same
actual cut-domain/port input for the Python comparison. It does not fall back to
the initial uncropped source proposal. Height-profile equations remain separate.

Reported output vector capacities and hierarchy storage describe those components;
they are neither whole-process RAM nor summed simultaneous peak residency. GEOS
allocations and physical copies remain uninstrumented. This is once-per-asset
preparation; no visible runtime fix follows before height/body/nav/terrain/cache
integration. The isolated test profile links only these sources, the point index,
the shared polygon factory and GEOS; no renderer, network or height solver.
