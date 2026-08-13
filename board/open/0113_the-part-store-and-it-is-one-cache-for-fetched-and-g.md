Type: feature
Area: generators
Tags: perf

**The part store, and it is one cache for fetched and generated content**

- [ ] **One store, and the key covers the generator's own version.** `data/ContentStore.h` already states the rule for fetched content — *"It covers the source's id, its declared VERSION and the address … That is the one failure this scheme cannot catch"* — and a generator is a source under the same rule. **A generator that changes its output without raising its version serves stale parts, and no cache can catch that from inside**
- [ ] **The budget is part of the key.** Two requests for one species at two errors are two parts, and a key that omitted the budget would serve the first answer to the second question — which is a silent LOD failure and the hardest class to see in a still
- [ ] **The cap is enforced during a run and not only at construction.** `ContentStore` trims once, in its constructor (`data/ContentStore.cpp:45,67-71`); `Keep` writes and never sweeps (`:106-130`). For fetched tiles that is a slow drift within a session; **for a city compositor at one unique part per footprint it is unbounded growth against a declared cap**, and the store is exactly where the city's *"distinct-part count equals instance count"* lands
- [ ] **Eviction is keyed by reuse and not by age alone**, because the two part populations are opposite: a forest's K prototypes are hot for the whole run and a city's N footprints are cold the moment the camera leaves the block. **An oldest-first sweep evicts the forest's prototypes to make room for a street the camera has already passed** — which is `e_StreamCgfVisObjPriority`'s thrash setting rediscovered in a different layer (Band 0)
- [ ] **The in-memory part cache and the on-disk store are two tiers of one thing and answer one request**, so *"is it resident"* and *"is it cached"* are one question with one answer, in the shape Band 0 already requires of tiles
