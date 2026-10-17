# Corpus preparation

`prepare.py` fetches pinned inputs for HTML/CSS, JavaScript, geodesy, transport
and atmospheric reference tests. Preparation is separate from execution.

```sh
python3 test/harness/shared/corpus/prepare.py dry-run --every-case
make corpus-prepare MANIFEST=test/geographiclib/geodesic/geodtest-short/manifest.json
```

`dry-run` validates declarations and reports missing source bytes without network access.
`fetch` and `all` acquire the declared files; `--force` replaces prepared copies from
the content store. `wpt-cases` and `test262-cases` create declarations from explicit
upstream commits. Each command lists its required arguments with `--help`.

`manifest-schema.json` defines accepted fields, source kinds and file roles.
Unknown fields, duplicate output names and undeclared entries are errors.
Each file retains its URL, digest, size and licence declaration. The owning
harness supplies source and redirect validation; shared fetching verifies bytes.

Prepared inputs live under the system temporary directory's `outshine-prepared/`,
using the test path with separators replaced by hyphens. Source bytes are shared
under `outshine-content/`. `--dest` and `--store` override these locations.
`provenance.json` records preparation results and `.prepared-by` records runner
ownership. Neither is a test verdict or an input substitute.

See [the corpus inventory](../../../CORPORA.md) for retained families and their
integration status. Normal tests never update pins or expected values.
