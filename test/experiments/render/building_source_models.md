# Source and group image losses

Owner #1167; follows the mixed/negative local-coverage comparison #1326.
Before changing geometry or normal representations, compare the existing
analytic source roof/wall model to the captured native reference at the same
two dense-cell root-boundary views. Independently transfer the existing group
materials onto the unchanged native geometry and normals. Original source IDs,
fallback geometry, UV/colour fields and full material mixtures remain available.

The analytic reference now honors the frame's explicit selected-source set
before patch generation. Requested unsupported sources still fail; no roof,
triangulation or Float32 check is relaxed. The full dense-cell analytic reference
fails its existing hip-facet coverage check. This diagnostic therefore uses only
the sources already qualified and represented by the existing root parent;
the root's entire original remainder stays native. Analytic/native source sets
must be disjoint and together cover the whole original population. This is a
labelled reference mixture, not native mesher/postweld/normal equivalence.

Two material controls preserve every original vertex/face and its geometric
shading normal. One uses the parent model's normal-based wall/roof choice;
the other uses the captured native wall/roof stream. Both apply the same group
albedo/roughness measures only to represented sources. They retain the exact
original material path for the remainder. T must be word-equal to the native
reference. Normal/semantic role C/T equality is reported, not assumed.

The final control is the unchanged parent geometry, uniform fractional coverage
and full original remainder. Results are separate differences against native;
their scalar errors are not additive causal shares. A smaller analytic-source
error than parent error does not isolate geometry, depth, normals or masking
individually. Material-only loss can coexist with those other losses. No free
normal/contour/pose/resolution sweep or new native representation follows.

1280×720/SS16 and the existing two root-boundary poses are unchanged. ROI includes
all compared geometry. Output includes actual full-frame PNGs, local contact
sheet, linear C/T arrays, source provenance, known array bytes, GGX/sample work
and whole diagnostic CPU/wall/peak cost. Source meshing here is an offline image
reference cost; it is not a source-first parent producer or runtime recommendation.
Finite samples provide no native/world/shadow/device/p99 qualification. Read
the linked PR for exact measurements and conclusions; images remain local Temp.

Use `.venv` Python, one BLAS/OMP thread and
`PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings`:

```sh
python -B -W error test/experiments/render/compare_building_source_models.py \
  --input /tmp/outshine-transition-products \
  --source test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --native-input test/outshine/data/render/building-full-view/cell.json.gz \
  --cell 33 --output /tmp/outshine-source-models
```

Input generation is unchanged `building_transition.md`. Source geometry/material
provenance remains `source_material_patches.py`, `source_material_mesh.py` and
`building_mass_materials.py`; no new simplification or shading algorithm is added.
