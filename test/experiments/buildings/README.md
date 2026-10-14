# Buildings experiments

Run from the repository root with `.venv`. Install the reference library as well
as the experiment dependencies:

```sh
.venv/bin/python -m pip install -e ./lib
.venv/bin/python -m pip install -r test/experiments/buildings/requirements.txt
.venv/bin/python -m unittest discover -s lib/tests -v
.venv/bin/python test/experiments/buildings/verify_modelling_scope.py
.venv/bin/python test/experiments/buildings/verify_scope_library.py
```

The connected Shape/modelling core and CGA ↔ ENU conversion come from
`outshine_ref.buildings.modelling`. Static OBJ/glTF/GLB decoding additionally needs
`.venv/bin/python -m pip install -e './lib[assets]'`.
`verify_scope_library.py` compares all emitted streams on four saved control meshes
against the committed pre-migration implementation, then measures the same five
operations and emission. Saved inputs live under `build/experiments/buildings/`;
the comparison is offline and reports each input hash. It does not install or
import another checkout. CGA scope frames use east/up/south metres internally;
shared spatial products use east/north/up. The conversions preserve winding.

Building plans/recipes and comparisons still live here and are being
transferred separately. The installed modelling core does not provide the complete
CityEngine capability set or building generator. Issue [#51](https://github.com/Weichwolf/outshine/issues/51)
tracks that transfer; [#34](https://github.com/Weichwolf/outshine/issues/34) owns
modelling capabilities.

Experimental coplanar CSG support partitioning compares GEOS triangle/plane
regions with exact rational convex clipping and original-edge stations. The
`exact-rounded` alternative contracts only existing edges whose endpoints
convert to identical Float64 positions; disconnected contact fans stay separate.
None is a general Shape Boolean API. Full checks keep directed closure,
resolvable normals, volume, surface, whole-region source support, UV0–9,
material priority and source tags. Rational construction alone does not prove
valid Float64 output. [#88](https://github.com/Weichwolf/outshine/issues/88) owns
the remaining rounding and general modelling contracts.

```sh
.venv/bin/python -W error test/experiments/buildings/verify_boolean_exact.py
.venv/bin/python -W error test/experiments/buildings/verify_boolean_semantic_overlay.py --real --method exact
.venv/bin/python -W error test/experiments/buildings/verify_boolean_semantic_overlay.py --real --method exact-rounded
```

The latter two commands currently expect exit 1 and preserve rejected reports.
Rational coordinate numerators/denominators are capped at 4096 bits; candidate,
region and triangle counts are bounded. There is no hard process deadline or
general memory bound, and source coplanarity selection remains floating point.
The experiment adopts exact construction from Zhou et al., SIGGRAPH 2016,
not their complete winding-number mesh arrangement or output repair algorithm.

```sh
.venv/bin/python test/experiments/buildings/showcase.py --publish --serve
```

The showcase reads the checked current snapshot and serves it on localhost:8766.
New published states appear after manually reloading the page; no polling timer runs.
Outputs remain under `build/experiments/buildings/`. Its four pages are viewing
scales; native LOD boundaries and runtime budgets remain unproven.
