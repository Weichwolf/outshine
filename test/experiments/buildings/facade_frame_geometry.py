"""Compose native outer frame products after bounded source/contact/detail admission."""

from dataclasses import asdict

import numpy as np
import shapely
from demand import displacement_error
from facade_frame_programme import choose
from outshine_ref.buildings.facade_frames import frame_surface


def footprint(plan):
    a, tangent = np.asarray(plan.origin_xy), np.asarray(plan.tangent)
    normal = np.array([tangent[1], -tangent[0]])
    b = a + tangent * plan.frame.width_m
    return shapely.Polygon(
        [a, b, b + normal * plan.frame.depth_m, a + normal * plan.frame.depth_m]
    )


def demand(plan, view):
    a, t = np.asarray(plan.origin_xy), np.asarray(plan.tangent)
    points = np.array(
        [
            [*(a + t * u), plan.base_m + v]
            for u in (0, plan.frame.width_m)
            for v in (0, plan.frame.height_m)
        ]
    )
    q = (points - view.eye) @ view.basis.T
    distance = q @ view.planes[:, :3].T + view.planes[:, 3]
    margin = (view.movement_m + plan.frame.depth_m) * np.linalg.norm(
        view.planes[:, :3], axis=1
    )
    visible = not np.any(distance.max(axis=0) + margin < 0)
    return (
        visible
        and displacement_error(points, view, plan.frame.depth_m) > view.detail_error_px
    )


def placed(template, plan, material):
    tangent = np.asarray(plan.tangent)
    basis = np.array([[*tangent, 0], [0, 0, 1], [tangent[1], -tangent[0], 0]])
    origin = np.r_[plan.origin_xy, plan.base_m]
    vertices = template["vertices"] @ basis + origin
    return {
        "vertices": vertices,
        "faces": template["faces"],
        "normals": template["normals"] @ basis,
        "uv": template["uv"],
        "materials": np.full(len(template["faces"]), material, dtype=np.uint16),
        "facade_extent": np.zeros((len(vertices), 3)),
    }


class FrameDetails:
    def __init__(self, entrances):
        self.entrances = entrances
        self.templates = {}
        self.completed, self.rejected = [], []

    def logical_info(self, count):
        if count == 0:
            return {}
        keys = (
            "id",
            "facade_id",
            "kind",
            "frame",
            "origin_xy_m",
            "tangent",
            "base_m",
            "origin",
        )
        return {
            "facade_frame_plan": [
                {k: r[k] for k in keys} for r in self.completed[-count:]
            ]
        }

    def products(self, row, parameters, facades, view, material, family, entry):
        plans, rejected = choose(row, parameters, facades, family, entry)
        self.rejected.extend(rejected)
        chunks, replacements = [], []
        for current in plans:
            try:
                support = footprint(current)
                if support.intersection(row["polygon"]).area > 1e-8:
                    raise ValueError(
                        "Facade frame intersects another wing of its source"
                    )
                if not self.entrances.clear(
                    row,
                    support,
                    current.base_m + current.frame.height_m,
                    low_m=current.base_m,
                ):
                    raise ValueError(
                        "Facade frame intersects a known height-overlapping neighbor"
                    )
                resolved = demand(current, view)
                if resolved:
                    if current.frame not in self.templates:
                        self.templates[current.frame] = frame_surface(
                            current.frame, back_covered=True
                        )
                    chunks.append(
                        placed(self.templates[current.frame], current, material)
                    )
                replacements.append(current.facade_id)
                self.completed.append(
                    {
                        "id": row["id"],
                        "facade_id": current.facade_id,
                        "kind": current.kind,
                        "frame": asdict(current.frame),
                        "origin_xy_m": current.origin_xy,
                        "tangent": current.tangent,
                        "base_m": current.base_m,
                        "mode": "outer" if resolved else "deferred",
                        "triangles": current.frame.triangle_count(True)
                        if resolved
                        else 0,
                        "origin": "procedural hierarchy on unchanged source repeat axes",
                        "scope": "Open source-attached decorative frame; closed reference and aperture plan share dimensions. Not collision or full image/shadow/native admission",
                    }
                )
            except (ValueError, shapely.errors.GEOSException) as error:
                self.rejected.append(
                    {
                        "id": row["id"],
                        "facade_id": current.facade_id,
                        "reason": str(error),
                    }
                )
        return chunks, tuple(replacements)
