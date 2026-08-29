# Corpora

What an established corpus is, which one covers each capability TARGET demands, and which three
would move outshine furthest. The survey `board/1878` names.

**The rule.** A corpus is admissible when its truth was stated by someone who never saw outshine.
Five conditions, all of them:

| | |
|---|---|
| INVARIANT | a standard, a measurement, or an independent high-precision computation states the answer |
| PINNED | a commit or a DOI, and a `sha256` per file — a moving corpus is not a measurement |
| LICENSED | an SPDX the tree can carry. Non-commercial, no-derivatives and ODbL are refusals |
| ORACULAR | the expected answer ships beside the input, machine-readable |
| AFFORDABLE | the fast gate is seconds; anything else is named and run by name |

**The grade** every row carries says which kind of truth it holds, and it is the sharpest
distinction in this file:

| grade | the expectation is | worth |
|---|---|---|
| **SPEC** | a standards body's stated answer | highest — the answer outlives every implementation |
| **TRUTH** | a measurement of the world, or a computation to more digits than we hold | highest |
| **SNAPSHOT** | another implementation's own output, frozen | agreement, never correctness |
| **INPUT** | nothing ships beside the input | survival only: "it did not crash" |
| **NONE** | no corpus exists | say so and stop |

**The cost has two halves, and the second is the one that bites.** FETCH is what it takes to get
the bytes; almost everything good here is a pinned raw URL and a hash. REACH is what it takes for
a case to ask outshine the question, and `board:1879` fixes that price: `test/` may reach only
through `include/outshine/`, which today publishes nine headers and no geodesy, no polygon, no
libm. A corpus is cheap when the door can already ask its question; every other row pays for the
grammar it needs. Both halves are stated separately below, because a row that reads "low" on
fetch and "high" on reach is not a low-cost row.

## What runs today

Measured 2026-08-25 by `find test/render -name manifest.json`.

| corpus | cases | grade | what it proves |
|---|---|---|---|
| Khronos glTF-Sample-Assets | 151 | SPEC + TRUTH | glTF 2.0 and its ratified extensions read correctly, and the picture agrees with Cycles |
| Khronos glTF-Asset-Generator, `Animation_*` | 34 | SPEC | node animation, sampler types and skinning load and interpolate as declared |
| WPT CSS (`css-flexbox` 138, `css-sizing` 19, `css-overflow` 5) | 162 | SPEC | the UI layer lays boxes where CSS says they go |
| test262, expression and statement containers | 813 | SPEC | the expression language means what ECMAScript says |
| | **1160** | | |

`test/outshine/` held 38 grown cases and is gone (86b38d15) — its subjects were ours, so
only its oracle was invariant. The prepared corpus occupies **20 GB** in the system temp dir.

The shape every new row must match: a pinned commit, a per-file `sha256`, an SPDX licence with the
holder, and a criterion that says what is being judged — `test/khronos/glTF/Triangle/manifest.json`
is the worked example.

---

## 1 · Content surface — glTF, textures, images

`glTF 2.0` is the only content surface, and `board:1382` wants all of it. What runs proves what we
do with a CORRECT asset. Nothing proves what we do with a wrong one.

| corpus | maintainer | licence | size | fetch | grade | proves | fetch cost / reach cost |
|---|---|---|---|---|---|---|---|
| **glTF-Validator test corpus** | Khronos | Apache-2.0 | **263** `.report.json` pairs under `test/base`, **109** under `test/ext` (counted at `434283be`, 2026-08-25) | `raw.githubusercontent.com/KhronosGroup/glTF-Validator/<sha>/test/base/data/<group>/<file>` and its `.report.json` twin | **SPEC** | the golden report names the error CODE, the severity and the JSON POINTER for every malformed asset — a refusal corpus, not a render corpus | low / **medium**: our refusal must carry a code and a pointer comparable to the report's, which is the direction `std::expected` already points |
| glTF-Asset-Generator, all 26 positive + 2 negative groups | Khronos | MIT | 219 models | pinned commit, `Output/{Positive,Negative}/Manifest.json` | SPEC (loadability) / SNAPSHOT (pictures) | `loadable: true/false` is machine-checkable; the sample images are screenshots for a human eye, NOT a pixel oracle — do not confuse them with the Cycles rung | low / low (34 already bound) |
| PngSuite | Willem van Schaik | permissive, no fee | 125,022 B archive, dozens of cases incl. deliberately corrupt | `schaik.com/pngsuite/PngSuite-2017jul19.zip` | SPEC | PNG decoding across bit depths, interlace, palettes, and refusal on corruption | low / medium (no image door) |
| KTX-Software-CTS | Khronos | multi-licence | submodule + LFS | separate repo | SPEC | KTX2 container conformance | medium / high |
| Basis Universal `test_files`, ARM `astc-encoder/Test` | Binomial / ARM | Apache-2.0 | uncounted | pinned commit | SNAPSHOT | each encoder against its own past output | low / high |
| libjpeg-turbo test images | libjpeg-turbo | bundled | 16 files | pinned commit | SNAPSHOT | that decoder's fidelity, not the JPEG spec's | low / high |
| ITU/ISO JPEG conformance streams; BC1–BC7 conformance | ITU/ISO; historically the non-free DirectX CTS | paywalled / absent | — | — | **NONE reachable** | — | — |

**Verdict.** The validator corpus is the strongest single addition in this whole survey and it
rides the fetch machinery already in `prepare.py`.

