"""Bounded world-space wood axes and geometric/chart access for generator graphs."""

from dataclasses import dataclass
from types import SimpleNamespace

import numpy as np
from bark_coordinates import axis_chart
from graph_sources import source_parts, source_triangles
from spatial_axis import SpatialAxis
from spine_closest import closest_spine


@dataclass(frozen=True)
class WoodAxis:
    path: tuple
    pose: object
    shape: object

    def centres(self, parameter):
        if isinstance(self.shape, SpatialAxis):
            local = (
                self.shape.spine.evaluate(
                    self.shape.measure.parameter(np.asarray(parameter))
                )
                * self.pose.scale
            )
        else:
            from curve_frame import arc_frame

            local = arc_frame(
                self.pose.length, self.pose.bend, 0, np.asarray(parameter)
            )[0]
        return local @ self.pose.frame.T + self.pose.origin

    def radius(self, parameter, inscribed=False):
        factor = (
            min(self.shape.ellipse)
            if inscribed and isinstance(self.shape, SpatialAxis)
            else 1
        )
        return self.shape.radius.evaluate(parameter) * self.pose.scale * factor

    def chart(self, points, method="bounded"):
        if not isinstance(self.shape, SpatialAxis):
            level = SimpleNamespace(base_radius=1.0, tip_radius=1.0)
            coordinates, _, distance = axis_chart(self.pose, level, points)
            u = coordinates[:, 2] / self.pose.length
            return coordinates, self.radius(u), distance
        local = (points - self.pose.origin) @ self.pose.frame / self.pose.scale
        raw, distance = closest_spine(self.shape.spine, local, method)
        u = self.shape.measure.fraction(raw)
        sample = self.shape.field.sample(np.clip(u, 0, 1))
        relative = (local - sample["centre"]) * self.pose.scale
        coordinates = np.column_stack(
            (
                np.sum(relative * sample["lateral"], axis=1),
                np.sum(relative * sample["normal"], axis=1),
                u * self.pose.length,
            )
        )
        radius = self.radius(u) * np.sqrt(np.prod(self.shape.ellipse))
        return coordinates, radius, distance * self.pose.scale

    def bounds(self):
        centre = self.centres(np.array([0.5]))[0]
        if isinstance(self.shape, SpatialAxis):
            local = self.shape.field.sample(0.5)["centre"]
            length = (
                np.linalg.norm(
                    np.asarray(self.shape.spine.controls) - local, axis=-1
                ).max()
                * self.pose.scale
            )
            outer = max(self.shape.ellipse)
        else:
            length = self.pose.length / 2
            outer = 1.0
        return centre, float(
            length + 2 * outer * self.shape.radius.table.max() * self.pose.scale
        )


def wood_axes(graph, detail="near", max_axes=128, max_triangles=1_000_000):
    if (
        detail not in ("near", "coarse")
        or type(max_axes) is not int
        or not 1 <= max_axes <= 128
        or type(max_triangles) is not int
        or not 0 <= max_triangles <= 1_000_000
    ):
        raise ValueError("declared complete wood detail and bounded budgets required")
    ids = [key for key in graph.order if graph.by_id[key].kind == "axis"]
    count = sum(graph.capacities[key] for key in ids)
    triangles = sum(
        graph.capacities[key] * source_triangles(graph.by_id[key].shape, detail)
        for key in ids
    )
    if count > max_axes or triangles > max_triangles:
        return None
    axes, meshes = [], []
    prototypes = {}
    for key in ids:
        node = graph.by_id[key]
        identity = str(node.shape.recipe())
        if identity not in prototypes:
            prototypes[identity] = source_parts(node.shape, detail)["wood"]
        mesh = prototypes[identity]
        for pose in graph.instances(key, 100_000):
            axes.append(WoodAxis(pose.path, pose, node.shape))
            meshes.append(
                {
                    "vertices": mesh["vertices"] @ (pose.frame * pose.scale).T
                    + pose.origin,
                    "faces": mesh["faces"],
                    "normals": mesh["normals"] @ pose.frame.T,
                }
            )
    return tuple(axes), meshes
