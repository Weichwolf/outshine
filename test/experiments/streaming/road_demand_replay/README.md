# Native road-demand replay

This optional Linux/Clang/libc++ diagnostic wraps the native demand, page-load,
asset-load, resolve and render entry points. Its explicit manifest selection is
a test input. It does not establish coverage or authorize product page suppression.
The GNU wrapper names bind the inspected libc++ ABI; other toolchains need their
own verified bindings.

Build against a prepared native client's complete compilation database and a
successful link receipt containing its actual `command` and `returncode: 0`:

```sh
python3 test/experiments/streaming/road_demand_replay/build.py \
  --compile-commands /path/to/native/compile_commands.json \
  --client-link /path/to/successful-client-link.json \
  --out /tmp/outshine-road-replay-build
```

The output directory must be empty and under the system temporary directory.
The client entry object must match the CDB. The probe declares its own
`libxxhash` and `libzstd` pkg-config dependencies. Build commands and terminal
statuses are recorded in `build.json`; both translation units have actual CDB
entries for focused analysis.

Create a fresh probe directory and set `OUTSHINE_CONSUMER_PROBE_DIR`. With prepared
inputs, running without `OUTSHINE_CONSUMER_SELECTION` stops at the restoration
entry, writes `consumer-manifest.json`, and rejects the missing selection before
road-page loads. Earlier preload failures may prevent reaching that entry.
Other world classes can perform work during this discovery.

Provide a JSON file with its exact `manifest_key` and `candidate_indices`, an
array of sorted, unique unsigned 32-bit page indices. Set
`OUTSHINE_CONSUMER_SELECTION` to that file and run the ordinary benchmark client:

```sh
OUTSHINE_CONSUMER_PROBE_DIR=/tmp/outshine-road-replay-case \
OUTSHINE_CONSUMER_SELECTION=/tmp/road-selection.json \
  /tmp/outshine-road-replay-build/ConsumerReplay benchmark \
    --scenario /path/to/scenario --offline --frames 60 \
    --output /tmp/outshine-road-replay-case/report.json
```

Only one manifest is accepted. Native selection validation succeeds before the
allowlist becomes visible. Excluded `LoadKey` and road-tagged `AssetCache::Load`
calls fail; `Resolve` always fails without invoking its factory. Frame records
retain camera serial, dimensions and complete Zstandard-compressed RGBA data.
`declared_asset_slice_bytes` counts requested native record lengths, not compressed
package or physical SSD traffic. Capture overhead is recorded separately and
changes execution; these runs do not measure a product frame budget.

`GuardCheck <fresh-directory>` exercises all three guards through actual native
API calls, with an independently readable packet and an ordinary cache miss as
positive controls. Set `OUTSHINE_CONSUMER_PROBE_DIR` to that same directory; it
receives the guard summary. The fixture writes only its own temporary caches.

The historical excluded-page image counterexample remains unresolved
([#990](https://github.com/Weichwolf/outshine/issues/990#issuecomment-6302365198)).
Identical full-page replays also exposed sparse image differences; GPU-input
identity and cause remain open. Functional, reflection, changed-root and global
world admission remain separate. All summaries keep
`qualified_for_page_suppression=false`.
