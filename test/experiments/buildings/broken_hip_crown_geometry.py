"""Reuse bounded hip and two-slope cuts; keep the valid hip on profile failure."""

import time
from dataclasses import dataclass, fields
from types import MappingProxyType

import numpy as np
import shapely
from broken_hip import surface
from broken_hip_compare import logical
from broken_hip_crown_plan import BrokenHipCrown
from hip_crown_geometry import HEIGHT_EPSILON_M, ClosedHip
from hip_crown_geometry import build as hip_build
from outshine_ref.buildings.modelling import Scope
from outshine_ref.buildings.modelling.mesh_fans import connected_fans
from outshine_ref.buildings.modelling.prism_union import frozen
from outshine_ref.buildings.rooflight_panels import triangle_surface
from structural_shell_geometry import native
from terraced_crown_plan import ROOF_MATERIAL


@dataclass(frozen=True)
class ClosedBrokenHip(ClosedHip):
    run_fraction: float = 0.15
    height_fraction: float = 0.65
    maximum_source_height_correction_m: float = 0.0
    profile_work: object = None
    source_triangle_ids: object = None


def canonical_planes(product, crown):
    mesh = dict(product.mesh)
    vertices = np.array(mesh["vertices"], copy=True)
    old_z = vertices[:, 2].copy()
    if (
        old_z.min() < crown.deck_m - HEIGHT_EPSILON_M
        or old_z.max() > crown.top_m + HEIGHT_EPSILON_M
    ):
        raise ValueError("Broken crown original hip leaves source-height reserve")
    for height in (crown.deck_m, crown.top_m):
        vertices[abs(old_z - height) <= HEIGHT_EPSILON_M, 2] = height
    mesh["vertices"] = vertices
    xyz = vertices[mesh["faces"]]
    z_normal = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])[:, 2]
    if np.any(z_normal == 0):
        raise ValueError("Broken crown requires heightfield facets and a flat base")
    upward = z_normal > 0
    if not np.all(xyz[~upward, :, 2] == crown.deck_m):
        raise ValueError("Broken crown original base does not bind the source eave")
    return mesh, upward, float(abs(vertices[:, 2] - old_z).max())


def close_cap(upper, canonical, upward):
    retained = canonical["faces"][~upward]
    xyz = np.r_[
        upper["vertices"][upper["faces"]].reshape(-1, 3),
        canonical["vertices"][retained].reshape(-1, 3),
    ]
    uv = np.r_[
        upper["uv"][upper["faces"]].reshape(-1, 2),
        canonical["uv"][retained].reshape(-1, 2),
    ]
    raw = {
        "vertices": xyz,
        "uv": uv,
        "faces": np.arange(len(xyz), dtype=np.uint32).reshape(-1, 3),
        "materials": np.r_[upper["materials"], canonical["materials"][~upward]],
    }
    packed, triangles, duplicates, targets = connected_fans(
        raw, np.arange(len(raw["faces"]))
    )
    roots = np.arange(len(packed), dtype=np.uint64)
    roots[duplicates] = targets
    mesh = {
        "vertices": packed[:, :3],
        "uv": packed[:, 3:5],
        "faces": triangles.astype(np.int32),
        "materials": raw["materials"],
    }
    solid = native(mesh, roots)
    if solid.volume() <= 0:
        raise ValueError("Broken crown has no positive closed volume")
    return mesh, roots, solid


def profile(product, crown):
    canonical, upward, correction = canonical_planes(product, crown)
    count, lower = int(upward.sum()), int((~upward).sum())
    if not count or 3 * count + lower > crown.maximum_triangles:
        raise ValueError("Broken crown conservative triangle capacity exceeded")
    top = dict(
        canonical,
        faces=canonical["faces"][upward],
        materials=canonical["materials"][upward],
        groups=np.flatnonzero(upward).astype(np.uint32),
    )
    options = {
        "run_fraction": crown.run_fraction,
        "height_fraction": crown.height_fraction,
        "source_scope": Scope(
            position=(0.0, 0.0, crown.deck_m),
            size=(1.0, 1.0, crown.top_m - crown.deck_m),
        ),
    }
    if crown.profile_method == "direct":
        upper = surface(
            top,
            crown.deck_m,
            crown.top_m - crown.deck_m,
            max_triangles=crown.maximum_triangles - lower,
            **options,
        )
    else:
        upper = logical(top, crown.deck_m, crown.top_m - crown.deck_m, **options)
    mesh, roots, solid = close_cap(upper, canonical, upward)
    source_triangles = np.r_[upper["groups"], np.flatnonzero(~upward)].astype(np.int32)
    if len(mesh["faces"]) > crown.maximum_triangles:
        raise ValueError("Broken crown output exceeds its triangle capacity")
    values = {f.name: getattr(product, f.name) for f in fields(ClosedHip)}
    frozen_mesh = MappingProxyType({key: frozen(value) for key, value in mesh.items()})
    values.update(
        mesh=frozen_mesh,
        vertex_roots=frozen(roots),
        source_ids=(crown.identifier,),
        volume_m3=float(solid.volume()),
        area_m2=float(solid.surface_area()),
    )
    work = MappingProxyType(
        {
            "source_top_triangles": count,
            "source_bottom_triangles": lower,
            "slope_zones": 2,
            "source_cut_triangle_visits": 2 * count,
            "logical_split_outputs": 2 if crown.profile_method == "logical" else 0,
            "output_triangles": len(mesh["faces"]),
            "known_copied_bytes_lower_bound": canonical["vertices"].nbytes
            + roots.nbytes
            + source_triangles.nbytes
            + sum(value.nbytes for value in frozen_mesh.values()),
        }
    )
    result = ClosedBrokenHip(
        **values,
        run_fraction=crown.run_fraction,
        height_fraction=crown.height_fraction,
        maximum_source_height_correction_m=correction,
        profile_work=work,
        source_triangle_ids=frozen(source_triangles),
    )
    return (result,), triangle_surface(upper, ROOF_MATERIAL)


def build(crown):
    if not isinstance(crown, BrokenHipCrown):
        raise TypeError("Bound two-slope hip crown required")
    started = time.monotonic()
    original, original_skin = hip_build(crown.base_hip)
    try:
        if time.monotonic() - started >= crown.limits.seconds:
            raise ValueError(
                "Broken crown existing time budget exhausted before profile"
            )
        products, skin = profile(original[0], crown)
        if time.monotonic() - started >= crown.limits.seconds:
            raise ValueError(
                "Broken crown existing time budget exhausted after profile"
            )
        return products, skin, crown, None
    except (ValueError, NotImplementedError, shapely.errors.GEOSException) as error:
        return original, original_skin, crown.base_hip, str(error)
