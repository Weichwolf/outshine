# Native urban-area source cook

Owner: #1102. Reference: #1105 / #1107, implemented under
`test/experiments/buildings/`. Native implementation:
`src/generators/osm/urban/`. This is the source-to-field step; native box/mask
rendering belongs to #1111. The default world is not connected by this comparison.

`GenerateUrbanArea` consumes immutable prepared `OsmField` tiles. The first tile
owns the output region; coarse contexts contribute urban evidence only. The
existing native provider/format layer supplies normalized properties and geometry.
No individual buildings or meshes are constructed. Source identities, payload
digests, provider feature IDs and every source disposition remain owned values.

The canonical projected lattice, deterministic height/vacancy recipe, exclusions
and broad exact gate follow the independent Python implementation. Rows run south,
with explicit global cell keys, projected origin and local metric scale. A missing
urban prior and a completely excluded prior remain distinguishable. These are
inferred render masses, not reconstructed functional buildings or measured heights.

Cell work is checked before GEOS/domain construction. Source/feature bounds,
cancellation and deadlines are checked during generation. GEOS union/difference
calls cannot be interrupted in their interior; run this cook on the bounded worker,
never the frame thread. No asset-cache hit path is implemented by this experiment.

## Reproduction

Requires the built native archive/CDB and the existing Python research dependencies.
Provide a JSON object with `owner` and one `urban_parents` receipt. Each is an
existing `outshine_ref.places` vector receipt (`kind`, `tile`, `path`, `url`, `bytes`,
`sha256`). No request is issued; original receipt bytes are verified before decode.

```sh
python3.12 test/experiments/render/native_urban_area.py \
  --receipts /tmp/urban-source-receipts.json --output "$(mktemp -d)"
```

The driver retains exact compiler/link commands, actual C++ standard-library option,
archive hash, source receipts, scalar work and complete byte equality of the
independently cooked categorical grids. Time scopes exclude provider acquisition;
these single runs do not establish a speedup. Ordinary tests create no pins.
The native analytic case also checks water/park/corridor subtraction, underground
hints, ownership, absent/fully excluded evidence, limits and cancellation.

Terrain/Earth curvature, global metric/neighbor/LOD coherence, prepared-asset IO,
native rendering cost and complete-world 1280×720/60 remain separate work.
