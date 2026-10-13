"""Fixed facet triangulations retain folds when equalizing connected convex ridges."""

from dataclasses import dataclass

import numpy as np
import shapely

from .skeleton_plan import immutable_array


@dataclass(frozen=True, slots=True)
class SurfacePatch:
    polygon: object
    gradient: tuple
    intercept_m: float
    facet: int
    node_indices: tuple


@dataclass(frozen=True, slots=True)
class HipSurface:
    plan: object
    distances_m: object
    facets: tuple
    ridge_groups: tuple

    @property
    def origin(self):return self.plan.origin

    @property
    def footprint(self):return self.plan.footprint

    @property
    def maximum_refinement_m(self):return self.plan.maximum_refinement_m


def ridge_groups(plan):
    edges={}; parents=np.arange(len(plan.positions_m))
    for number,facet in enumerate(plan.facets):
        nodes=facet.node_indices
        for a,b in zip(nodes,nodes[1:]+nodes[:1]):
            edges.setdefault(tuple(sorted((a,b))),[]).append((number,a,b))
    def root(node):
        while parents[node]!=node:
            parents[node]=parents[parents[node]];node=int(parents[node])
        return node
    for owners in edges.values():
        if len(owners)!=2:continue
        first,a,b=owners[0];second=owners[1][0]
        if plan.distances_m[a]<=0 or plan.distances_m[b]<=0:continue
        tangent=plan.positions_m[b]-plan.positions_m[a]
        length=np.linalg.norm(tangent)
        if length==0:
            ra,rb=root(a),root(b);parents[max(ra,rb)]=min(ra,rb);continue
        inward=np.array([-tangent[1],tangent[0]])/length
        if not plan.facets[first].polygon.exterior.is_ccw:inward=-inward
        change=(np.asarray(plan.facets[first].gradient)-plan.facets[second].gradient)@inward
        if change < -64*np.finfo(float).eps:
            ra,rb=root(a),root(b);parents[max(ra,rb)]=min(ra,rb)
    groups={}
    for node in np.flatnonzero(plan.distances_m>0):groups.setdefault(root(int(node)),[]).append(int(node))
    return tuple(tuple(group) for group in groups.values())


def patches_for(plan,heights,owner,lookup):
    facet=plan.facets[owner];ids=list(facet.node_indices)
    prior=plan.distances_m[ids];values=heights[ids];positive=prior>0
    ratio=values[positive]/prior[positive]
    if len(ratio) and np.allclose(ratio,ratio[0],rtol=1e-12,atol=0.):
        yield SurfacePatch(facet.polygon,tuple(np.asarray(facet.gradient)*ratio[0]),
                           facet.intercept_m*ratio[0],owner,facet.node_indices)
        return
    for triangle in shapely.get_parts(shapely.constrained_delaunay_triangles(facet.polygon)):
        points=np.asarray(triangle.exterior.coords)[:3]
        nodes=tuple(lookup[tuple(xy)] for xy in points);values=heights[list(nodes)]
        gradient=np.linalg.solve(points[1:]-points[0],values[1:]-values[0])
        yield SurfacePatch(triangle,tuple(gradient),float(values[0]-points[0]@gradient),owner,nodes)


def prepare_even(plan):
    groups=ridge_groups(plan);heights=plan.distances_m.copy()
    for group in groups:
        _,unique=np.unique(plan.positions_m[list(group)],axis=0,return_index=True)
        heights[list(group)]=plan.distances_m[np.asarray(group)[unique]].mean()
    lookup={tuple(xy):i for i,xy in enumerate(plan.positions_m)}
    patches=[patch for owner in range(len(plan.facets)) for patch in patches_for(plan,heights,owner,lookup)]
    return HipSurface(plan,immutable_array(heights),tuple(patches),groups)
