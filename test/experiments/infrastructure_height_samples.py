"""Read coarse Mapterhorn heights directly from the existing source cache."""

import hashlib
import math
from pathlib import Path

import numpy as np
from PIL import Image

GIRTH_M = 40075016.68557849


def sample(sources, origin, xy, zoom=11):
    lat, lon = origin
    scale = math.cos(math.radians(lat))
    anchor = np.array([lon/360*GIRTH_M,
                       math.asinh(math.tan(math.radians(lat)))*GIRTH_M/(2*math.pi)])
    mercator = xy/scale + anchor
    world = (mercator + [GIRTH_M/2, -GIRTH_M/2]) * [1/GIRTH_M, -1/GIRTH_M]
    result = np.full(len(xy), np.nan)
    receipts = []
    for level in range(zoom, -1, -1):
        needed = np.flatnonzero(~np.isfinite(result))
        if not len(needed):
            break
        coordinates = world[needed] * (1<<level)
        tiles, indices = np.unique(np.floor(coordinates).astype(int), axis=0, return_inverse=True)
        for tile, cell in enumerate(tiles):
            x,y = cell
            subject = f'mapterhorn.terrarium\n1\n\nelevation\n{level}/{x}/{y}\nhttps://tiles.mapterhorn.com/{{z}}/{{x}}/{{y}}.webp'
            path = sources / hashlib.sha256(subject.encode()).hexdigest()
            if not path.is_file():
                continue
            rgb = np.asarray(Image.open(path).convert('RGB'), dtype=float)
            heights = rgb[...,0]*256 + rgb[...,1] + rgb[...,2]/256 - 32768
            selected = np.flatnonzero(indices == tile)
            p = coordinates[selected] - cell
            p *= [heights.shape[1]-1, heights.shape[0]-1]
            base = np.minimum(np.floor(p).astype(int), [heights.shape[1]-2,heights.shape[0]-2])
            fraction = p-base
            col,row = base.T
            u,v = fraction.T
            result[needed[selected]] = ((1-v)*((1-u)*heights[row,col] + u*heights[row,col+1])
                                        + v*((1-u)*heights[row+1,col] + u*heights[row+1,col+1]))
            receipts.append(dict(tile=[level,int(x),int(y)], path=str(path), bytes=path.stat().st_size,
                                 sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    if not np.all(np.isfinite(result)):
        raise ValueError('coarse DEM cache does not cover the height experiment')
    return result, receipts
