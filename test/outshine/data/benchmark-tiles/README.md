# Paired Metal benchmark

The fixture catalog holds geographic cameras for the Körbersee slope and the Malcesine shore,
plus the generated terrain receipts used for the single-tile path. Both renderers replay the
same scenario for one second each in still, turn and flight cases. The flight velocities keep
the stationary camera's terrain tile in view during a low-altitude pass.

Set `fixture_dir=test/outshine/data/benchmark-tiles`. Create `products` and choose a fresh
`work` directory in system temp. Export the prepared products from the local asset DB.

```sh
jq -r '.[] | [.scenario, .source_asset_key, (.page[] | tostring)] | @tsv' \
  "$fixture_dir/fixtures.json" |
while read -r scenario key zoom x y; do
  PYTHONPATH=lib/src python3 -B test/experiments/render/export_surface_tile.py \
    --cache build/assets/assets.sqlite --key "$key" --tile "$zoom" "$x" "$y" \
    --output "$products/${scenario%.scenario}.json"
done
python3 -B test/scripts/paired_render_bench.py --fixtures "$fixture_dir/fixtures.json" \
  --products "$products" --work "$work" --shots build/shots/paired-render
```

Each repetition builds the world in its own empty generated-asset store from local source
data, then reruns the same replay against that store. The tile path reads the same explicit
prepared product for each repetition. The JSON records both cache scopes. The temporary
generated stores are removed after the warm sample. The driver keeps raw reports in system temp
and one latest screenshot per renderer and case under the requested shots directory.

The report retains separate scopes for the full world and the single opaque tile. The tile
pass time covers its entire single-pass Metal submission. World pass times come from native
encoder counters. The last-scene geometry counters are scoped to their respective stages.
The world upload number covers the last reported geometry rebuild and the
tile upload number covers its initial vertex and index payload. These measurements track
progress in each scope. Whole-world comparisons require both paths to draw the same content.
