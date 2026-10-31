# Native package IO comparison

This experiment compares publication and fresh offline direct-key reads of the same
existing native packages. It does not run generators, source acquisition, typed
consumers, World or rendering. Filesystem caches are not cleared. Run measurements
without concurrent builds/lint; retain the sampled system load.

Build `test_experiments_assets_NativePackageIo` in both revisions with identical
compiler flags, benchmark source and dependencies. The program explicitly enables
SQLite allocation counters before its first SQLite call and rejects unavailable
counters. Preparation, publication, opening, read planning, loading and closing are
reported separately. Input validation/decompression and verification hashes are
outside the publication/read timings. Process RSS includes the input and validation
work; `wait4` peaks can also include inherited pre-exec memory. SQLite peaks measure
only SQLite allocations above the baseline reset immediately before publication.

Inputs are immutable private SQLite snapshots with the schema-4 `assets` and
`packages` tables. Copy the complete original compressed package and all its canonical
asset rows in one read transaction; preserve codec, CRC, native length and package
key. Do not migrate a shared cache to obtain inputs. The driver opens inputs read-only,
validates CRC and full native package SHA, and republishes every member unchanged.
Each package must fit the explicit 256-MiB experiment bound; oversized input fails.

The comparison accepts a JSON object with an `inputs` array. Each entry supplies:
`name` (unique plain path component), `kind`, `input` (absolute snapshot path),
`input_sha256` (snapshot file hash), `key`, `package`, `native_bytes`, `slice_bytes`
and `records`. Snapshot provenance and the original cache schema/size/mtime should
be retained alongside the manifest. Existing cache contents without generator commit
provenance demonstrate stored native outputs, not current generator acceptance.

```sh
python3 test/experiments/assets/compare_native_package_io.py \
  --inputs /tmp/private-inputs/inputs.json \
  --before /tmp/before/build/test_experiments_assets_NativePackageIo \
  --after /tmp/after/build/test_experiments_assets_NativePackageIo \
  --before-revision BEFORE_COMMIT --after-revision AFTER_COMMIT \
  --output /tmp/private-comparison --pairs 7
```

The output directory must be new and outside the checkout. Each pair alternates
revision order and uses a new destination database. Each write and read has its own
process. The reader receives only its destination, key and expected slice SHA; it
cannot fall back to the input snapshot. Package identity, record count, compressed
size and loaded slice bytes must agree. Raw stdout/stderr, process status/resource
usage, incremental samples and final per-case medians remain in the output directory.
This is component evidence; it proves neither frame p99 nor whole-application RAM.
