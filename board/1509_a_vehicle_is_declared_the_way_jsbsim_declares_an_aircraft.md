Type: task
State: active
Parent: 1581
Area: world, assets
Tags: scope, declarative
Supersedes: 1511

# A vehicle is declared the way JSBSim declares an aircraft, and every number carries its origin

**Benchmark** — Unreal: a wheeled vehicle is a plugin's data asset with engine curves, gearbox and tyre model. RAGE: `CVehicle` handling data in the game layer. **Both agree** on the shape — a vehicle is DECLARED data, not an engine type — and JSBSim is the same shape with a physical vocabulary, which is why it is the model here.

JSBSim is the QUALITY BAR, not a thing to copy: an aircraft is declared in XML — mass and
balance, an inertia tensor, ground reactions as spring/damper contact points, propulsion,
aerodynamics as coefficient tables — and the simulator assembles it. No aircraft is hard-coded,
which is why one program flies a Cessna and a 737. A car is the same shape with different
tables, and the declaration is a row of OUR scenario, read by the same grammar and refused by
the same sentence when it is misspelled — one format, one reader, one place a typo is caught.

The first asset proves why the numbers cannot be a convention the asset must satisfy: the F31
(2014 BMW 3 Series, CC-BY-4.0, 519 nodes) carries **no semantic node names at all** — the
exporter grouped them by material. There are no nodes to name, so the asset is MEASURED once
(the tyre material's points near the model's lowest point separate cleanly into four contact
patches of 2873 points each) and the numbers are declared with where they came from.

## What will be true

- [ ] `<vehicle>` sits beside the other scenario rows: inertia, contacts (spring, damper,
      travel, bump stop, link limit), tyre, drive, brake, body — and the physics assembles it.
- [ ] Every number in the shipped vehicle traces to a measurement or a published dimension;
      the spring and damper rates are DERIVED from a declared ride frequency and the load, never
      guessed; the eye height is confirmed rather than estimated.
- [ ] The asset is fetched and pinned like every other corpus subject, with its attribution
      travelling beside it.
- [ ] A vehicle the declaration cannot satisfy refuses by name at assembly.
