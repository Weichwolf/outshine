Type: task
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
