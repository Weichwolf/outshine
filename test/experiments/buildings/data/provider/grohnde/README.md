# Grohnde complete provider tile

Fixed offline input for prepared-building experiments: the original complete
OpenFreeMap tile, not a hand-authored building or reconstructed model.
The filename is SHA256 of its URL, matching the existing source-store convention.

- URL: https://tiles.openfreemap.org/planet/20260927_080001_pt/14/8620/5409.pbf
- Bytes: 9441
- SHA256: c91248b4db2a57d2dad79c49b8703f200bd6ec8177616a054bc207702b05c4fe
- Original inventory/latitude: `doc/references/data/osm/openfreemap-inventory-20261009.json`.
- Data: © OpenStreetMap contributors; https://www.openstreetmap.org/copyright
  and https://openfreemap.org/ (ODbL source data).

`read_tile` and `contexts` use the same actual decoder/normalization with this
explicit source directory. All building/landuse features and other context layers
remain in the original bytes. `view.json` pins the earlier own industrial-group
comparison camera (1920×1080), with no terrain or external image.
