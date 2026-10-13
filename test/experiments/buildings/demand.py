"""Conservative camera/movement demand and projected recess error before mesh emission."""

from dataclasses import dataclass
import math

import numpy as np


@dataclass(frozen=True)
class View:
    eye: np.ndarray
    target: np.ndarray
    width: int = 854
    height: int = 480
    fov_deg: float = 50.
    movement_m: float = 0.
    detail_error_px: float = .5

    def __post_init__(self):
        if (not np.isfinite(self.eye).all() or not np.isfinite(self.target).all()
                or np.asarray(self.eye).shape != (3,) or np.asarray(self.target).shape != (3,)
                or min(self.width, self.height) <= 0 or not 0 < self.fov_deg < 179
                or not math.isfinite(self.movement_m+self.detail_error_px)
                or self.movement_m < 0 or self.detail_error_px <= 0):
            raise ValueError('invalid view demand')

    @property
    def focal_px(self):
        return self.height/(2*math.tan(math.radians(self.fov_deg)*.5))

    @property
    def basis(self):
        forward = np.asarray(self.target)-self.eye
        length = np.linalg.norm(forward)
        if length <= 1e-10:
            raise ValueError('zero view direction')
        forward = forward/length
        up = np.array([0., 0., 1.]) if abs(forward[2]) < .99 else np.array([0., 1., 0.])
        right = np.cross(forward, up)
        right /= np.linalg.norm(right)
        return np.stack((right, np.cross(right, forward), forward))

    @property
    def planes(self):
        x, y = self.width/(2*self.focal_px), self.height/(2*self.focal_px)
        return np.array([[0, 0, 1, -.1], [1, 0, x, 0], [-1, 0, x, 0],
                         [0, 1, y, 0], [0, -1, y, 0]], dtype=float)


def clip(points, view):
    polygon = np.asarray(points, dtype=float)
    for plane in view.planes:
        if len(polygon) < 3:
            return np.empty((0, 3))
        distances = ((polygon-view.eye) @ view.basis.T) @ plane[:3]+plane[3]
        distances += view.movement_m*np.linalg.norm(plane[:3])
        result = []
        for i, a in enumerate(polygon):
            j = (i+1) % len(polygon)
            b, da, db = polygon[j], distances[i], distances[j]
            if da >= 0:
                result.append(a)
            if (da >= 0) != (db >= 0):
                result.append(a+(b-a)*(da/(da-db)))
        polygon = np.asarray(result, dtype=float).reshape(-1, 3)
    return polygon


def horizontal_demand(surface, view):
    height, upward = surface['height_m'], surface['upward']
    facing = (view.eye[2]-height)*(1 if upward else -1)+view.movement_m
    if facing <= 0:
        return None
    x0, y0, x1, y1 = surface['bounds']
    bounds = np.array([[x0, y0, height], [x1, y0, height], [x1, y1, height], [x0, y1, height]])
    clipped = clip(bounds, view)
    if len(clipped) < 3:
        return None
    return (clipped[:, 0].min(), clipped[:, 1].min(), clipped[:, 0].max(), clipped[:, 1].max())


def select(product, view):
    faces = []
    projection = math.sqrt(1+(view.width/(2*view.focal_px))**2+(view.height/(2*view.focal_px))**2)
    for index, facade in enumerate(product['facades']):
        a, b, repeat = facade['a'], facade['b'], facade['repeat']
        midpoint = np.r_[(a+b)*.5, (facade['top_m']+facade['bottom_m'])*.5]
        if (view.eye-midpoint) @ facade['normal']+view.movement_m <= 0:
            continue
        points = np.array([[*a, facade['bottom_m']], [*b, facade['bottom_m']],
                           [*b, facade['top_m']], [*a, facade['top_m']]])
        clipped = clip(points, view)
        if len(clipped) < 3:
            continue
        u = (clipped[:, :2]-a) @ ((b-a)/repeat.width_m)
        v = clipped[:, 2]-facade['bottom_m']
        region = np.array([u.min(), v.min(), u.max(), v.max()])
        region = np.clip(region, 0., [repeat.width_m, repeat.height_m]*2)
        if np.any(region[2:]-region[:2] <= 1e-8):
            continue
        nearest_z = ((clipped-view.eye) @ view.basis.T)[:, 2].min()-view.movement_m-repeat.depth_m
        error = view.focal_px*repeat.depth_m*projection/max(nearest_z, .1)
        faces.append(dict(edge=index, region=region, projected_recess_error_px=float(error),
                          detail=bool(repeat.count and error > view.detail_error_px)))
    caps = []
    for surface in product['surfaces']:
        bounds = horizontal_demand(surface, view)
        if bounds is not None:
            caps.append(dict(part=surface['part'], bounds=bounds))
    return dict(faces=faces, caps=caps, view=view)
