"""Complete native roof facets from measured bisectors and unchanged source rings."""

import numpy as np
import shapely

from .skeleton_plan import SkeletonBudgetExceeded


def graph_data(local,rings,node_limit):
    from pysfcgal.geometry import Geometry
    source=Geometry.from_wkt(shapely.to_wkt(local,rounding_precision=-1))
    output=source.straight_skeleton_distance_in_m()
    if output is None:raise ValueError('SFCGAL returned no measured skeleton')
    data=output.to_dict()
    if data['type']!='MultiLineString':raise ValueError('SFCGAL returned a different skeleton geometry')
    if not data['coordinates'] or len(data['coordinates'])>3*node_limit:
        raise SkeletonBudgetExceeded('Native graph segment capacity exceeded or empty')
    original=np.concatenate(rings);tolerance=max(1.,abs(original).max())*64*np.finfo(float).eps
    segments=[];measures={};maximum_snap=0.
    for line in data['coordinates']:
        if len(line)!=2:raise ValueError('Native skeleton segment needs two endpoints')
        points=[]
        for raw in line:
            if len(raw)!=4 or raw[2] is not None:raise ValueError('Native graph needs XYM coordinates')
            point=np.asarray(raw[:2],dtype=float);height=float(raw[3])
            if not np.isfinite(point).all() or not np.isfinite(height) or height<0:
                raise ValueError('Native skeleton has invalid coordinates or wavefront time')
            distances=np.linalg.norm(original-point,axis=1);matched=np.flatnonzero(distances<=tolerance)
            if len(matched)>1:raise ValueError('Native roundtrip cannot distinguish original vertices')
            if len(matched):
                maximum_snap=max(maximum_snap,float(distances[matched[0]]));point=original[matched[0]];height=0.
            key=tuple(point)
            if key in measures and abs(measures[key]-height)>1e-7:raise ValueError('Native graph node times disagree')
            measures[key]=height;points.append(point)
        segments.append(points)
    if len(measures)>node_limit:raise SkeletonBudgetExceeded('Native graph node capacity exceeded')
    network=shapely.union_all([*shapely.linestrings(segments),local.boundary])
    cells,cuts,dangles,invalid=shapely.polygonize_full(shapely.get_parts(network))
    if any(not g.is_empty for g in (cuts,dangles,invalid)):
        raise ValueError('Native skeleton graph has unused or invalid linework')
    cells=shapely.get_parts(cells);cells=cells[shapely.covers(local,shapely.point_on_surface(cells))]
    if len(cells)!=len(original):raise ValueError('Native graph has an incomplete eave partition')
    nodes=[];heights=[];faces=[];lookup={}
    for polygon in cells:
        if polygon.interiors:raise ValueError('Native facet holes require a topology contract')
        face=[]
        for point in np.asarray(polygon.exterior.coords)[:-1]:
            key=tuple(point)
            if key not in measures:raise ValueError('Native graph noding introduced an unmeasured vertex')
            if key not in lookup:
                if len(nodes)>=node_limit:raise SkeletonBudgetExceeded('Native graph cell-node capacity exceeded')
                lookup[key]=len(nodes);nodes.append(point);heights.append(measures[key])
            face.append(lookup[key])
        faces.append(tuple(face))
    return (np.asarray(nodes),np.asarray(heights),tuple(faces),None),dict(
        segments=len(segments),source_endpoint_tolerance_m=tolerance,maximum_source_endpoint_snap_m=maximum_snap)

