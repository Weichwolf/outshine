"""Scoped existing NumPy/Memray comparison, no native or full-memory claim."""

import numpy as np
from field_view_cost import measured
from triangle_columns import prepare, query
from triangle_image_reference import prepare_source


def known_rtree_arrays(prepared):
    mesh = prepared["mesh"]
    values = {
        "vertices": mesh.vertices,
        "faces": mesh.faces,
        "normal": prepared["normal"],
    }
    values.update(
        {
            "cache/" + k: v
            for k, v in mesh._cache.cache.items()
            if isinstance(v, np.ndarray)
        }
    )
    unique = []
    for value in values.values():
        if not any(np.shares_memory(value, v) for v in unique):
            unique.append(value)
    return {
        "observed_numeric_array_bytes": sum(v.nbytes for v in unique),
        "array_inventory": {k: v.nbytes for k, v in values.items()},
        "scope": "Observed installed Trimesh numeric buffers, overlapping views counted once. Native R-tree heap/Python objects excluded; not full residency or peak.",
    }


def query_costs(field, world, points, raster, expected, output, pose, reference_call):
    import trimesh

    result = {"trimesh_version": trimesh.__version__, "numpy_version": np.__version__}
    for name, call in (
        ("columns64", lambda: query(field, world, points, raster)),
        ("existing-rtree", reference_call),
    ):
        value, cost = measured(call, output, pose + "-" + name)
        np.testing.assert_array_equal(value[0] > 0, expected > 0)
        np.testing.assert_allclose(value[0], expected, atol=1e-18, rtol=1e-12)
        if name == "columns64" and not value[2].all():
            raise ValueError(
                "Instrumentation reached cooperative limit; result unqualified"
            )
        result[name] = cost
    return result


def build_costs(source, capture, output):
    result = {}
    for name, call in (
        ("columns64", lambda: prepare(source, capture, 64)),
        ("existing-rtree", lambda: prepare_source(source)),
    ):
        value, cost = measured(call, output, "prepare-" + name)
        if name == "columns64":
            cost["numeric_payload_bytes"] = value.payload_bytes
            np.testing.assert_array_equal(value.positions, source["positions"])
            np.testing.assert_array_equal(value.triangles, source["indices"])
        else:
            cost["observed_numeric_storage"] = known_rtree_arrays(value)
        result[name] = cost
    return result
