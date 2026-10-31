# Feldkirch: native shore / road contact

Input capture for #1908 / #826; no repaired geometry or expected solver output.
All files work offline. `python3 verify.py` checks identity and topology references.

| Input | Meaning |
|---|---|
| `contact.json.gz` | Original frame, 18,493 selected whole stamps, all 7,522 water vertices / 6,507 faces and face/source incidence |
| `water.native.gz` | Complete original WaterAsset, including source identities, inputs, rings, levels, prepared mesh and rejections; decode with the captured native codec |
| `roads.json.gz` | 127 whole rendered streets parts (178,164 vertices / 171,506 triangles), original page/plan references and 2,145 local source-way observations |
| `road-bindings.json.gz` | Actual plan-to-lane/contact-key and loaded-page-to-render-part bindings; group 106753 has 559 lanes / 125 whole mixed render pages |
| `road-overlap.json` | Double-precision membership check: neither diagnostic point is on a captured rendered road triangle; this is not a GPU/navigation oracle |
| `deformed-pages.npz` | Four existing z20 pages around the observed edge; immutable float32 35×35 words including halo |
| `dem-16-34514-22987.webp` | Original cached Mapterhorn Terrarium source; RGB: R×256+G+B/256−32768 metres |
| `input.json` | File hashes, capture/source identity, page metadata and native observations |

Frame: WGS84 anchor `(longitude, latitude)`, ECEF origin and E/N/U axes in metres.
Water vertices use their separately declared ENU anchor. Do not mix frames.
Stamp ordinals retain positions in the original 79,106-element sequence; selected
arrays are sparse. `pads`/`basins` describe original sequence ranges, not local counts.
Whole stamp bounds expanded by apron/support select the local rectangle; every
stamp of each selected nonzero corridor key is retained. All water geometry is
retained. Excluded stamps are **not** proved irrelevant to global rejection or
admission: this is local contact input, not a complete deformation-job replay.

Stamp plane: `[AtE, AtN, PlateauM, SlopeE, SlopeN, SagInv]`.
Kind: 0 pad, 1 corridor, 2 basin, 3 clearance. Rings/seams: alternating E/N metres.
Profile, when present: its corridor key and
`[beginE,beginN,endE,endN,beginDE,beginDN,beginDU,endDE,endDN,endDU,
stationLength,beginBed,endBed,beginPavementHalfWidth,endPavementHalfWidth,
beginHalfWidth,endHalfWidth]`. Plane evaluation is not the final apron/profile mix.
Both winning stamps have `profile=null`; corridor 60942 uses the plane/apron path.

Native observations: stamp 46764 lowers the first point to the basin bed; stamp
60942 controls the neighbouring apron blend. Their final local heights differ
8.839662 m. Original DEM at the same geographic samples differs about 0.2995 m.
Numbers are observations, not desired replacement heights. Native and geodetic
heights differ by frame curvature. The final original image is reproduced exactly.

Road positions use East/Up/South, column-major affine part placements and the
declared anchor. The selected parts are complete: the initial local rectangle plus every
whole page containing a plan part assigned contact key 106753. No faces are clipped.
Manifest pages remain unfiltered; the explicit loaded-page map, not array ordinal,
binds a render part. The 125 related pages can include foreign plan parts; no
per-triangle group mask is implied. Plan maps are captured under the exact
manifest PlanKey. Repeated page loads agree in part ID and position/index counts.
Source-way point pairs are latitude/longitude degrees. Source ways preserve
their original indices/points and provider feature/tile/digest
references. They are selected by feature bounds. The full 559-lane contact-key membership
is captured separately; only 326 of these lanes have a local source-way row here. Neither diagnostic point is covered by a rendered road triangle in the
double membership check. The corridor effect is an apron, not a road surface hit.

Still missing: road navigation mesh and per-triangle source-way association,
all original/deformed DEM outside the supplied pages/tile, and globally refused
stamp state. Water/native provenance does not supply missing road provenance.
Never treat missing exterior samples or the selection rectangle as a physical bank.
No native solver, world, motion, p99 or memory acceptance follows from this input.

Capture: private instrumentation on e8d9002a7; source fields and untouched frame
math. Hashes in `input.json` bind exact words. See #826/6541778949 and #1908.
Credits: OpenStreetMap contributors / OpenFreeMap; elevation: Mapterhorn.
The actual source epoch and provider identities remain in the native water asset;
this capture does not claim original unfiltered OSM tags.
