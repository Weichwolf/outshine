# Shared 512-pixel radiance-prefix component input

Owner: [#1762](https://github.com/Weichwolf/outshine/issues/1762).
Existing stored-source input from the #1775/#1762 native Metal campaign, shared
unchanged for the Iris counterpart. Two cached products contain the catalogue
and one captured material view. No network, atlas acquisition or capture on hit.
The request binds its atlas digest, provenance, bounds, view and light.
This is a relight-plus-prefix component, not a complete World/frame benchmark.
It does not qualify original-source coverage, PBR error, motion or runtime admission.

Run from the repository root, with a freshly empty host-temp directory:

```sh
case_dir=$(mktemp -d "${TMPDIR:-/tmp}/outshine-prefix-512.XXXXXX")
gzip -dc test/outshine/data/render/radiance-prefix-512/assets.sqlite.gz > "$case_dir/world-view-assets.sqlite"
cp test/outshine/data/render/radiance-prefix-512/input.json "$case_dir/input.json"
build/test_experiments_render_NativeBuildingImpostors "$case_dir" --radiance-world
```

Keep the native binary fixed while comparing normal shader-package variants.
`radiance-result.json` reports `field_relight_filter_gpu_ms`; do not relabel the
pure prefix-driver time as relight-plus-prefix. Require `field_hit: true` and
`source_readback_bytes: 0`. There is deliberately no atlas fallback file here.
Compare all five complete `field-radiance-world.{linear,depth,normal,rgba,ss1}`
readbacks; retain commands, binary/shader identities and terminal results.
Temporary assets, logs and readbacks remain outside the repository.

SHA-256 of gzip: `ed58cf62f0e70afc21bd70fdf1f162ec95ebb66ac6acd4a3f4d308a38d289fa0`.

SHA-256 of expanded SQLite: `7654f6c59f276266e55cbd13c8fab4a15f28f462e46444883236817dead90b38`.

Expanded 503808 bytes; compressed 458030 bytes.