## 2 · Geometry — meshing, booleans, simplification, predicates

| capability | corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|---|
| exact orientation predicates | **Shewchuk predicate fixtures** | provenance CMU (Nanevski et al. 2001), redistributed by `georust/robust` | Apache-2.0 on the redistribution; original carries no SPDX | 4 text files, `orient2d.txt` = 147,461 B, one case per line: coordinates + expected sign | `raw.githubusercontent.com/georust/robust/<sha>/fixtures/{orient2d,orient3d,incircle,insphere}.txt` | **TRUTH** | adversarial near-degenerate coordinates with the known correct sign — the floor every polygon, triangulation and cut-fill operation stands on | **lowest in the survey** / high: nothing public computes a predicate |
| CAD normals and curvature | ABC dataset | NYU/Skoltech | MIT tooling, data licence stated separately | ~1,000,000 models, 10,000-model chunk archives | chunk URL lists | TRUTH | per-vertex analytic normals and curvature, and B-rep patch segmentation from the STEP source | high — the smallest unit is a chunk / high |
| degenerate mesh robustness | Thingi10K | Thingi10K org (NYU) | **per-model Thingiverse licences, not uniform** | 10,000 STL | not raw-URL fetchable; NYU Box / HF mirrors, `pip install thingi10k` | INPUT | that a loader survives real-world garbage. No per-model ground truth exists | high / high, and the licence must be audited per model |
| triangulation | CDT `tests/expected` | artem-ogre | MPL-2.0 | dozens of golden text snapshots | pinned raw URL | SNAPSHOT | agreement with that library's own past output | low / high |
| triangulation | Triangle | Shewchuk | **"may not be sold or included in commercial products without a license"** | reference implementation, no corpus | — | reference, not corpus | — | **refused on licence** |
| tangent frames | MikkTSpace | mmikk | **no LICENSE file in the repo** | reference implementation, no corpus | — | it IS the oracle | vendor and run it as the definition of correct; every "MikkTSpace test corpus" in the wild is that implementation's frozen output | low / medium — `src/gltf/Tangents.cpp` is the claimant |
| mesh booleans | Cherchi arrangements; Zhou et al.; Cork | — | — | — | — | **NONE** | no ground-truth Boolean corpus exists anywhere. Every paper benchmarks Thingi10K inputs on crash-freedom, watertightness and timing — never against a known-correct output mesh | — |
| mesh simplification | — | — | — | — | — | **NONE** | the field has input datasets and a METRIC (METRO / Hausdorff), never a "simplify X, expect Z" corpus. Each paper defines its own comparison | — |
| general | CGAL `test/` | CGAL project | test and doc subtrees **CC0-1.0**; library LGPL/GPL | per-package | pinned commit | SPEC, but as C++ assertions | the oracle is the test PROGRAM, not the data — using it means building CGAL with GMP/MPFR/Boost | high / high |
| general | libigl-tests-data | libigl | **no LICENSE file** | 55 files | pinned raw URL | mostly INPUT | a fixture bag; a few `*_output` pairs are real, most are inputs | low / high, and no licence to cite |
| general | OpenMesh | RWTH Aachen | BSD-3-Clause | unconfirmed | **GitLab, not GitHub** | SNAPSHOT | hardcoded topology counts after operations | medium — wrong host / high |
| general | MeshLab / vcglib | CNR-ISTI | **GPL-3.0** | — | — | — | — | **refused on licence** |

**Verdict.** Only the predicate fixtures clear the bar cleanly. Booleans and simplification have no
oracle in existence — that is the finding, not a gap in the search.

## 3 · Polygons, footprints, buildings from OSM

The Blackshark class. There is no conformance corpus and there cannot be one: footprint + tags →
volume is underdetermined, roof shape and height are heuristics, and CLAUDE.md already says so —
OSM-derived infrastructure is PLAUSIBLE, never true to the real road. What can be tested is the
problem class beneath it.

| corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|
| **JTS TestBuilder XML** | Eclipse LocationTech | EPL-2.0 / EDL-1.0 dual | **129 XML files** — `general` 50, `robust` 55, `misc` 11, `validate` 9, `failure` 5 (counted at `dc7700d8`, 2026-08-25); one file holds many cases | `raw.githubusercontent.com/locationtech/jts/<sha>/modules/tests/src/test/resources/testxml/<group>/*.xml` | **SPEC** | the `<op>` tag body IS the expected WKT: overlay, buffer, relate, convex hull, validity, and a `robust/` group aimed at exactly the near-degenerate predicate class | low / **high, and there is no claimant**: `grep` over `src/ground` and `src/generators` finds no buffer, no overlay, no union, no point-in-polygon. A corpus for operations the engine does not perform proves nothing |
| GEOS `xmltester` | libgeos | MPL-1.1 dual | the same corpus, C++-native | pinned commit | SPEC | same; the better bind of the two for a C++ tree if this is ever taken | low / high |
| LoD2 building models (Bavaria LDBV, Geobasis NRW) | German state survey offices | **CC BY 4.0** / dl-de-by-2.0 | statewide CityGML, per tile | open-data portals | **TRUTH** | independently surveyed heights and roof shapes for a real place — an ORACLE like Cycles, not a conformance suite | medium / **high**: CityGML parsing, tile selection, Gauss-Krüger reprojection |
| OGC CITE / TEAM Engine | OGC | mostly Apache-2.0 | `ets-sfs11/12` | — | SPEC of a SQL surface | that a database's `ST_*` functions type-check. Needs a running server | **refused on shape** |
| pprepair / prepair benchmark | TU Delft | GPL-3.0 | the README's Corine dataset **is absent at HEAD** | — | — | — | **refused: the dataset does not exist where it is claimed** |
| real OSM extracts | OSM contributors | **ODbL** | — | — | INPUT | share-alike and attribution would attach to our derived test outputs — a licence class none of the three running corpora carry. Author synthetic `.osm` fixtures the way `osmium-tool` does | **refused on licence** |
| GDAL `autotest` | OSGeo | MIT | thousands of files, needs a built GDAL + pytest | pinned commit | SNAPSHOT | GDAL's own algorithms against their own past output | high / high, low payoff |

