# Native declared species mixtures

`SpeciesMixture` implements the constant-height case of the Python
`SpeciesHeightMixture` contract. Names define canonical CDF order; results retain
their original catalogue index. Half-open intervals skip zero support. Common
weight scaling preserves selection, including very large and subnormal inputs.

Shipping binds the explicit vegetation-definition file to ground-template rows.
Unknown names, duplicate keys and invalid weights fail transactionally. Empty
support stays empty. The standalone Forest constructor without mixtures remains
an unconstrained stem sampler; the engine always supplies its declared mixtures.
World and renderer receive native products, not species-recipe semantics.

```sh
make all db
PYTHONPATH=lib/src python test/experiments/vegetation/species_mixture_native.py \
  --output "${TMPDIR:-/tmp}/outshine-species-mixture-comparison"
```

The script compiles its C++ probe against matching production archives and flags.
It links the production process-allocation interceptor for tagged allocation
counts. Preparation's retained allocator bytes and sampling's cumulative allocated
bytes are separate. Inputs include the shipped mixtures, canonical-order changes,
CDF boundaries, zero support and extreme scales. Comparisons shuffle the variates.
Logs and raw inputs/results stay in system temp. `--binary` accepts an existing probe.

This proves categorical selection and local CPU costs. It does not validate
height-dependent ecology, site hashing, plant geometry, native-world rendering,
frame time or whole-process memory. Normal tests do not generate reference files.
