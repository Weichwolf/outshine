"""Bounded exact source placements using demand-built shared leaf patch prototypes."""

import hashlib
import json
from functools import cached_property

import numpy as np
from group_cloud import Chart
from leaf_patch_prototype import build_prototype


class LeafPatchCatalog:
    def __init__(self, source, patches=2, size=16):
        if type(patches) is not int or patches not in (1, 2, 4):
            raise ValueError("one, two or four leaf patches required")
        if type(size) is not int or size not in (8, 16, 32):
            raise ValueError("bounded isolated patch resolution required")
        self.source, self.patches, self.size = source, patches, size
        self.prototypes = {}
        self.areas = {}

    @cached_property
    def identity(self):
        recipe = {
            "format": "exact-pose-leaf-patches-1",
            "source": self.source.identity,
            "patches": self.patches,
            "size": self.size,
            "source_mesh": [8, 1],
            "normal_field": "four-positive-signed-mass-first-moment-1",
        }
        return hashlib.sha256(json.dumps(recipe, sort_keys=True).encode()).hexdigest()

    def group(self, node, max_leaves=128):
        if type(max_leaves) is not int or not 0 <= max_leaves <= 128:
            raise ValueError("explicit leaf capacity up to128 required")
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
                self.prototypes[variant] = build_prototype(
                    blade, variant, self.patches, self.size
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
        }

    def charts(self, group):
        """Diagnostic emission only: runtime may instance the shared templates instead."""
        poses = group["poses"]
        origin, frame = self.source.crown.stem(group["host_node"])
        charts = []
        for index, (point, rotation, variant) in enumerate(
            zip(poses["origins"], poses["frames"], poses["variants"], strict=True)
        ):
            r, p = frame.T @ rotation, (point - origin) @ frame
            for template in self.prototypes[int(variant)].charts:
                normal = r @ template.frame[:, 2]
                mesh = template.mesh | {
                    "vertices": template.mesh["vertices"] @ r.T + p,
                    "normals": template.mesh["normals"] @ r.T,
                }
                charts.append(
                    Chart(
                        mesh,
                        template.colour,
                        template.normal,
                        np.array([index]),
                        r @ template.frame,
                        template.offset + float(p @ normal),
                        template.displacement_m,
                    )
                )
        return tuple(charts)

    def summary(self):
        charts = [c for p in self.prototypes.values() for c in p.charts]
        return {
            "identity": self.identity,
            "patches_per_leaf": self.patches,
            "size": self.size,
            "prepared_variants": len(self.prototypes),
            "prepared_charts": len(charts),
            "source_triangles_built": sum(
                p.source_triangles for p in self.prototypes.values()
            ),
            "chart_image_array_bytes": sum(
                c.colour.nbytes + c.normal.nbytes for c in charts
            ),
            "normal_moment_array_bytes": sum(
                a.nbytes for p in self.prototypes.values() for a in p.normal_moments
            ),
            "chart_mesh_array_bytes": sum(
                a.nbytes for c in charts for a in c.mesh.values()
            ),
            "retained_pose_groups": len(self.source.crown._groups),
        }