## 4 · Terrain — DEM, resampling, cut and fill

| corpus | grade | verdict |
|---|---|---|
| DEM resampling / hillshade conformance | **NONE** | GDAL's `test_gdaldem_lib.py` checks GDAL against itself. USGS/SRTM "validation" measures elevation accuracy against survey marks — a science question, not an algorithm conformance file. No independent reference hillshade with known-correct pixels was found |
| cut and fill of terrain to meet a road | **NONE** | nobody publishes a "correct" earthwork. What can be asserted is geometric: the surfaces close, the join is continuous, volume is conserved. That is a closed-form check we derive, not a corpus we fetch |

## 5 · Curves, splines, alignment

`board:1795` measured the defect that `Alignment` exists to repair. Nothing external could have
found it, and nothing external can guard it.

| corpus | maintainer | licence | grade | proves | verdict |
|---|---|---|---|---|---|
| Bertolazzi & Frego `Clothoids` | UniTN | BSD-style | reference implementation | 18 `.cc` files, all demo and plotting drivers — **no committed reference dataset**. The paper's tables would have to be transcribed by hand | not a corpus |
| ASAM OpenDRIVE | ASAM e.V. | spec free after registration, unrestricted-use grant; `qc-opendrive` validator MPL-2.0 | SPEC of a SCHEMA | that an `.xodr` is well-formed and obeys the rule set. It does NOT carry an independently computed radius or curvature to check a fit against | schema conformance only |
| esmini `resources/xosc`, `.xodr` | esmini | MPL-2.0 | INPUT | that a player parses these files and drives through them. A demo scene, not a test | low value |
| SVG path tests in WPT | W3C | BSD-3-Clause | SPEC, but as reftests | pixel comparison, no curve-parameter ground truth | wrong instrument |
| arc-length reparametrisation, G2 continuity | — | — | **NONE** | academic papers only, no citable dataset | own analytic fixtures or nothing |

**Verdict.** No third party owns this ground truth. `ACurveIsFittedAtTheRadiusItHas` — the case
`board:1878` calls the sharpest invariant the tree ever had — has no external home and never will.
It should be rewritten against the door, not reclaimed from a corpus.

## 6 · Numerics — floating point, transcendentals, linear algebra

| corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|
| glibc `auto-libm-test-out-*` | GNU | LGPL | e.g. `…-sin` is 322,436 B; exact hex-float expected values per rounding mode and format, MPFR-generated | `raw.githubusercontent.com/bminor/glibc/<sha>/math/auto-libm-test-out-<func>` | **TRUTH** | correctly-rounded transcendental results | low / **no claimant**: outshine ships no libm. This bites only at the MSL-vs-C++ shader twin, and that suite was deleted with `test/outshine` |
| CORE-MATH `.wc` | INRIA | MIT | per-function worst-case lists | `gitlab.inria.fr/core-math/core-math` | TRUTH | hard-to-round inputs for correctly-rounded functions | low / same absent claimant |
| Berkeley TestFloat 3 | J. Hauser | BSD-3-Clause | no static data — a generator and comparator | pinned commit | TRUTH, live | IEEE-754 arithmetic against SoftFloat | high — build / high |
| FPBench / FPCore | FPBench | MIT | symbolic expressions | pinned commit | not an I/O corpus | inputs for accuracy-research tools (Herbie, Daisy) | wrong artefact class |
| LAPACK `TESTING/` | LAPACK | BSD-3-style | matrices generated at runtime, never persisted | — | **residual only** | `‖AE−EW‖/(‖A‖‖E‖·ulp)` — there is no precomputed eigenvalue anywhere in it | **not a ground truth** |
| Matrix Market, SuiteSparse | NIST / TAMU | public domain / CC BY 4.0 | large | per-matrix | INPUT | inputs, no answers | fuzzing at best |
| Eigen `test/` | Eigen | MPL-2.0 | in-tree | — | residual, harness-coupled | Eigen against itself | not portable |
| quaternion / rotation conformance | — | — | — | — | **NONE** | searched graphics, robotics and aerospace bodies; nothing comparable to test262 exists. Quaternion identities are provable in closed form and belong in an analytic fixture | — |

**Verdict.** Matrix decomposition has no external oracle — the honest path is a matrix built with a
known decomposition (diagonal, or Householder-constructed), which is derivation, not fetching.

## 7 · Geodesy and projection

`src/core/Geodesy.h` and `src/core/Mercator.h` carry `GeoToEcef`, `EnuOffsetM`, `PlanarDistM`,
`BearingDeg` and `kMercatorLatMaxDeg = 85.05112877980659`. Every metre of Munich–Hamburg rests on
them, and nothing in the tree proves one of them.

| corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|
| **GeodTest.dat** | C. Karney, GeographicLib | **CC0-1.0** | **86,558,916 B**, 500,000 geodesics, 10 columns (measured 2026-08-25) | `zenodo.org/records/32156/files/GeodTest.dat` — a DOI, immutable by construction | **TRUTH** | `lat1, az1, lat2, az2, s12, m12, a12, S12` computed in Maxima at 50 digits — the direct and inverse geodesic to far more precision than a `double` holds | **lowest** / medium: the door publishes no geodesy |
| **GeodTest-short.dat** | same | CC0-1.0 | **803,843 B** gzipped, 10,000 rows, an exact 1/50 subsample | `downloads.sourceforge.net/project/geographiclib/testdata/GeodTest-short.dat.gz` | **TRUTH** | the same, at fast-gate size | lowest / medium |
| **TMcoords.dat** | same | CC0-1.0 | **34,422,206 B**, 287,000 rows | `zenodo.org/records/32470/files/TMcoords.dat` | **TRUTH** | transverse Mercator: lat/lon → easting, northing, convergence, scale, from Lee's elliptic formulas at 80 digits | low / medium |
| PROJ `.gie` tests | OSGeo | MIT | 26 top-level files, ~1300 selftests | `raw.githubusercontent.com/OSGeo/PROJ/<sha>/test/gie/*.gie` | SPEC for standard projections, SNAPSHOT for grid transforms | plain `accept` / `expect` pairs. The Mercator, UTM and conic cases rest on published Snyder/EPSG formulas and are reusable; the grid and network-transform cases prove only PROJ's own consistency and must be filtered out | low / medium, plus curation |
| EPSG registry | IOGP | terms of use | — | — | SPEC of parameters | constants, not cases | reference |

**Verdict.** GeodTest is the cleanest hit in the survey: CC0, DOI-pinned, one file, one hash,
50-digit truth, and a claimant that already exists in `src/core/`.

## 8 · Rigging, skinning, animation

| corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|
| glTF-Sample-Assets skinning models | Khronos | CC BY 4.0 | `RiggedFigure`, `RiggedSimple`, `SimpleSkin`, `MorphPrimitivesTest`, `MorphStressTest`, `InterpolationTest`, `AnimatedMorphCube`, `BrainStem`, `Fox` — all present at HEAD | already fetched | SPEC + TRUTH via Cycles | joint hierarchies, morph targets and interpolation, judged as pixels | low / low — the machinery is running |
| glTF-Asset-Generator `Animation_Skin`, `Animation_SkinType` | Khronos | MIT | 12 + 4 models | already bound (34 cases) | SPEC (loadability) | the manifest carries `loadable` and a reference PNG filename — **no numeric joint or vertex ground truth** | low / low |
| **OpenUSD `usdSkel/testenv/testUsdSkelBakeSkinning`** | Pixar | **Tomorrow Open Source Technology License 1.0** — non-standard, read it before binding | `lbs.usda` (21,851 B), `dqs.usda`, `blendshapes*.usda`, each with a paired `baseline/` directory | `raw.githubusercontent.com/PixarAnimationStudios/OpenUSD/<sha>/pxr/usd/usdSkel/testenv/testUsdSkelBakeSkinning/…` | **SPEC** — input and expected output as numbers | linear-blend and dual-quaternion skinning as engine-agnostic mathematics, the only numeric skinning oracle found | low / high: USD is not a surface outshine speaks, so the fixtures would need translating to glTF |
| inverse kinematics | IKFast, MoveIt, KDL, Robotics Library | — | — | — | **NONE** | all of them ship benchmark harnesses — solver success rate and timing over random configurations — never a ground-truth (pose → joint angle) fixture. Closed-form two- and three-bone IK is derivable; that is derivation | — |
| FACS / ARKit blendshapes | — | — | — | — | **NONE** | naming and coefficient conventions, no conformance corpus | — |
| AMASS, SMPL, CMU MoCap, Human3.6M | MPI-IS et al. | **non-commercial, registration required** | large | portals | INPUT | captured human motion for perception and ML. Says nothing about a skinning matrix | **refused on licence and on purpose** |

## 9 · Sky, atmosphere, ephemeris

The render plan marks `mediumTransmittance`, `mediumMultiScatter` and `mediumRadiance` GREEN in
both maps, and Cycles cannot be their oracle — it does not model an atmosphere. Meanwhile
`src/render/stages/ParticipatingMedium.h:18` carries
`RayleighScatteringPerKm = {0.005802f, 0.013558f, 0.033100f}` and `grep` finds that number in no
board item. Under *every number carries its origin*, those constants are undeclared.

| corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|
| **ebruneton/clear-sky-models `input/`** | E. Bruneton | BSD | `kider_full_day_irradiance_raw_2013_5_27.txt` **50,522 B**, `astm-g173.txt` **87,246 B**, `130527_130527_Egbert.pfn` **22,288 B** (measured 2026-08-25) | `raw.githubusercontent.com/ebruneton/clear-sky-models/<sha>/input/…` | **TRUTH** | Kider et al.'s measured full-day sky irradiance for a real clear sky, and the ASTM G173 reference solar spectrum — the ground truth eight published sky models were graded against, including the one this tree implements | **low** / medium: a scenario declaring sun angle and publishing sky radiance, plus an honest spectral-to-RGB mapping |
| `precomputed_atmospheric_scattering` | E. Bruneton | BSD-3-Clause | reference implementation | pinned commit | reference | where the constants above come from — citing it would already discharge half the debt | low / low |
| **IAU SOFA `t_sofa_c.c`** | IAU SOFA Board | SOFA Software Licence (SPDX `SOFA`) — free for any use incl. commercial, but a modified copy must be renamed | **227,170 B**, every routine called with expected values produced independently in quadruple precision | official site is a JavaScript-rendered Squarespace with no stable tarball URL; the practical pin is the **unofficial** mirror `raw.githubusercontent.com/Starlink/sofa/<sha>/src/t_sofa_c.c`, whose `vendor` branch is the board's release verbatim | **TRUTH** | precession, nutation, sidereal time, equinoxes, refraction — everything `src/core/Ephemeris.h` computes by hand, with `kEphemerisMinYear = 1901` bounding it | low, with a caveat on the mirror / medium |
| **JPL DE440 `testpo.440`** | NASA JPL SSD | US Government work, freely redistributable | **858,238 B**, one test point per line, 20 significant digits in AU, spanning JD 2287184.5 to 2688976.5 | `ssd.jpl.nasa.gov/ftp/eph/planets/ascii/de440/testpo.440` (HTTP 200, verified 2026-08-25) | **TRUTH** | planetary and lunar positions to 1e-14 AU — the sun and moon the TARGET render plan draws | low for the test points, **high for the data**: evaluating them needs `ascp01950.440`, 29.2 MB of Chebyshev coefficients per 50-year block |

## 10 · Rendering, colour, image comparison

| corpus | maintainer | licence | grade | proves | verdict |
|---|---|---|---|---|---|
| VK-GL-CTS | Khronos | Apache-2.0 | SPEC | it tests a **driver**, through Vulkan or GL. We run Metal through SDL3_GPU and have neither | **refused on layer and API** |
| ACES (`aces-aswf/aces-core`) | AMPAS / ASWF | permissive Academy licence | TRUTH, no tolerance stated | 3 source images with per-transform golden outputs, and no stated bound — we would invent the threshold, i.e. rebuild what Cycles already gives us, worse | only if a colour pipeline is adopted |
| OpenColorIO `tests/` | ASWF | BSD-3 | SNAPSHOT | OCIO's own maths. We do not depend on OCIO | not applicable |
| ICC conformance | color.org | unclear | — | `color.org/testing.xalter` 404s; only human-eyeball workflow demos found | **NONE reachable** |
| `colour-science` + CIE datasets | Colour Developers | BSD-3 | constants | a self-tested reference implementation, useful as INPUT constants for a test we write | data, not a test |
| MERL BRDF database | MERL | **research-only, fee otherwise** | TRUTH | raw reflectance samples, no scene and no image — it would validate a BRDF routine, never the rasteriser | **refused on licence** |
| RGL/EPFL Materials | EPFL | licence page 404s | TRUTH | same class | unverified |
| pbrt-v4-scenes, Bitterli Rendering Resources | Pharr et al. / B. Bitterli | mixed per scene, NC scenes present | reference images | reference renders ship, but the `(seed, sample count, integrator)` tuple that makes them reproducible was not confirmed. Without it a reference image is a picture, not an oracle | per-scene audit required |
| Moana Island, NVIDIA ORCA | Disney / NVIDIA | restrictive / unstated | INPUT | 45–131 GB on S3, no pinned-commit model, no reference renders | **refused on size and shape** |
| NVIDIA FLIP | NVIDIA | BSD-3 | an INSTRUMENT | purpose-built for rendered-image difference, LDR and HDR. Worth a bake-off against our 0.005 px floor — but it is a metric, not a corpus | instrument evaluation |
| SSIMULACRA2, Butteraugli | Cloudinary / Google | BSD-3 | instrument | second-tier alternatives | instrument |
| PerceptualDiff | — | **GPL-2.0** | instrument | — | **refused on licence** |
| TID2013, CSIQ, LIVE IQA | various | TID2013 **CC BY-NC-ND** | TRUTH about human perception | they validate a METRIC against human opinion scores — orthogonal to renderer correctness | wrong purpose |
| tonemapping, auto-exposure, shadow quality | — | — | **NONE** | these are picture choices, not standards. There is no correct output to conform to; Cycles is the reference by construction | own oracle only |

## 11 · Physics and vehicle dynamics

