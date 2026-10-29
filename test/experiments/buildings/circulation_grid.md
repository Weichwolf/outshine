# Bounded circulation grid preparation

Issue #1705 reduces preparation of the existing orthogonal circulation graph.
Terrace planning still tries the same doglegs and bounded grid before its existing
triangle-channel fallback. Every requested anchor contributes its coordinates;
graphs for different anchors are not interchangeable. No new cache is introduced.

## Method and decision

Keep coordinate selection, the40-coordinate axis limit, vertex/edge containment,
adjacency order, shortest-tree search, tie breaking and local failure responses.
Gather candidate edge endpoints once with an integer index array, then measure
only contained edges in a batch with the existing two-coordinate Euclidean norm.
Consume these lengths in the original adjacency order. Public routing inputs,
outputs and work counters are unchanged.

Compare the scalar baseline, gathered endpoints with scalar norms, batched norms
of all candidates and batched norms of contained edges on actual source queries.
The last method keeps rejected edges unmeasured. Measuring every candidate can
overflow on a rejected gap where the old solver returns a local failure. Replacing
norms with absolute axis distances also changes underflow/overflow behavior; that
shortcut is rejected even when ordinary physical examples match.

The selected method trades fewer Python/NumPy calls and allocation events for
additional temporary coordinate gathers. For E candidate and V contained edges,
guaranteed float64 coordinate copies increase from32E to32E+32V bytes. This lower
bound excludes integer conversion, arithmetic temporaries and geometry/native
internals. Total internal copied bytes remain unmeasured. Batched edge-norm calls
replace V scalar calls with one call per nonempty query. There is no persistent
memory increase; incremental heap peaks can nevertheless increase during a query.

## Reproduction and qualification

Use the project environment and explicit system-temp output paths:

```sh
git show 8f881e11b:lib/src/outshine_ref/buildings/interiors/routes.py > /tmp/outshine-circulation-reference.py
PYTHONPATH=lib/src:test/experiments/buildings:test/experiments/assets:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/buildings/circulation_grid_cost.py --reference-module /tmp/outshine-circulation-reference.py --output /tmp/outshine-circulation-costs
PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -m unittest test_circulation_grid verify_terraced_route_mesh
```

The driver requires frozen producers, normal process priority and a fresh output
directory. Nine rotating comparisons verify complete route WKB/order, all work
and failure responses, complete coarse body/facade arrays, triangle ownership,
portal/terrace plans and source IDs. Source loading and plan preparation are
reported separately. Graph-only inputs are already prepared; whole body building
includes fresh geometry/preparation, all actual routing and compact body joining.
Checks are outside timing; Memray/native-stack measurements are separate.

One actual body requests no graph, serving as a control. Failed bounded graph
queries remain failures; lower preparation cost does not create new connections.
Whole near/roof products and matched opened images are checked separately from
the cost scope. Tests cover equal-length hole routes, connected multi-anchor
trees, boundary/duplicate anchors, rejected gaps, disconnection and axis capacity.
The PR owns commits, exact outputs, terminal results and measured cost decisions.

The [SIGGRAPH2006 building paper](../../../doc/references/buildings/siggraph/2006-procedural-modeling-of-buildings.pdf),
p.4/Fig.8, was optically rechecked for the existing scope/component recipe. This
change preserves that recipe and the existing graph algorithm; it introduces no
new architecture generation or routing theorem. Python allocation/copy and CPU
results do not establish native World, frame, device or UMA acceptance. Building
appearance, broader architecture variety and native integration remain open.
