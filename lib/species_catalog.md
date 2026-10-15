# Vegetation species pilot

[#226](https://github.com/Weichwolf/outshine/issues/226) owns this fixed,
versioned metadata and regional-candidate resource. The pilot contains
**30 accepted species, 31 explicit name requests and 2,068 occurrence records**.
Its installed JSON is 66,103 bytes. It spans European trees/understorey/grasses,
redwoods, Australian eucalypts, African dryland plants and tropical forms.
It is not the completed worldwide flora or a set of 30 accepted plant models.
Every record currently has `recipe_id: null` and `visually_accepted: false`.

```python
from outshine_ref.vegetation import load_species_catalog

catalog = load_species_catalog()  # once; caller retains this immutable object
taxa = catalog.candidate_ids(("SWI", "AUT", "GER"))
for taxon in taxa:
    record = catalog.records[taxon]
    print(record["scientific_name"], record["lifeform"], record["climate"])

introduced = catalog.candidate_ids(("CAL",), introduced=True)
same_taxon = catalog.aliases["Acacia tortilis"] == catalog.aliases["Vachellia tortilis"]
```

Region keys are **WGSRPD level3 botanical regions**, not ISO political country
codes. The geographic experiment in PR #241 supplies region candidates from
coordinates. This metadata API accepts those explicit codes; it introduces no
second world index or network/cache manager. Region lists are indexed once
when loading; queries union already prepared candidates without a flora scan.
Results are deterministic sorted taxon IDs, independent of region order and
duplicate query regions. No randomness, camera or render/LOD state is involved.

The default returns recorded **native, extant, certain** occurrences.
`introduced=True` selects **introduced, extant, certain** occurrences instead.
Introduced evidence is not an instruction to plant gardens or cities. All
extinct/doubtful records and original source locality IDs remain in metadata.
Missing pilot candidates do not mean a region has no vegetation. No abundance,
height suitability, fine habitat or ecological transition is inferred here.
For example, Switzerland's coarse region can contain Mediterranean *Quercus
ilex*; it must not consequently be placed across the entire Swiss landscape.
WCVP's broad climate/lifeform strings are retained verbatim; empty strings mean
missing data. They are not numeric local climate or morphology parameters.

IDs use the owned `outshine:taxon:wcvp:<accepted-plant-name-id>` namespace,
retaining the explicit WCVP identity rather than a sorting ordinal or mutable
name. All external name/accepted/IPNI/POWO identifiers remain distinguishable.
`aliases` contains only the 31 reviewed requests, not complete WCVP synonymy.
Both valid synonyms and eight explicitly disambiguated names retain the
selected source name ID/status. No first-match, fuzzy or genus expansion is
performed. Updates are explicit new resource versions, not automatic refresh.

`SpeciesCatalog(document)` also accepts a trusted structured pilot recipe.
It checks identities, occurrence columns/flags, explicit source IDs and alias
references, then copies/freezes its inputs. It is not a general untrusted
binary asset decoder. The JSON's `distribution_fields` describes compact rows:
`[region, introduced, extinct, doubtful, [source_locality_ids...]]`.
The three independent flags are integers 0 or 1; they are not interchangeable.

## Pinned data and attribution

Source: Govaerts R (ed.). 2026. *WCVP: World Checklist of Vascular Plants*.
Facilitated by the Royal Botanic Gardens, Kew. Version16, extracted4June2026,
[DOI10.34885/egs6-cp24](https://doi.org/10.34885/egs6-cp24).
Retrieved15October2026 from the [official data download](https://sftp.kew.org/pub/data-repositories/WCVP/).
The ZIP is 88,179,649 bytes, SHA256
`d32ea2b3a85e489b14e83bcc9eae7274532e1d113753f7be290d4b2dfde573fa`.
Its `README_WCVP.xlsx` explicitly states Version16, extracted04/06/2026 and
license [CC BY3.0](https://creativecommons.org/licenses/by/3.0/).
The packaged resource retains this attribution, link and adaptation description.
Source CSV/ZIP remain in system-temp; they are not bundled in this repository.

Adaptation: explicit selected species; verbatim name/trait strings; synonymous
requests resolve to accepted species; identical region/status combinations
group their retained source locality IDs. No source geometry, pictures, prose
descriptions or plant-model recipes are included. WGSRPD polygon redistribution
rights remain a separate open question; this license statement concerns WCVP.

Compiler/specification and independent fictional controls are under
`test/experiments/vegetation/species_source*.py` and
`species_source_targets.json`. The compiler requires the exact pinned ZIP hash,
bounded members, unique exact requests and unambiguous accepted species. A
first real run refused ambiguous *Pinus sylvestris* instead of silently using a
misapplied record. The specification now explicitly names the eight selected
source identities. It refuses missing, ambiguous, unplaced or inconsistent
names rather than turning them into plausible-looking accepted species.

```sh
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=test/experiments/vegetation \
  .venv/bin/python -W error -m unittest species_source_checks
PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=lib/src \
  .venv/bin/python -W error -m unittest discover -s lib/tests -p test_species_catalog.py
```

Full goal: botanical forms and optical acceptance remain #25/#22; actual
weighted altitude/climate/habitat selection, continuous transitions, cache
replay and full worldwide catalog remain #226. Stable candidate IDs alone do
not prove those later decisions or provide image/native/device admission.
