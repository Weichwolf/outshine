Type: feature
Area: scenario
Tags: khronos, perf, instrument

**I.24 Settings in two tiers, and what makes the first one untouchable**

*Added 2026-08-12 on the owner's ruling: **the library carries JSON settings that generators and
providers require — defaults, and values a consumer should not change; everything else is set by the
client or the test.** The split half exists already and has never been stated: `assets/world/*.json`
(ground materials, vegetation classes, 34 species files) is the library tier and `mods/*/mod.json` is
the client tier, and nothing in the tree says so or enforces it.*

**The tier test, and it has to be sharper than *should not*.** A value belongs to the library when a
wrong value there is a **defect in the engine**; it belongs to the client when a wrong value there is a
**defect in the run**. Beech leaf length wrong → the engine grows a wrong beech → library. Camera at the
wrong latitude → the run looked at the wrong place → client. An upstream's zoom range wrong → the engine
asks for tiles that do not exist → library. Render width 640 → the run measured something other than
720p → client.

- [ ] Two readers, two types, and no key path from one into the other — the library tier is not *merged with* the client tier, it is a different object
- [ ] The library tier is `const` at the type level: constructed once, handed out as `const&`, **no setter exists**. *Untouchable* is then `C.12` and `Con.*`, not a sentence in a document
- [ ] The client tier is an **enumerated** surface, not an open one: the scenario schema of § I.4 names every settable key and a key it does not name is refused with its path — so *"everything else"* is a list rather than a hole
- [ ] A test overrides the library tier by **substituting a whole table, never by patching a key** — `FromSubstitute(tables, why)` beside `FromDeclared(storage, root)`, with `why` required and a run refused when it is empty. A patched key is invisible in the row it produced; a substituted table is a declared act
- [ ] The substitution's `why` enters the run identity, so a run built on a substituted table **cannot enter the archive looking like a shipping run** — the same defect class as § I.17's `client=gpu_walk` string literal
- [ ] The `why`-or-refuse shape is the one `clients/Scene.h:30-37` already uses for a render size that is not the budget's 1280×720 — this is that pattern generalised, not a new one
- [ ] **A number is a constant if changing it is a code change, and a setting if changing it is a data change. Nothing is both**, and the check is mechanical: no `constexpr` in a `const/` header may share a name with a key in any library-tier schema
- [ ] **No constant is a default for a setting.** A default lives in the library JSON where the owner ruled it lives; a constant has no alternative value at all. That gives every number exactly one owner and makes the previous line maintainable
- [ ] The library tier is read through `Host::Storage` (§ I.18) and never `fopen` — it is the same call on every target, and it deletes the three `fopen` sites in `world/`
- [ ] Every generator and every provider states **which library tables it requires**, so a missing table is a named refusal at load instead of a default nobody declared. § I.4's *declared strata list per ground class, with no global default, so an unclassified place grows nothing* is the same rule for one table
- [ ] A library table that no generator and no provider requires is reported — the drawer check of § I.23, applied to data
- [ ] The library tier's location is `assets/` and it ships with the library; the client tier's is the scenario. A test that declares neither gets a refusal naming both
- [ ] **A value lives in exactly one tier**, and a key present in both is refused at load with its path — the ruling's *everything else is set by the client or the test* is only enforceable if the two sets are provably disjoint, and disjointness is a check over two schemas rather than a habit

**A scenario carries its own data.** *Owner's ruling, 2026-08-12: a scenario must be able to provide
its own glTF assets, and to give a provider — elevation above all — data from a file inside the
scenario, so a run has correct terrain, depends on no network, and goes at the machine's full speed.
This was done once for the FlightBox gym.* **It needs no new mechanism, which is the evidence the
registry is the right shape:** a scenario-declared source is a `Data::Source` like any other — it
declares what it covers, it is registered at a rank, and everything else follows.

- [ ] A scenario declares sources of its own, registered above the network ones, so a covered request never reaches a wire
- [ ] A scenario declares whether the network may be reached at all; with it refused, the provider list is exactly what the scenario carries and exhaustion is the terminal absence — the same rule, not a second one
- [ ] A file-backed elevation source: the scenario names a file, the source declares the box and the zoom range it holds, `Covers` answers from the declaration without touching disk
- [ ] A scenario carries its own glTF assets, addressed relative to the scenario, so a subject is declared where the scene is declared
- [ ] A run with no network and no store reaches its verdict at the machine's speed, and **the same scenario with the network available produces the same answer** — that identity is the test, and it is the same shape as *cache on and cache off differ only in timing*
- [ ] A scenario whose declared file is missing, unreadable or does not cover what the scene asks for is refused **by name and by path**, never silently filled from the wire — a fallback here would make a deterministic run quietly non-deterministic
