# Vienna pedestrian references

Run from the repository root with Blender 5.3 on the development Mac.
Install the reference library with the Python shipped with Blender so binary wheels match.

```sh
/Applications/Blender.app/Contents/Resources/5.3/python/bin/python3.13 -m venv --system-site-packages build/blender-python
build/blender-python/bin/python -m pip install './lib[places,transport]' astral==3.2
blender -b --factory-startup --python-exit-code 1 -P test/experiments/vienna/render.py -- --place Adlerhof --repeat
```

The measured interval includes rendering and PNG output. Source acquisition, scene construction and
Blender startup are separate. Run with `--engine cycles --output /tmp/outshine-vienna-light` for
an independent lighting comparison. Inputs are genuine ZXY tiles pinned in sources.json.
The seed, camera, clear sky and UTC are in places.json. The final eye position follows a vertical
intersection with the rendered support mesh.

The initial integration covers the local DEM, mapped building shells, near recessed facade cells
and ground-level road surfaces. Transport topology, passages, material variation, foliage, local
water surfaces and the distant DEM skyline require further integration before the twelve-site baseline
can be accepted.
