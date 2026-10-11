"""Measure clearance shortfalls only for exterior and near-boundary sample points."""

import numpy as np
import shapely


class SurfaceClearance:
    def __init__(self,surface):
        self.surface,self.boundary = surface,shapely.boundary(surface)
        parts = shapely.get_parts(self.boundary)
        coordinates,rings = shapely.get_coordinates(parts,return_index=True)
        valid = (rings[1:]==rings[:-1])&np.any(coordinates[1:]!=coordinates[:-1],axis=1)
        segments = shapely.linestrings(np.stack((coordinates[:-1][valid],coordinates[1:][valid]),axis=1))
        self.edges = shapely.STRtree(segments)
        self.samples,self.measured_samples = 0,0
        shapely.prepare(surface)

    def shortfall(self,points,minimum_distance):
        inside = shapely.covers(self.surface,points)
        interior = np.flatnonzero(inside)
        nearby = self.edges.query(points[interior],predicate='dwithin',distance=np.nextafter(minimum_distance,np.inf))
        active = ~inside
        active[interior[nearby[0]]] = True
        selected = np.flatnonzero(active)
        result = np.zeros(len(points))
        distance = shapely.distance(points[selected],self.boundary)
        result[selected] = np.maximum(minimum_distance-np.where(inside[selected],distance,-distance),0)
        self.samples += len(points)
        self.measured_samples += len(selected)
        return result

    def sampled_body_shortfall(self,corners,minimum_distance):
        delta = np.roll(corners,-1,axis=1)-corners
        samples = corners[:,:,None,:]+delta[:,:,None,:]*np.array([0.,.25,.5,.75])[None,None,:,None]
        edges = self.shortfall(shapely.points(samples.reshape(-1,2)),minimum_distance)
        edges = edges.reshape(*corners.shape[:2],4).max(axis=2)
        centre = self.shortfall(shapely.points(corners.mean(axis=1)),minimum_distance)
        return np.maximum(edges,centre[:,None])
