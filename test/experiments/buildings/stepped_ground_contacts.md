# Ground contacts of stepped building parts

[#1007](https://github.com/Weichwolf/outshine/issues/1007) owns the native repair.
The fixed input contains two actual prepared buildings, all their physical part
rings/holes, the stored whole-footprint contact planes and two adjacent terrain
samples from the current Flensburg world. Coordinates are metres in the recorded
ENU frame. Part ring Z is the physical standing plane less the existing plinth.
Elevated parts are retained in the input and excluded from ground support.

```sh
"$BUILDINGS_PY" test/experiments/buildings/stepped_ground_contacts.py --output "${TMPDIR:-/tmp}/outshine-stepped-contacts/result.json"
```

Use Python with NumPy and Shapely. No network, cache or private files are needed.
The observer was linked against the unchanged native product at `7dfcc6bfe`; its
complete 60-frame world capture is pixel-identical to the normal current shot.
The exact prepared key was captured from the loaded building field. Surface-cell
products were then loaded and checked against their index headers before export.
The recorded layouts have source object ID zero; no source identity is invented.
The asset keys and prepared geometry establish product provenance instead.

The whole-footprint minimum predicts the recorded 3.770308 m drop over 0.688739 m.
At the lower sample, the actual higher wing permits 15.785551 m ground, while the
blanket contact cuts to 12.515096 m: 3.270455 m of unnecessary excavation.
Independent local cut-only contacts predict a 0.499853 m drop for this edge.
That prediction is neither a new runtime image nor a general terrain solver.
The production repair must persist separate physical support polygons/planes,
consume them without loading fine shapes and preserve original cache readers,
courtyards, frame transforms and elevated-part semantics. Actual rendering and
miss/hit costs remain required after native integration. Smoothing the conflicting
whole-footprint heights cannot recover their missing spatial ownership.