| corpus | maintainer | licence | grade | proves | verdict |
|---|---|---|---|---|---|
| **Test Set for IVP Solvers** (Bari) | F. Mazzia, Univ. Bari | **no licence found — no SPDX, no LICENSE file** | **TRUTH** | Fortran drivers plus reference solutions to stated digit counts for HIRES, Pollution, Ring Modulator, Vdpol, Rober, Orego, Pleiades, Arenstorf and others. Genuine integrator ground truth: the physics is exact classical mechanics, not our design | the only real corpus in this cluster. **Host note:** the URL cited in the literature, `archimede.dm.uniba.it`, now fails TLS — the certificate covers `archimede.uniba.it` only. No git repo, so a pin means vendoring a snapshot and hashing it, and the licence must be asked for |
| DETEST (Hull et al. 1972) | — | — | — | a journal paper. The Bari set is its maintained successor | not fetchable |
| Project Chrono validation | UW–Madison | BSD-3 (code) | — | one PDF comparing Chrono::Vehicle to **ADAMS/Car**, another model. No measured data, no HMMWV or Polaris field dataset. Our engine tracking Chrono tracking Adams triangulates opinions | **not a ground truth** |
| Pacejka: MFeval, TNO MF-Tyre, open `.tir` sets | various | proprietary or unclear | SNAPSHOT at best | MFeval exists *because* MF implementations disagree. A `.tir` plus the MF equations is a self-consistency check against another evaluator, never physical truth | cross-implementation only |
| FSAE Tire Test Consortium | Milliken Research | **NDA, university-domain membership** | TRUTH | real dyno data. `TTC_FSAE` on GitHub is code only, and its README says the sample data was deliberately altered so as not to leak | **refused on licence** |
| ISO 3888-1/-2, 4138, 7401, 13674 | ISO | **purchase only** | SPEC of a manoeuvre | the manoeuvre geometry is reproducible from secondary sources; no open measured-response dataset for a named vehicle exists | **dead end** |
| ASAM OpenSCENARIO + `qc-framework` | ASAM | MPL-2.0 (validator) | SPEC of a schema | XML and rule-layer validity | schema only |
| rigid-body contact solvers | Erez/Tassa/Todorov; MuJoCo Menagerie | — | — | the ICRA paper is a **performance and behaviour comparison** — its finding is that engines disagree. Menagerie is a model library | **NONE** |
| GJK/EPA, distance queries | libccd, FCL | BSD | SNAPSHOT | real cases, but buried in two libraries' own unit *code*, not data — binding them means vendoring C++, a different shape from every other row here | marginal |
| friction cones, contact numerics | — | — | **NONE** | validated in the literature by convergence proofs and hand-worked examples. A block on an incline at the critical friction angle is a closed form we derive | own fixtures only |
| Waymo Open Motion, nuScenes | Waymo / Motional | research terms | TRUTH about trajectories | pose and velocity from sensor fusion, with no control inputs and no measured tyre forces. Nothing behind the trajectory is known | wrong quantity |
| CARLA logs | CARLA | MIT | SNAPSHOT | throttle, brake and steer alongside trajectory — but CARLA is PhysX, so this checks agreement with another engine's opinion | same category error as Chrono |

**Verdict.** Tyre models, contacts and vehicle response have no invariant corpus. the drive path
plus the architect's screenshot is the only instrument, and saying otherwise would be a wish.

## 12 · Spatial audio

| corpus | maintainer | licence | grade | proves | verdict |
|---|---|---|---|---|---|
| libsamplerate `tests/snr_bw_test.c` | libsndfile | BSD-2-Clause | SPEC, self-contained | **10,686 B**. Hardcoded minimum SNR and bandwidth per converter against self-generated sweeps. A genuine pass/fail that needs no external corpus at all | the cheapest real check in the survey |
| Opus RFC 6716 test vectors | IETF / Xiph | effectively public | SPEC | bit-exact decode via `opus_compare` | real conformance, if we ever decode Opus |
| FLAC test files | ietf-wg-cellar | **CC0** | INPUT | feature and crash coverage; no bundled PCM reference confirmed | weak |
| SOFA HRTF databases — CIPIC, ARI, LISTEN, SADIE II, SONICOM | per institution | **varies per dataset**: CIPIC public domain, ARI CC BY-SA 3.0, SONICOM CC BY 4.0, SADIE II unstated | INPUT | filter data. No renderer-versus-database validator exists anywhere | a dataset, not a test |
| libmysofa `tests/` | C. Hoene | BSD-3-Clause | SNAPSHOT | golden-JSON diff — parser stability, not DSP correctness | weak |
| EBU Tech 3341/3342 loudness set | EBU | EBU terms of use, redistribution unclear | claimed SPEC | 70 files, ~87 MB. The landing page describes the bundled `readme.txt` as **a change log**, not a table of expected LUFS, and the archive is Cloudflare-gated from an unattended fetch. **Whether per-file expected values ship is unresolved** | **not verified — do not bind on this evidence** |
| room impulse responses — OpenAIR, AIR, MIT/McDermott | various | mixed, McDermott CC BY 4.0 | INPUT | impulse responses. ISO 3382 is a measurement procedure, not a corpus | inputs only |
| distance attenuation, bus mixing | — | — | **NONE** | searched AES and ITU; no conformance suite exists for either | own fixtures only |
| ambisonics — IEM suite, ambix | TU Graz / kronihias | GPL-family | — | no reference-decoder vectors; `ambix`'s `run_tests.py` is a build check | **NONE** |

**Verdict.** `src/audio/BusGraph` has no external oracle. The mix, the falloff and the ear are ours
to specify.

## 13 · Procedural generation — vegetation, determinism

There is no conformance corpus for a tree, and there cannot be one: there is no correct tree.
SpeedTree and L-systems are aesthetic generators. What generation actually rests on is
reproducibility, and that has real corpora.

