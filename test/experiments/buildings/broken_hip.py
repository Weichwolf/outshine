"""Continuous two-slope urban roofs from the existing conformal hip surface."""

import numpy as np
import shapely
from outshine_ref.buildings.modelling import Shape, heightfield_envelope
from outshine_ref.buildings.modelling.cut_geos import Slab
from outshine_ref.buildings.modelling.cut_source import prepare_source
from outshine_ref.buildings.modelling.scope import scalar


def surface(
    top, eave_m, rise_m, *, run_fraction=0.15, height_fraction=0.65, max_triangles=65536
):
    eave, rise = scalar(eave_m), scalar(rise_m)
    run, height = scalar(run_fraction), scalar(height_fraction)
    if rise <= 0 or not 0 < run < height < 1:
        raise ValueError("Broken hip requires positive rise and 0 < run < height < 1")
    count = len(top["faces"])
    if type(max_triangles) is not int or max_triangles < 3 * count or count == 0:
        raise ValueError("Broken hip conservative triangle capacity exceeded")
    allowed = {
        "vertices",
        "faces",
        "uv",
        "normals",
        "materials",
        "groups",
        "parts",
        "face_tag_mask",
    }
    if set(top) - allowed:
        raise ValueError("Broken hip has no inheritance contract for extra channels")
    points = top["vertices"][top["faces"]]
    tolerance = max(1e-10, 64 * np.finfo(float).eps * max(abs(eave), rise, 1.0))
    if np.any(points[:, :, 2] < eave - tolerance) or np.any(
        points[:, :, 2] > eave + rise + tolerance
    ):
        raise ValueError("Broken hip input exceeds its declared height interval")
    knot = eave + rise * run
    if np.any(np.all(abs(points[:, :, 2] - knot) <= tolerance, axis=1)):
        raise ValueError("Broken hip split cannot duplicate a coplanar knot surface")
    source = prepare_source(
        Shape.from_mesh(
            {
                "vertices": points.reshape(-1, 3),
                "faces": np.arange(points.size // 3).reshape(-1, 3),
                "uv": top["uv"][top["faces"]].reshape(-1, 2),
                "materials": top["materials"],
            }
        )
    )
    if source.closed:
        raise ValueError("Broken hip requires an open upper surface")
    offset = eave - source.shape.scope.origin[2]
    properties, owners = [], []
    for low, high in (
        (offset, offset + rise * run),
        (offset + rise * run, offset + rise),
    ):
        cut = Slab(source, 2, low, high, None, 0)
        cut.surfaces()
        if not cut.keys:
            raise ValueError("Broken hip profile has an empty slope zone")
        properties.append(np.asarray(cut.properties))
        owners.extend(key[1] for key in cut.keys)
    data = np.concatenate(properties)
    vertices = data[:, :3].copy()
    level = (vertices[:, 2] - eave) / rise
    mapped = np.where(
        level <= run,
        level * height / run,
        height + (level - run) * (1 - height) / (1 - run),
    )
    vertices[:, 2] = eave + rise * mapped
    for original, target in (
        (eave, eave),
        (knot, eave + rise * height),
        (eave + rise, eave + rise),
    ):
        vertices[abs(data[:, 2] - original) <= tolerance, 2] = target
    triangles = vertices.reshape(-1, 3, 3)
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    lengths = np.linalg.norm(normals, axis=1)
    if (
        not np.isfinite(vertices).all()
        or np.any(lengths == 0)
        or np.any(normals[:, 2] <= 0)
    ):
        raise ValueError("Broken hip has an unresolved or reversed top triangle")
    normals /= lengths[:, None]
    mesh = {
        "vertices": vertices,
        "faces": np.arange(len(vertices), dtype=np.uint32).reshape(-1, 3),
        "uv": data[:, 3:5].copy(),
        "normals": np.repeat(normals, 3, axis=0),
    }
    for name in ("materials", "groups", "parts", "face_tag_mask"):
        if name in top:
            mesh[name] = top[name][owners].copy()
    return mesh


def recipe(row, parameters, choice):
    """A morphology/use estimate, independent of city names, camera and source IDs."""
    if choice.kind != "hip" or choice.origin != "procedural urban size/height estimate":
        return None
    properties = row["properties"]
    if any(key.startswith(("roof:", "roof_")) for key in properties):
        return None
    height = parameters["bodyHeight"]
    polygon = row["polygon"]
    ratio = height / np.sqrt(polygon.area)
    if not 13 <= height <= 30 or not 0.55 <= ratio <= 1.4:
        return None
    return {
        "kind": "broken-two-slope-hip",
        "run_fraction": 0.15,
        "height_fraction": 0.65,
        "origin": "procedural urban morphology estimate; source roof type unproven",
    }


def envelope(top, parameters, material, choice, profile):
    options = {"wall_material": material, "base_material": material}
    if profile is None:
        return heightfield_envelope(top, parameters["minHeight"], **options), None
    try:
        upper = surface(
            top,
            choice.eave_m,
            choice.rise_m,
            run_fraction=profile["run_fraction"],
            height_fraction=profile["height_fraction"],
        )
        product = heightfield_envelope(upper, parameters["minHeight"], **options)
        return product, profile | {"status": "constructed"}
    except (ValueError, NotImplementedError, shapely.errors.GEOSException) as error:
        fallback = heightfield_envelope(top, parameters["minHeight"], **options)
        return fallback, profile | {"status": "hip-fallback", "reason": str(error)}
