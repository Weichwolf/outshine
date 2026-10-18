"""Demand-built connected leaves at the exact original saved spatial crown poses."""

import hashlib
import json
from functools import cached_property

import numpy as np
from connected_leaf import build_connected


class ConnectedCatalog:
    def __init__(self, source, *, size=32, segments=2):
        if type(size) is not int or size not in (16, 32, 64):
            raise ValueError("bounded whole-leaf map size required")
        if type(segments) is not int or segments not in (2, 4):
            raise ValueError("two or four connected intervals required")
        self.source, self.size, self.segments = source, size, segments
        self.prototypes = {}
        self.areas = {}

    @cached_property
    def identity(self):
        recipe = {
            "format": "exact-source-connected-catalog-1",
            "source_catalog": self.source.identity,
            "size": self.size,
            "segments": self.segments,
            "normal_space": "blade-object-premultiplied-raw-first-moment",
        }
        return hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()

    def group(self, node, max_leaves=128):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 128:
            raise ValueError("explicit host leaf capacity up to128 required")
        crown = self.source.crown
        i = crown._host_index(node)
        if int(crown.counts[i]) > max_leaves:
            return None
        if node not in crown._groups:
            crown._groups.clear()
        poses = crown.group(node, max_leaves)
        for variant in np.unique(poses["variants"]):
            variant = int(variant)
            if variant not in self.prototypes:
                blade = self.source.blades[variant]
                self.prototypes[variant] = build_connected(
                    blade, variant, size=self.size, segments=self.segments
                )
                self.areas[variant] = blade.area()
        physical = np.array([self.areas[int(v)] for v in poses["variants"]])
        physical.setflags(write=False)
        return {
            "poses": poses,
            "host_node": node,
            "plan_identity": crown.identity,
            "source_catalog_identity": self.source.identity,
            "representation_identity": self.identity,
            "physical_leaf_area_m2": physical,
            "material_inputs_json": self.source.material_json,
            "normal_space": "blade-object-premultiplied-raw-first-moment",
        }

    def surfaces(self, group):
        """Requested diagnostic draw geometry; game assets can instance the same templates."""
        origin, frame = self.source.crown.stem(group["host_node"])
        poses = group["poses"]
        surfaces = []
        for point, rotation, variant, owner in zip(
            poses["origins"],
            poses["frames"],
            poses["variants"],
            poses["ids"],
            strict=True,
        ):
            p = self.prototypes[int(variant)]
            r = frame.T @ rotation
            mesh = p["mesh"] | {
                "vertices": (p["mesh"]["vertices"] @ rotation.T + point - origin)
                @ frame,
                "normals": p["mesh"]["normals"] @ r.T,
            }
            surfaces.append(
                {
                    "mesh": mesh,
                    "colour": p["colour"],
                    "normal": p["normal"],
                    "basis": r,
                    "owner": owner,
                }
            )
        return tuple(surfaces)

    def summary(self):
        return {
            "identity": self.identity,
            "size": self.size,
            "segments": self.segments,
            "prepared_variants": len(self.prototypes),
            "source_triangles_built": sum(
                p["source_triangles"] for p in self.prototypes.values()
            ),
            "image_array_bytes": sum(
                p["colour"].nbytes + p["normal"].nbytes
                for p in self.prototypes.values()
            ),
            "mesh_array_bytes": sum(
                a.nbytes for p in self.prototypes.values() for a in p["mesh"].values()
            ),
            "maximum_paired_source_bound_m": max(
                (p["paired_source_surface_bound_m"] for p in self.prototypes.values()),
                default=0,
            ),
            "retained_pose_groups": len(self.source.crown._groups),
        }
