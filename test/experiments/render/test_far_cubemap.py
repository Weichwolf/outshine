"""Check cube directions, reverse-z reconstruction, translated planes and silhouettes."""
import numpy as np
from far_cubemap import FORWARD, RIGHT, DOWN, addresses, directions, from_device_depth, reproject, unit

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
