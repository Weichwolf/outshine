"""Triangulated rectangular terrain, increasing east and north."""

import numpy as np


def lattice(x, y, z):
    east, north = np.meshgrid(x, y)
    xy = np.column_stack((east.ravel(), north.ravel()))
    elevation = z(xy) if callable(z) else np.asarray(z).ravel()
    first = np.arange(len(xy)).reshape(len(y), len(x))[:-1, :-1].ravel()
    return np.column_stack((xy, elevation)), first[:, None] + [0, 1, len(x) + 1, len(x)]


def axes(vertices):
    size = np.searchsorted(vertices[:, 1], vertices[0, 1], side="right")
    return vertices[:size, 0], vertices[::size, 1]


def terrain_height(vertices, points):
    east, north = axes(vertices)
    x, y = np.asarray(points).T
    i = np.clip(np.searchsorted(east, x, side="right") - 1, 0, len(east) - 2)
    j = np.clip(np.searchsorted(north, y, side="right") - 1, 0, len(north) - 2)
    u = (x - east[i]) / (east[i + 1] - east[i])
    v = (y - north[j]) / (north[j + 1] - north[j])
    first = i + j * len(east)
    a, b, c, d = vertices[first[:, None] + [0, 1, len(east) + 1, len(east)], 2].T
    return a + np.where(u >= v, u * (b - a) + v * (c - b), v * (d - a) + u * (c - d))


def boundary_stations(vertices, radius):
    east, north = axes(vertices)
    xx, yy = [east, [-radius, radius]], [north, [-radius, radius]]
    for edge in (-radius, radius):
        for cells, cross, result in ((east, north, yy), (north, east, xx)):
            i = np.searchsorted(cells, edge, side="right") - 1
            if 0 <= i < len(cells) - 1:
                fraction = (edge - cells[i]) / (cells[i + 1] - cells[i])
                result.append(cross[:-1] + fraction * np.diff(cross))
    return np.unique(np.concatenate(xx)), np.unique(np.concatenate(yy))
