"""Select facade cells before reusing existing closed/outer/front profile extrusion."""

import numpy as np
from clad_facade_mesh import surface
from facade_frame_geometry import placed
from terraced_facade_surface import joined


def expand(plan, selected_ids=None, *, mode="outer", reuse=True):
    if mode not in ("front", "outer", "closed"):
        raise ValueError("Unknown facade panel mode")
    known = {c.identifier for c in plan.cells}
    selected = known if selected_ids is None else set(selected_ids)
    if not selected <= known:
        raise ValueError("Facade selection contains a foreign cell")
    templates, products, owners = {}, [], []
    for index, cell in enumerate(plan.cells):
        if cell.identifier not in selected:
            continue
        for item in cell.layers:
            key = (item.region_wkb, item.depth_m, mode)
            if not reuse or key not in templates:
                templates[key] = surface(item, mode)
            product = placed(templates[key], item, item.material)
            products.append(product)
            owners.append(np.full(len(product["faces"]), index, dtype=np.int32))
    if not products:
        return {
            "vertices": np.empty((0, 3)),
            "faces": np.empty((0, 3), dtype=np.uint32),
            "normals": np.empty((0, 3)),
            "uv": np.empty((0, 2)),
            "materials": np.empty(0, dtype=np.uint16),
            "facade_extent": np.empty((0, 3)),
        }, np.empty(0, dtype=np.int32)
    return joined(tuple(products)), np.concatenate(owners)