| corpus | maintainer | licence | grade | proves | verdict |
|---|---|---|---|---|---|
| xoshiro / xoroshiro reference vectors | Blackman & Vigna; `Quuxplusone/Xoshiro256ss` | CC0 (reference C), MIT (wrapper) | **SPEC** | a checked-in test vector, cross-checked across three language implementations, from a `splitmix64(100)` seed. The strongest of the PRNG candidates because the vector already exists in a repo | fast gate, if outshine picks this generator |
| PCG | M. O'Neill | Apache-2.0 | reference | the reference header self-checks; no separate vector file. Mint our own from a pinned source | low |
| MT19937 canonical 1000 outputs | Matsumoto & Nishimura | BSD-style | SPEC | the vector is universally cited but **was not found at a stable pinned URL**. Mint it from the pinned reference C | not verified as a fetchable file |
| Perlin / Simplex noise vectors | — | — | **NONE** | no repo ships a frozen expectation table. Perlin's reference Java exists; mint from it and pin the source | mint, do not pretend |
| SMHasher | rurban, orig. Appleby | MIT | statistical | avalanche, collision and distribution for a chosen hash — relevant to the content store, where hash IS filename. Minutes to an hour | sporadic audit |
| PractRand, TestU01 BigCrush | Doty-Humphrey; L'Ecuyer | permissive / restricted | statistical | BigCrush runs for **hours**. Never the gate | sporadic audit |

`board:1780` — the forest's randomness stable under a new stream and a new species — is a
determinism claim, and a frozen PRNG vector is exactly what anchors it.

## 14 · The declaration surface — XML, CSS, expressions, text

The scenario is XML, the UI is CSS-like, the expression language is ECMAScript-shaped. Two of the
three are already covered.

| corpus | maintainer | licence | size | fetch | grade | proves | fetch / reach |
|---|---|---|---|---|---|---|---|
| **W3C XML Conformance Test Suite** | W3C, ex-OASIS | W3C document and software licence | **1,574,648 B** archive, over 2000 test files | `w3.org/XML/Test/xmlts20130923.zip` — not a git repo, so the pin is the dated archive plus its `sha256` | **SPEC** | well-formedness, encodings, entities, namespaces — for the reader the scenario door depends on | low / **low**: `Engine::Read(path)` is already the door, and a malformed scenario must refuse |
| Unicode UAX #29 `GraphemeBreakTest.txt` | Unicode Consortium | Unicode Data Licence | **188,211 B** at 15.1.0 | `unicode.org/Public/<version>/ucd/auxiliary/GraphemeBreakTest.txt` | **SPEC** | grapheme cluster boundaries — cursor movement and selection in the UI layer | low / medium |
| `WordBreakTest.txt`, `LineBreakTest.txt` | same | same | 1,974 and 19,368 lines | `.../ucd/auxiliary/` | **SPEC** | UAX #29 word boundaries and UAX #14 line-break opportunities — the latter is what a CSS-like text layer needs to wrap correctly | low / medium |
| `NormalizationTest.txt` | same | same | **2,625,136 B**, 20,095 lines | `unicode.org/Public/<version>/ucd/NormalizationTest.txt` | **SPEC** | NFC/NFD/NFKC/NFKD | low / medium |
| `BidiTest.txt`, `BidiCharacterTest.txt` | same | same | 497,590 and 96,465 lines | `unicode.org/Public/<version>/ucd/` — **not** under `auxiliary/` | **SPEC** | UAX #9, exhaustively | low / medium; measure before putting 500k assertions in a fast gate |
| JSONTestSuite | N. Seriot | MIT | **318** files, `y_`/`n_`/`i_` prefixes | pinned commit | **SPEC** | RFC 8259 edge cases — the content store and tool interchange | low / medium |
| Markus Kuhn `UTF-8-test.txt` | M. Kuhn | CC BY 4.0 | one file | `cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt` | INPUT | the author states it prescribes **no particular outcome** — a stimulus set. The accept/reject policy would be ours | weaker than it looks |
| HarfBuzz shaping suite | HarfBuzz | MIT-style | ~2252 cases | pinned commit | SPEC | ligatures and OpenType features — only if the UI does real shaping. Needs GLib and meson | high / high |

---

## What nothing tests, and why

Stated plainly, because a survey that hides its holes is worse than none.

| capability | why no corpus exists |
|---|---|
| vegetation, and every SpeedTree-class generator | there is no correct tree. The output is judged aesthetically; no ground truth is definable |
| buildings from OSM footprints and tags | underdetermined by construction — height and roof come from heuristics over incomplete tags. CLAUDE.md already concedes it: PLAUSIBLE, never true to the real building |
| the reconstructed third dimension — bridges, ramps, tunnels | the four plausibilities (geometric, physical, static, architectural) are judgements, and three of the four are engineering opinion. No corpus can hold them |
| terrain cut and fill | nobody publishes a correct earthwork. What is checkable — closure, continuity, conserved volume — is closed-form and ours |
| mesh booleans, mesh simplification | the fields self-test on crash-freedom, watertightness and a distance metric. No published "correct output mesh" corpus exists anywhere |
| tonemapping, auto-exposure, shadow method | picture choices, not standards. There is nothing to conform to |
| tyre models, contact solvers, friction cones | validated by convergence proofs and hand-worked examples in papers; the only open datasets are simulator-versus-simulator or perception-grade |
| inverse kinematics | benchmarks measure solver success rate and speed, never a (pose → joint angle) expectation |
| reverb, distance attenuation, bus mixing | impulse-response databases are inputs. No AES or ITU conformance suite exists for any of the three |
| clothoid fitting, arc-length reparametrisation, G2 continuity | papers only, no citable dataset. `ACurveIsFittedAtTheRadiusItHas` has no external home |
| quaternion and rotation semantics | provable in closed form; nobody ships a corpus for an identity |
| matrix decomposition | LAPACK checks a residual, not an answer. A matrix with a known decomposition is built, not fetched |

