"""Metadata-only field planning; candidates grant no native replacement authority.

Compare centred grouping with the panoramic capture domain used by World. Report
how whole-cell admission blocks otherwise eligible groups. No raster/mesh payloads
are read, and estimated candidates never certify coverage, appearance or GPU cost.
"""

import math
from dataclasses import dataclass

import numpy as np

SIGNS = np.array([(x, y, z) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)])


@dataclass(frozen=True)
class FieldProduct:
    node: int
    kind: str
    pixels: int = 0
    error_px: float = 0


def enu_rows(anchor):
    if len(anchor) != 3 or not all(map(math.isfinite, anchor)) or math.hypot(*anchor) == 0:
        raise ValueError("nonzero finite Earth anchor required")
    x, y, z = anchor
    longitude = math.atan2(y, x)
    horizontal = math.hypot(x, y)
    eccentricity = (1 / 298.257223563) * (2 - 1 / 298.257223563)
    latitude = math.atan2(z, horizontal * (1 - eccentricity))
    for _ in range(16):
        normal = 6378137 / math.sqrt(1 - eccentricity * math.sin(latitude) ** 2)
        updated = math.atan2(z + eccentricity * normal * math.sin(latitude), horizontal)
        if abs(updated - latitude) < 1e-14:
            latitude = updated
            break
        latitude = updated
    else:
        raise ValueError("geodetic frame did not converge")
    a, b = latitude, longitude
    return np.array([[-math.sin(b), math.cos(b), 0],
                     [math.cos(a) * math.cos(b), math.cos(a) * math.sin(b), math.sin(a)],
                     [math.sin(a) * math.cos(b), math.sin(a) * math.sin(b), -math.cos(a)]])


class FieldGroups:
    def __init__(self, tree, anchor, eye, axes, focal, viewport, error_px):
        self.tree = tree
        self.anchor, self.eye, self.axes = map(np.asarray, (anchor, eye, axes))
        if (self.anchor.shape != (3,) or self.eye.shape != (3,)
                or self.axes.shape != (3, 3)
                or not np.all(np.isfinite((*self.anchor, *self.eye, *self.axes.flat)))
                or not np.allclose(self.axes @ self.axes.T, np.eye(3), atol=1e-12, rtol=0)
                or np.linalg.det(self.axes) < 0
                or not math.isfinite(focal) or focal <= 0
                or len(viewport) != 2 or any(not math.isfinite(v) or v <= 0 for v in viewport)
                or not math.isfinite(error_px) or error_px < 0):
            raise ValueError("finite rigid frame, eye, viewport and error budget required")
        self.focal, self.viewport, self.allowed = focal, tuple(viewport), error_px
        self.world = np.array([node[:6] for node in tree.nodes])
        self.local = np.empty_like(self.world)
        for at in range(len(tree.nodes) - 1, -1, -1):
            node = tree.nodes[at]
            if node[7] == 1:
                bounds = self.world[at]
                centre = (bounds[:3] + bounds[3:]) / 2 - self.anchor
                half = (bounds[3:] - bounds[:3]) / 2
                middle, extent = self.axes @ centre, np.abs(self.axes) @ half
                self.local[at] = np.r_[middle - extent, middle + extent]
            else:
                left, right = node[8:]
                self.local[at, :3] = np.minimum(self.local[left, :3], self.local[right, :3])
                self.local[at, 3:] = np.maximum(self.local[left, 3:], self.local[right, 3:])

    def capture(self, at):
        box = self.world[at]
        radius = math.dist(box[:3], box[3:])
        distance = math.dist(self.eye, (box[:3] + box[3:]) / 2) - radius
        if (distance <= 0 or self.allowed <= 0
                or self.focal * radius**2 / distance**2 > self.allowed):
            return None
        required = math.ceil(4 * self.focal * radius / distance) + 2
        if required > 4096:
            return None
        pixels = 1 << max(2, (required - 1).bit_length())
        direction = self.axes @ (self.eye - (box[:3] + box[3:]) / 2)
        direction /= np.linalg.norm(direction)
        bounds = self.local[at]
        centre, half = (bounds[:3] + bounds[3:]) / 2, (bounds[3:] - bounds[:3]) / 2
        right = np.array([direction[2], 0, -direction[0]])
        size = np.linalg.norm(right)
        right = right / size if size else np.array([1., 0, 0])
        up = np.cross(direction, right)
        h = max(np.abs(right) @ half, np.abs(up) @ half,
                .5 * (np.abs(direction) @ half)) * pixels / (pixels - 2)
        offsets = SIGNS @ np.array([right * h, up * h, direction * 2 * h])
        relative = self.anchor - self.eye
        rays = (offsets + centre) @ self.axes + relative
        support = (offsets + centre - (SIGNS[:, 2] * 2 * h)[:, None] * direction)
        support = support @ self.axes + relative
        return pixels, h, direction @ self.axes, rays, support

    def candidate(self, at):
        capture = self.capture(at)
        if capture is None:
            return None
        pixels, half, direction, rays, support = capture
        joint = np.vstack([rays, support])
        least, most = joint.min(axis=0), joint.max(axis=0)
        closest = np.maximum(np.maximum(least, -most), 0)
        scale = math.hypot(1, *((v * .5 + .5) / self.focal for v in self.viewport))
        depth = np.linalg.norm(closest) / scale - np.linalg.norm(most - least)
        if depth <= 0:
            return None
        numerator = np.linalg.norm(np.cross(rays, direction), axis=1).max()
        error = 2 * half * numerator * math.hypot(self.focal, self.focal) / depth**2
        return (FieldProduct(at, "field", pixels, float(error))
                if error <= self.allowed else None)

    def cut(self):
        pending, products, visited = [0], [], 0
        while pending:
            at = pending.pop()
            visited += 1
            node = self.tree.nodes[at]
            candidate = (self.candidate(at) if self.tree.supported[at]
                         and self.tree.cohorts[at] is not None else None)
            if candidate is not None:
                products.append(candidate)
            elif node[7] == 1:
                products.append(FieldProduct(at, "geometry" if self.tree.supported[at]
                                             else "unresolved"))
            else:
                pending.extend((node[9], node[8]))
        return products, visited

    def costs(self, products, cells, *, verify=False):
        if verify:
            represented = [source for product in products for source in self.tree.order[
                self.tree.nodes[product.node][6]:sum(self.tree.nodes[product.node][6:8])]]
            if sorted(represented) != list(range(len(cells))):
                raise ValueError("products do not partition the complete source inventory")
        fields, blocked = set(), set()
        groups = texels = 0
        for product in products:
            node = self.tree.nodes[product.node]
            members = self.tree.order[node[6]:sum(node[6:8])]
            candidate = self.candidate(product.node) if product.kind == "field" else None
            if candidate is not None:
                fields.update(members)
                groups += 1
                texels += candidate.pixels**2
            else:
                blocked.update(int(cells[source]) for source in members)
        eligible = sum(int(cells[source]) not in blocked for source in fields)
        return {"products": len(products), "eligible_groups": groups,
                "eligible_sources": len(fields), "eligible_capture_texels": texels,
                "cell_admitted_sources": eligible,
                "cell_blocked_eligible_sources": len(fields) - eligible,
                "fully_eligible_cells": len(set(map(int, cells)) - blocked),
                "residual_sources": len(cells) - len(fields)}
