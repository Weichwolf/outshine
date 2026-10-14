# Dense young conifer crown reference

Owner: [#131](https://github.com/Weichwolf/outshine/issues/131).
The sparse two-order control remains a valid independent comparison. This probe
adds leafy third-order shoots to four complete young plants at the same roots,
camera, lights, grass and 2560×1440. Heights and physical needle prototypes stay
unchanged. Branch counts/lengths are an own plausible interpretation, not measured
allometry or approved botanical growth. Mature trees, cones and seasons remain open.

Weber/Penn, SIGGRAPH 1995, page 3's stem-child diagram was opened alongside JRC2016
Picea page 2's branching needle shoots. The four generator/parent/template routines
in the local SpeedTree 10.2 binary were successfully decompiled again in Ghidra,
read-only. Their node/parent processing informs separation of plan and emission;
it neither calibrates biology nor proves equivalent algorithms. Private diagnostic
code remains in Downloads and is not bundled here.

```sh
.venv/bin/python test/experiments/vegetation/conifer_crown_products.py
.venv/bin/python test/experiments/vegetation/conifer_crown_checks.py
.venv/bin/python -c 'import sys; from pathlib import Path; sys.path.insert(0,"test/experiments/vegetation"); from europe_conifer_tile import make_tile; make_tile(Path("build/experiments/vegetation/conifer-crowns").resolve())'
.venv/bin/python test/experiments/vegetation/conifer_preview.py -- \
  --scenario build/experiments/vegetation/conifer-crowns/tile/tile.json \
  --output build/experiments/vegetation/conifer-crowns/tile/render.png
```

`NeedledPlant.foliage(path, max_needles, retain=False)` creates owned readonly
detail without retaining newly generated groups internally. Existing cached groups
may be reused. Default retention, recipes, IDs and per-call budgets stay unchanged.
Pose packets hold at most 16,384 rows; streaming keeps fixed buffers plus axes and
one group, rather than accumulating every detailed organ field. Three plants exceed
100,000 needles; their prohibited global detail tables still return `None`. Counts
are not truncated. The index references complete pose/wood packets and the full
physical recipe. A cached near twig is an exact subset of those needle IDs/poses.

The new whole-plan reference contains 709,809 needles and 43,680,520 triangles
including 11,169,720 triangles in tiny short shoots. It is a deliberately expensive
physical image oracle for the crown/LOD work, not a bounded far-forest product or
runtime recommendation. Near detail and coarse products must subsequently be chosen
before expensive geometry; the current numerical checks do not approve the image,
shading or native frame cost. The showcase retains its four verified tiles until a
new current complete tile is optically accepted.
