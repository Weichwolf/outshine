Type: bug
Area: generators
Tags: perf

**Picture**

- **The canopy changes colour with distance further than the air can explain.** Seen in
  `demo/walk` from the shipping wasm module `70993b0f2d7a5327` in Chromium 151.0.7922.34, 1280x720,
  sun at 11.2 deg elevation: the crowns in front of roughly 100 m are a saturated yellow-green, and
  every crown behind that is a pale, low-contrast near-white that reads as a row of identical
  lollipops along the whole treeline. Aerial perspective over 100-400 m of clear air at that sun
  angle moves a canopy by a few percent, not from mid-green to near-white, so the step is the tree
  representation and not the medium. **The mechanism is not isolated and the still cannot isolate
  it** — a single frame cannot separate "the impostor bakes a different albedo" from "the impostor
  bakes a different light" from an LOD cut placed where the eye can see it. What decides it: the same
  standpoint with the impostor distance moved outward, two frames, one difference; and the same walk
  in motion, because if the two representations disagree in colour then every stand that crosses the
  cut pops, which a still cannot show at all.
- **The demo road reads as a dirt track** since the unmapped substrate landed: the ground fragment uses the default row as the **runner-up** class where the structure has no second hit.
- **Crowns are too transparent at 30–80 m.** A stand reads as a wall of white trunks with a green fringe; no canopy closure. Opacity, not form.
- **The bow-tie crown persists**, reduced but not eliminated — two crowns in `horizon-after.png` still show a straight diagonal seam.
- **A near trunk reads as a straight grey slab**, not as a beech.
- **The hornbeam hedge reads as a young plantation** — ten stems in a row with a bare lower third. The grower has no cut response, which is also what blocks coppice stool and pollard.
- **Leaf lamina is wrong on small dense plants** — box comes out ~8 cm against a real 2 cm, because `CardLeafM` solves LAI ÷ crown projection ÷ card count and few cards means huge leaves.
- **The poplar's stem stands at 84 % of its own buckling height.** `assets/world/species/poplar.json`
  declares `height_m 30` and `dbh_cm 25`, derived from `H/D = 1.20`. The *convention* is right — height
  in m over DBH in cm is the forestry slenderness ratio × 1/100, and the file's spruce at 0.85 → 85 sits
  exactly in the documented Norway-spruce snow-break band, so that derivation is sound and is not the
  defect. The *value* is: 120 is past every published stability threshold, and Greenhill's limit says so
  without appeal to forestry practice. `h_crit = 0.792 (E/ρg)^{1/3} d^{2/3}`; with `E ≈ 1.0e10 Pa` and
  `ρ ≈ 700 kg/m³` for green poplar, `d = 0.25 m` gives `h_crit = 35.6 m`, so a 30 m stem is at 0.84 of
  critical where real trees stand at 0.2–0.6 (McMahon 1973). Run backwards at 0.6 the same relation gives
  `d ≈ 0.42 m`. `dbh_cm` is what the grower solves its whole radius cascade against, so the error is the
  drawn trunk: a 30 m mast rather than a tree. Right: `dbh_cm` 40–60, i.e. `H/D` 0.50–0.75, and the
  origin string amended — the crown of `Populus nigra 'Italica'` being narrow lowers `h_crit` further
  rather than excusing the slenderness, so the reason currently written there argues the wrong way.
