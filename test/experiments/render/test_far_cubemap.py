"""Check cube directions, reverse-z reconstruction, translated planes and silhouettes."""
import numpy as np
from compose_panorama_overlap import overlap_weight
from far_cubemap import (
    FORWARD, RIGHT, DOWN, addresses, anchor_overlap, anchor_weight, directions, from_device_depth,
    reproject, unit,
)

side = 256
rays = directions(side)
face, row, column = addresses(rays, side)
assert np.all(face == np.arange(6)[:, None, None])
assert np.all(row == np.arange(side)[None, :, None])
assert np.all(column == np.arange(side)[None, None, :])
assert np.array_equal(np.cross(RIGHT, DOWN), -FORWARD)

center_distance = 3950
near, far = 1200, 240000
bias = near / (far - near)
device = np.full((side, side), far * bias / center_distance - bias)
radial = from_device_depth(device, near, far)
assert np.max(abs(radial - center_distance / rays[0, :, :, 0])) < .0005
assert np.all(from_device_depth(np.zeros_like(device), near, far) == 0)

axial = np.array([0, 1509, 1510, 1760, 1910, 2060, 2100])
axial_scale = np.array([[1.], [1.5]])
assert np.array_equal(overlap_weight(axial * axial_scale, axial_scale, 1510, 1760, 300),
                      np.tile([0, 0, 1, 1, .5, 0, 0], (2, 1)))

# A face-spanning plane perpendicular to +X has an analytic translated intersection.
volume = np.zeros(rays.shape[:-1], np.float32)
volume[0] = radial
sample = rays[0, 16:-16, 16:-16]
focal = 690.0
for position in ((0, 0, 0), (0, 0, 6), (0, 0, -12), (0, 3, 30)):
    index, distance, filled = reproject(volume, sample, position, focal)
    correct = (center_distance - position[0]) / sample[..., 0]
    error = float(np.max(abs(distance - correct)))
    print("plane", position, "unresolved", int(np.count_nonzero(~filled)),
          "maximum depth error m", round(error, 4))
    assert np.all(filled) and error < (30 if position != (0, 0, 0) else .001)
    assert np.all(index[0] == 0)

# A depth discontinuity cannot supply a newly revealed background from its hidden pixels.
foreground = volume.copy()
foreground[0, :, :side // 2] /= 3
_, distance, filled = reproject(foreground, sample, (0, 0, -100), focal)
unresolved = int(np.count_nonzero(~filled))
assert unresolved > 0 and np.all(distance[~filled] == 0)
print("depth discontinuity missing samples", unresolved)

start = np.array([0, 100, 0])
finish = start + [12, 0, 0]
anchors = start + np.array([[0, 0, 0], [8, 0, 0]])
assert np.allclose(anchor_overlap(start, finish, anchors, (6, 6)), (1 / 6, .5))
assert anchor_overlap(start, finish, anchors, (2, 4)) is None
assert anchor_overlap(start, finish, anchors, (4, 4)) == (1 / 3, 1 / 3)
for centers, radii in ((anchors, (6, 6)), (anchors + [[0, 4, 0], [0, 2, 0]], (7, 6))):
    overlap = anchor_overlap(start, finish, centers, radii)
    fractions = np.linspace(0, 1, 10001)
    weights = anchor_weight(fractions, overlap)
    path = start + fractions[:, None] * (finish - start)
    assert np.all(np.linalg.norm(path[weights < 1] - centers[0], axis=1) <= radii[0])
    assert np.all(np.linalg.norm(path[weights > 0] - centers[1], axis=1) <= radii[1])
    assert np.all(np.diff(weights) >= 0)
    assert np.max(np.diff(anchor_weight(np.arange(31) / 30, overlap))) <= .18