Two further exclusions, on grounds other than absence:

- **Licence.** MERL, AMASS/SMPL, FSAE TTC, TID2013, PerceptualDiff, MeshLab, Triangle and any real
  ODbL OSM extract are refused. ODbL deserves the sharpest note: it would attach share-alike and
  attribution to our derived test OUTPUTS, a burden none of the three running corpora carry.
- **Shape.** VK-GL-CTS tests a driver behind an API we do not use. OGC TEAM Engine needs a running
  server. CGAL's oracle is a C++ program, not data. Each would be vendored infrastructure, not a
  fetched corpus.

## The three that would move outshine furthest

Ranked on what TARGET demands, not on what is easy.

### 1 · Khronos glTF-Validator — 372 golden report pairs, Apache-2.0

**The argument.** *glTF 2.0 is the only content surface*, and `board:1382` wants all of it plus
every ratified KHR extension. The 151 render cases prove what we do with a CORRECT asset. Nothing
in the tree proves what we do with a WRONG one — and refusal is a first-class idea here, with
`std::expected` reserved for a fault that carries its reason. A golden report is precisely a
refusal expectation: a code, a severity and a JSON pointer per defect.

**Why it is cheapest.** It rides `prepare.py`'s existing Khronos fetch: pinned commit, per-file
`sha256`, Apache-2.0, `raw.githubusercontent.com`. No new job kind.

**What it costs.** Our refusals must carry a code and a pointer comparable to the report's. That is
the work, and it pushes the door exactly where `board:1879` says it must go — the case asks the
engine to read an asset and reports what came back, and nothing under `test/` reaches into `src/`.

### 2 · GeographicLib GeodTest and TMcoords — CC0, DOI-pinned, 50-digit truth

**The argument.** *One world space*, and the precision boundary is the camera. `src/core/Geodesy.h`
converts geodetic to ECEF, offsets in ENU, and answers distance and bearing;
`src/core/Mercator.h:6` pins `kMercatorLatMaxDeg`. Munich–Hamburg, every tile key, every building
placement and the whole cut-and-fill rest on those functions, and not one of them is proven.
`board:1878` lists the deleted invariants and has nothing here — because there was nothing here.

**Why it is the cleanest.** CC0. A Zenodo DOI is immutable, so the pin costs nothing to defend.
`GeodTest-short.dat` is 10,000 rows for the fast gate and the full 500,000 for the audit;
`TMcoords.dat` covers the Mercator half. Reference precision is 50 digits — far past `double`, so
the corpus can never be the thing that is wrong.

**What it costs.** The door publishes no geodesy. Under `board:1879` the answer is not to widen
`include/outshine/` with a coordinate verb but to let a scenario declare a position and publish
where the engine put it — which the driver's telemetry wants anyway.

### 3 · ebruneton/clear-sky-models `input/` — Kider's measured sky, 160 KB, BSD

**The argument.** The render plan marks three medium stages GREEN in CURRENT *and* TARGET, and they
have no oracle: Cycles does not model an atmosphere, so the one instrument the tree trusts is
silent on its own sky. Worse, `src/render/stages/ParticipatingMedium.h:18` carries
`RayleighScatteringPerKm = {0.005802f, 0.013558f, 0.033100f}` and `grep` over `board/` finds that
number nowhere. Under *every number carries its origin*, those are undeclared constants on the
frame path, and this corpus is what can adjudicate them — it is the measurement eight published
sky models, including the one this tree implements, were graded against.

**Why it is affordable.** Three text files, 160,056 B in total, BSD, one repository, one pin.

**What it costs.** A scenario that declares a sun angle and publishes sky radiance, and an honest
spectral-to-RGB mapping stated as a derivation, not assumed. That mapping is the real work and it
must be written down, because a comparison whose unit conversion is a guess proves nothing.

**Runner-up, and why it lost.** JTS TestBuilder — 129 files of SPEC-grade expected WKT, dual
EPL/EDL, aimed at exactly the near-degenerate predicate class the four plausibilities need. It
loses because it has **no claimant**: `grep` over `src/ground` and `src/generators` finds no
buffer, no overlay, no union, no point-in-polygon. The day the building and water fields perform a
polygon operation, this becomes the first corpus to bind. Beside it, the Shewchuk predicate
fixtures are the cheapest bytes in the survey and wait on the same day.

## Cost, honestly

| tier | what belongs there |
|---|---|
| **fast gate** — seconds, every edit | glTF-Validator reports · GeodTest-short (10,000 rows) · the clear-sky input files · UCD grapheme, word, line-break and normalization tests · JSONTestSuite · a frozen PRNG vector |
| **sporadic, run by name** | the running render corpora · full GeodTest (500,000) and TMcoords (287,000) · W3C xmlconf (2000+ files) · BidiTest (497,590 assertions — measure before deciding) · PROJ's curated projection subset · KTX and Basis · SMHasher |
| **hours, never a gate** | TestU01 BigCrush · PractRand at depth · a LoD2 CityGML oracle comparison |
| **not a corpus, an instrument** | NVIDIA FLIP, SSIMULACRA2 — worth a bake-off against the 0.005 px floor, but they are metrics |

A suite of 50,000 cases that runs for an hour is worthless in a regression gate and valuable once a
week. The split above is the whole difference between a corpus that gets run and one that gets
skipped.
