Type: task
State: open
Parent: 1480
Area: data
Tags: scope

**Providers are declared by the scenario rather than compiled in**

[MEASURED] `Data::RegisterDeclared` is an `if` chain of three provider nouns -- `StarBands`,
`TerrariumDem`, `VersatilesVector` -- gated by one boolean, where `CLAUDE.md` promises *external data
behind a provider interface, ranked, absence hands over* and `board:1480` says the scenario declares
which. **Found by `board:1483`'s sweep**, and it is a feature not reached rather than a silent
truncation: a rank clash refuses by name.

## What must be true

- [ ] **A scenario declares 0 or 1..N providers**, each with its kind, its pin and its rank
- [ ] **A kind the engine does not carry is a refusal naming it and listing what it does carry**
- [ ] **Absence hands over to the next rank**, which is the architecture's own sentence, and the
  hand-over is COUNTED so a run can see how often it fell through
- [ ] **A provider is reachable without a network** in a test, or the suite's verdict depends on the
  weather
- [ ] **`WithUpstreams` disappears**, because a boolean that means *three particular providers* is a
  name for a list somebody wrote down twice

---

Progress -- all five boxes stand: RegisterDeclared takes the scenario's Provider list
against the engine's catalogue (terrain, vector, stars); a stranger kind refuses naming it
and listing the catalogue; two of one kind refuse ("a lookup with two answers has none");
ZERO providers is a valid declaration (nothing registered, nothing assumed); the shipped
terrain-vector-stars battery is ShippedProviders(), a convenience every client SELECTS
explicitly (six driver call sites, Sim, the ground tests); WithUpstreams is gone; the stars
provider stays file-backed so the proof owes nothing to the weather. Hand-over stays
SourceSet's counted Ledger.HandedOver. GroundStack::Open and Sim::Provision carry the
declared list. Collateral repaid: the legacy namespace outshine::Scenario (Scene/Sim world)
collided with the public struct Scenario the moment data saw the declaration -- renamed to
outshine::SceneLegacy across its fourteen files, which is the truthful name for the
CURRENT-red facade it serves. Proving test:
unit/data/AProviderIsDeclaredAndTheCatalogueRefusesAStranger.cpp. Residue: the ENGINE door
handing Declared().Providers into its own assembly path once the drive folds behind it.
