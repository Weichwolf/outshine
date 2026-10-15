"""Same facade axes: bounded profile bands in window-free heights and free XY edges."""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.buildings.profiled_bands import (
    BandPlan,
    band_surface,
    edge_footprints,
)
from provider_contract import attributes


@dataclass(frozen=True)
class BandSpec:
    kind: str
    base_m: float
    profile: tuple


def specifications(facades, bottom):
    if not facades:
        return []
    first = facades[0]
    height, rows = first["height_m"], first["rows"]
    if any(f["height_m"] != height or f["rows"] != rows for f in facades):
        raise ValueError("Common facade storey intervals required for joined bands")
    floor = height / rows
    gap = min(floor * (1 - f["fractions"][1]) for f in facades)
    if gap < 0.2:
        return []
    plinth = min(0.42, gap * 0.4)
    specs = [
        BandSpec(
            "plinth", bottom, ((0.0, 0.0), (0.08, 0.0), (0.08, plinth), (0.0, plinth))
        )
    ]
    belt = min(0.18, gap * 0.32)
    for level in sorted({1, rows - 1} if rows >= 6 else ({1} if rows >= 3 else set())):
        profile = (
            (0.0, 0.0),
            (0.07, 0.0),
            (0.12, belt * 0.35),
            (0.12, belt),
            (0.0, belt),
        )
        specs.append(BandSpec("belt", bottom + floor * level - belt / 2, profile))
    cornice = min(0.42, gap * 0.4)
    profile = tuple(
        (d, z * cornice)
        for d, z in (
            (0.0, 0.0),
            (0.07, 0.0),
            (0.11, 0.25),
            (0.23, 0.25),
            (0.23, 0.65),
            (0.12, 0.65),
            (0.12, 1.0),
            (0.0, 1.0),
        )
    )
    specs.append(BandSpec("cornice", bottom + height - cornice, profile))
    return specs


def demanded(frame, spec, view):
    a, b = np.asarray(frame["a"]), np.asarray(frame["b"])
    points = np.array(
        [
            [*a, spec.base_m],
            [*b, spec.base_m],
            [*b, spec.base_m + spec.profile[-1][1]],
            [*a, spec.base_m + spec.profile[-1][1]],
        ]
    )
    # Expand clipping planes conservatively for any displacement up to the miter limit.
    radius = max(d for d, _ in spec.profile) * 4
    expanded = view.movement_m + radius
    coordinates = (points - view.eye) @ view.basis.T
    distances = coordinates @ view.planes[:, :3].T + view.planes[:, 3]
    if np.any(
        distances.max(axis=0) + expanded * np.linalg.norm(view.planes[:, :3], axis=1)
        < 0
    ):
        return False
    nearest = float(coordinates[:, 2].min()) - view.movement_m
    if nearest <= radius:
        return True
    numerator = radius * (np.linalg.norm(coordinates, axis=1).max() + view.movement_m)
    return (
        view.focal_px * numerator / (nearest * (nearest - radius))
        > view.detail_error_px
    )


class BandDetails:
    def __init__(self, rows):
        self.rows = [r for r in rows if r.get("layer", "building") == "building"]
        self.tree = shapely.STRtree([r["polygon"] for r in self.rows])
        self.completed = []
        self.rejected = []

    def free_edges(self, row, spec, footprints, requested):
        edges, blocked = [], []
        for i in requested:
            support = footprints[i]
            collision = False
            for j in self.tree.query(support, predicate="intersects"):
                neighbor = self.rows[j]
                if neighbor["id"] == row["id"]:
                    continue
                p = attributes(neighbor["properties"])
                if (
                    p["minHeight"] < spec.base_m + spec.profile[-1][1]
                    and p["buildingHeight"] > spec.base_m
                    and support.intersection(neighbor["polygon"]).area > 1e-9
                ):
                    collision = True
                    break
            (blocked if collision else edges).append(i)
        return edges, blocked

    def __call__(self, mesh, row, parameters, facades, view, material, family):
        if family not in (
            "residential",
            "commercial",
            "mixed-use",
            "institutional",
        ) or row.get("clipped"):
            return mesh
        try:
            specs = specifications(facades, parameters["minHeight"])
            requests = [
                (s, tuple(i for i, f in enumerate(facades) if demanded(f, s, view)))
                for s in specs
            ]
            requests = [(s, e) for s, e in requests if e]
            if not requests:
                return mesh
            chunks = []
            records = []
            remaining = 4096
            for spec, requested in requests:
                plan = BandPlan(row["polygon"], spec.profile, spec.base_m)
                footprints = edge_footprints(plan)
                if len(footprints) != len(facades):
                    raise ValueError(
                        "Complete rectangular source boundary frames required"
                    )
                for f, support in zip(facades, footprints, strict=True):
                    if not np.allclose(
                        [f["a"], f["b"]],
                        np.asarray(support.exterior.coords)[:2],
                        atol=1e-8,
                        rtol=0,
                    ):
                        raise ValueError("Facade boundary identity/order changed")
                edges, blocked = self.free_edges(row, spec, footprints, requested)
                if not edges:
                    self.rejected.append(
                        {
                            "id": row["id"],
                            "kind": spec.kind,
                            "reason": "all demanded edges overlap neighboring source buildings",
                            "edges": blocked,
                        }
                    )
                    continue
                emitted = band_surface(plan, tuple(edges), max_triangles=remaining)
                remaining -= len(emitted["faces"])
                emitted["materials"] = np.full(
                    len(emitted["faces"]), material, dtype=np.uint16
                )
                emitted["facade_extent"] = np.zeros((len(emitted["vertices"]), 3))
                emitted.pop("edge_ids")
                chunks.append(emitted)
                records.append(
                    {
                        "id": row["id"],
                        "kind": spec.kind,
                        "base_m": spec.base_m,
                        "profile": spec.profile,
                        "edges": edges,
                        "neighbor_blocked": blocked,
                        "triangles": len(emitted["faces"]),
                        "origin": "procedural existing repeat gaps",
                    }
                )
            if not chunks:
                return mesh
            chunks.insert(0, mesh)
            offsets = np.cumsum([0] + [len(c["vertices"]) for c in chunks[:-1]])
            result = {
                key: np.concatenate([c[key] for c in chunks])
                for key in ("vertices", "normals", "uv", "facade_extent", "materials")
            }
            result["faces"] = np.concatenate(
                [c["faces"] + offset for c, offset in zip(chunks, offsets, strict=True)]
            ).astype(np.uint32)
            self.completed.extend(records)
            return result
        except ValueError as error:
            self.rejected.append({"id": row["id"], "reason": str(error)})
            return mesh
