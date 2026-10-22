# Candidate water domains from class and provenance (#880 / #760)

The existing physical fit used a manually selected set of 13 original Ill surfaces.
`water_body_domains.py` now constructs candidate connected planar domains from
source polygons and their separately decoded input/provenance records. The whole
550-surface WATER3 observation recovers that set automatically after its actual
record permutation, without selecting a Place or reading the old selection.
The old selection is used only by the comparison driver as an independent check.

## Recipe policy, not hydraulic admission

Every source index addresses the caller's complete original polygon table.
Original rings, levels, IDs and metadata remain in the immutable source input.
The function consumes an explicit common planar frame; it chooses no geographic
chart, reprojects no source itself and invents no global body ID or cache.

`assume_ground=False` is the default: no unrecorded ground-level relationship is
inferred. Passing True is an **explicit procedural hypothesis**, not evidence from
the native false/default bridge/tunnel flags. It enables the following candidates:

- Ordinary `water_polygons` river pieces may join under the same provider origin,
  vector kind, provider namespace, nonempty revision, layer and class.
- Lake/pond/pool/dock/ocean candidates additionally require the same nonmissing
  provider feature ID. Zero is preserved as an ID; missing is distinct.
- Bridge/tunnel/nonempty crossing, unknown flags/classes/layers, missing provenance
  or missing still-water identity remain standalone candidates with a reason.

Only positive intersection area or length joins compatible candidates. A point
contact is insufficient; envelope overlap alone is insufficient. Metadata has
already been checked by the pinned WATER3 reader; this is not a replacement decoder.
Different metadata revisions are never silently equated. Provider IDs are not
promoted into original OSM identities. None of these rules proves hydraulic
connection or supplies a water datum, hard contact or observed flow direction.

## Bounded work and local absence

Source count is at most 1024; source coordinates at most 131072. One semantic
bucket retains at most the caller's candidate cap (hard maximum 32768 pairs).
Bounding boxes create potential components before exact geometry operations.
If all required relations of one potential component cannot fit the remaining
global relation budget, that component is deferred atomically; independent cheaper
components/buckets continue. Relation and union failures retain original indices
and reasons. Every source appears exactly once in a candidate body or rejection.

A candidate-enumeration overflow defers its entire semantic bucket because its
potential components are not fully known. This is explicitly a wider refusal than
the exact-relation component budget. Other buckets still continue. It is never an
empty/free-space claim. Unknown source geometry remains in the caller's original
table; the prototype does not replace prior valid runtime products.

Two measured broad phases produce the same envelope pairs: simple numeric box
pairs and scalar STRtree queries. The latter allocates at most one bucket's hits
per query and retains only the bounded pair list. The actual corpus has small
semantic buckets: pair bounds were cheaper than building/querying their trees in
the local Python observations, so the reference defaults to pairs. The tree is an
explicit checked comparison, not a claimed native/hardware winner.

These bounds count structural work and geometry calls. A GEOS operation remains
non-preemptible; its internal traversal/allocation/time is not bounded or measured
by the call count. No native frame/load/heap admission follows.

## Whole original comparison

With the declared ground hypothesis, all 550 real observed surfaces yield 490
candidate domains, with no omitted source. Complete pair testing inside compatible
metadata buckets plus independent breadth-first graph traversal checks every
membership. Each output geometry is compared with the complete original-source
union. An additional pair-box/tree comparison checks the same full assembly.

The observed Ill group contains the same 13 original surfaces, including the
WATER2-index145 → WATER3-index168 permutation. Its zero mesh has 1492 vertices,
1492 faces and the original island. Vertices/triangles are exact against the
existing canonical reference. With its explicitly supplied old laboratory
course/target/direction recipe, fitted physical vertices and actual P/A/q/b are
also exact. This does **not** automate course association, flow hypotheses, datum
or height fitting for the other domains.

Ground inference disabled returns all 550 original standalone candidates with no
broad-phase work. An exact-relation budget of ten returns a valid explicit partial
partition and reasons for unresolved sources; no global success is fabricated.
Controlled tests cover classes/revisions/IDs, crossings, point/line/area contacts,
holes, seeded geometry against full pairs, local failures and budget boundaries.
The actual 550 polygons have no bridge/tunnel flags; crossed-polygon checks are
synthetic, not a falsely advertised actual aqueduct counterexample.

## Reproduce and costs

```sh
PYTHONPATH=lib/src:test/experiments/water .venv/bin/python -W error \
  test/experiments/water/test_water_domains.py
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error test/experiments/water/compare_water_domains.py \
  --output "${TMPDIR:-/tmp}/outshine-research-water-domains"
```

Existing NumPy/Shapely, pyproj/PROJ, physical-reference dependencies and Matplotlib
are used. Outputs contain source memberships/policies, deferred reasons, both
broad-phase costs/work, exact WKB payload and zero/physical Ill arrays, plus a
local 1280×720 diagnostic. Input acquisition/projection, complete-pair graph
control, full assembly and physical reuse have distinct cost scopes. The raw graph
oracle omits validation/union work and is not a full-assembly speed comparison.
Source/query/pair/geometry counts, retained array bytes, traced peaks and process
RSS are measured; total allocations/native bus/GPU times are unavailable.

SIGGRAPH-first: local Gottschalk/Lin/Manocha, *OBBTree* (1996), pp.1–2 and
p.7/Figure1 textually/optically inspected: conservative bounds precede exact
geometry. No OBB/SAT code is copied. The implementation uses existing Shapely
geometry operations, simple components and independently checked BFS controls.
Native cooker/source admission, physical connections/datum/region/terrain/bridge
contracts and current Place/device gates remain #760/#48 work.
