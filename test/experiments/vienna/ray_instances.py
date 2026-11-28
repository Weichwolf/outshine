"""Ray traversal over repeated meshes kept in their original coordinates."""

from dataclasses import dataclass
from math import inf

import numpy as np
from mathutils import Vector


def ray_box(bounds, origin, inverse, limit):
    """Return the first distance in an AABB, including a ray starting inside it."""
    x, y, z = origin
    dx, dy, dz = inverse
    x0, y0, z0, x1, y1, z1 = bounds
    a, b = (x0 - x) * dx, (x1 - x) * dx
    c, d = (y0 - y) * dy, (y1 - y) * dy
    e, f = (z0 - z) * dz, (z1 - z) * dz
    near = max(min(a, b), min(c, d), min(e, f), 0)
    far = min(max(a, b), max(c, d), max(e, f), limit)
    return near if near <= far else inf


@dataclass(frozen=True, slots=True)
class RayInstance:
    bounds: tuple
    tree: object
    face_offset: int
    inverse: object
    local_direction: object
    world_normal: object

    @classmethod
    def make(cls, tree, face_offset, transform, bounds):
        inverse = transform.inverted()
        linear = inverse.to_3x3()
        normal_orientation = -1 if linear.determinant() < 0 else 1
        normal = linear.transposed() * normal_orientation
        return cls(bounds, tree, face_offset, inverse, linear, normal)

    def cast(self, origin, direction, limit):
        local_dir = self.local_direction @ direction
        scale = local_dir.length
        _, normal, face, distance = self.tree.ray_cast(
            self.inverse @ origin, local_dir, limit * scale
        )
        if face is None:
            return None
        distance /= scale
        point = origin + direction * distance
        return (
            point,
            (self.world_normal @ normal).normalized(),
            self.face_offset + face,
            distance,
        )


@dataclass(frozen=True, slots=True)
class RayBranch:
    bounds: tuple
    left: object
    right: object


def instance_tree(instances):
    if len(instances) == 1:
        return instances[0]
    boxes = np.asarray([instance.bounds for instance in instances])
    lower, upper = boxes[:, :3].min(axis=0), boxes[:, 3:].max(axis=0)
    centres = (boxes[:, :3] + boxes[:, 3:]) * 0.5
    axis = int(np.argmax(np.ptp(centres, axis=0)))
    indices = np.argsort(centres[:, axis], kind="stable")
    middle = len(instances) // 2
    return RayBranch(
        (*lower.tolist(), *upper.tolist()),
        instance_tree([instances[i] for i in indices[:middle]]),
        instance_tree([instances[i] for i in indices[middle:]]),
    )


class InstancedRays:
    def __init__(self, static, instances):
        self.static = static
        self.root = instance_tree(instances)

    def ray_cast(self, origin, direction, distance):
        origin = Vector(origin)
        direction = Vector(direction).normalized()
        hit = self.static.ray_cast(origin, direction, distance)
        limit = hit[3] if hit[2] is not None else distance
        ray_origin = tuple(origin)
        inverse = tuple(1 / value if value else 1e30 for value in direction)
        near = ray_box(self.root.bounds, ray_origin, inverse, limit)
        if near == inf:
            return hit
        pending = [(near, self.root)]
        while pending:
            near, node = pending.pop()
            if near > limit:
                continue
            if isinstance(node, RayInstance):
                candidate = node.cast(origin, direction, limit)
                if candidate is not None:
                    hit = candidate
                    limit = candidate[3]
                continue
            left = ray_box(node.left.bounds, ray_origin, inverse, limit)
            right = ray_box(node.right.bounds, ray_origin, inverse, limit)
            if left < right:
                left, right, a, b = right, left, node.right, node.left
            else:
                a, b = node.left, node.right
            if left != inf:
                pending.append((left, a))
            if right != inf:
                pending.append((right, b))
        return hit
