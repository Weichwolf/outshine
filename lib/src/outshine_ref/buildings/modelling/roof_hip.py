"""Hip roofs clip source interpolation domains before lifting conformal native corners."""

from dataclasses import dataclass, replace
import time

import numpy as np
import shapely

from .extrude import aligned_scope
from .facet_overlay import overlay
from .faces import Face, ROOF_TAGS, plane
from .hip_surface import prepare_even
from .hip_inset import prepare_hip_inset
from .hip_sides import close_rim
from .metadata import inherit
from .roof_mesh import RoofMesh
from .roof_ridge import source_cut
from .roof_shed import channel_faces
from .scope import Frame, scalar
from .shape import Shape
from .skeleton_plan import SkeletonBudgetExceeded, SkeletonLimits, prepare_hip


@dataclass(frozen=True)
class HipInfo:
    plans: tuple
    source_faces: tuple
    overlay_cells: tuple
    overlay_precision_m: tuple
    authored_normals_rebuilt: bool
    surfaces: tuple
    maximum_shared_height_adjustment_m: tuple
    insets: tuple=()
    collapsed_source_faces: tuple=()


def attributes(source, projected, cut):
    triangles = projected[source.triangles]
    first, last = triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0]
    inverse = np.linalg.inv(np.stack((first, last), axis=2))
    delta = cut.corners-triangles[cut.source_triangles, 0][:, None]
    pair = np.einsum('nij,nkj->nki', inverse[cut.source_triangles], delta)
    weights = np.concatenate((1.-pair.sum(axis=2, keepdims=True), pair), axis=2)
    if weights.min() < -1e-7 or not np.isfinite(weights).all():
        raise ValueError('Roof overlay corners left their native source triangle')
    properties = np.einsum('nki,nij->nkj', weights, source.properties[source.triangles[cut.source_triangles]])
    return properties.reshape(-1, properties.shape[-1])


def canonical(builder, points, properties, owners, plan, factor, precision):
    roots, upper, known = [], [], {}; adjustment=0.
    boundary = shapely.set_precision(plan.footprint, precision).boundary
    for xy, row, owner in zip(points, properties, owners, strict=True):
        key = tuple(xy)
        facet = plan.facets[owner]
        distance = float(xy@facet.gradient+facet.intercept_m)
        uncertainty = (np.sqrt(2)*precision*np.linalg.norm(facet.gradient)
                       +64*np.finfo(float).eps*(abs(xy*np.asarray(facet.gradient)).sum()+abs(facet.intercept_m)))
        if boundary.distance(shapely.Point(xy)) <= precision/2:
            distance = 0.
        if distance < -max(1e-7,uncertainty):
            raise ValueError('Hip height left its source half-plane')
        distance = max(distance, 0.)
        if key in known:
            root, position, height, prior_uncertainty = known[key]
            if abs(height-distance) > max(1e-7,uncertainty+prior_uncertainty) or np.linalg.norm(position-row[:3]) > 1e-8:
                raise ValueError('Adjacent hip cells disagree at a shared geometric vertex')
            adjustment=max(adjustment,abs(height-distance)*factor)
            row[:3] = position; distance = height
        else:
            root = builder.root(('base', *key)); position = row[:3].copy()
            known[key] = root, position, distance, uncertainty
        roots.append(root)
        upper.append(root if distance == 0 else builder.root(('top', root)))
    heights = np.array([known[tuple(xy)][2] for xy in points])*factor
    return np.asarray(roots), np.asarray(upper), heights, adjustment


def hip_face(shape, mesh, number, value, mode, channels, limits, max_cells, even=False, overhang=0.):
    face = shape.geometry.polygons[number]
    edges_count = sum(len(ring) for ring in (face.outer, *face.holes))
    if np.count_nonzero(mesh['parts'] == number)*edges_count > max_cells:
        raise SkeletonBudgetExceeded('Conservative native-triangle/facet candidate capacity exceeded')
    normal, polygon, _, face = plane(mesh['vertices'], face)
    origin = mesh['vertices'][face.outer[0]]
    vertices = mesh['vertices'][list(face.outer)]
    edges = np.roll(vertices, -1, axis=0)-vertices
    tangent = edges[np.argmax(np.linalg.norm(edges, axis=1))]
    tangent -= normal*np.dot(tangent, normal); tangent /= np.linalg.norm(tangent)
    basis = np.array([tangent, np.cross(normal, tangent)])
    plan = prepare_hip(polygon, limits=limits)
    surface = prepare_even(plan,mode) if even else None
    field = plan if surface is None else surface
    factor = field.factor(value, mode)
    inset = prepare_hip_inset(field,value,mode,overhang) if overhang<0 else None
    uv = channel_faces(face, mesh)
    face = replace(face, tags=face.tags-ROOF_TAGS.keys(), uvs=uv[0], uv_sets=tuple((c, v) for c, v in uv.items() if c))
    source = source_cut(shape, mesh, number, face, normal, channels)
    projected = (source.properties[:, :3]-origin)@basis.T-np.asarray(plan.origin)
    cut = overlay(projected[source.triangles], field, max_cells, None if inset is None else inset.domain)
    if not len(cut.corners):
        return None,(),normal,plan,0,cut.precision_m,surface,0.,inset
    properties = attributes(source, projected, cut)
    face = Face((0,1,2),material=face.material,tags=face.tags,group=face.group)
    points = cut.corners.reshape(-1, 2); owners = np.repeat(cut.facets, 3)
    builder = RoofMesh(channels,normal if even else None)
    roots, upper, heights, adjustment = canonical(builder, points, properties, owners, field, factor, cut.precision_m)
    order = np.arange(len(properties)).reshape(-1, 3)[:, ::-1].ravel()
    builder.append(properties[order], roots[order], 'bottom', replace(face, tags=face.tags|{'roof.bottom'}))
    if inset is not None:
        close_rim(builder,normal,dict(zip(roots,heights,strict=True)),face)
    top = properties.copy(); top[:, :3] += heights[:, None]*normal
    if surface is not None:owners=np.asarray([patch.facet for patch in field.facets])[owners]
    for index in range(len(plan.facets)):
        selected = owners == index
        builder.append(top[selected], upper[selected], ('top', index), replace(face, tags=face.tags|{'roof.top'}))
    native, faces = builder.finish()
    return native, faces, normal, plan, cut.cells, cut.precision_m, surface, adjustment,inset


def roof_hip(shape, value, mode='byAngle', overhang=0., even=False, limits=None, max_cells=65536):
    value, overhang = scalar(value), scalar(overhang)
    if type(even) is not bool:
        raise ValueError('Hip even option must be boolean')
    if type(max_cells) is not int or max_cells <= 0:
        raise ValueError('Hip overlay capacity must be a positive integer')
    if overhang > 0:
        raise NotImplementedError('Positive hip overhang and trim planes require separate verified contracts')
    if mode not in ('byAngle', 'byHeight') or value <= 0 or mode == 'byAngle' and not 1e-10 <= value < 90:
        raise ValueError('Hip roofs require a positive height or angle in [1e-10,90) degrees')
    limits = SkeletonLimits() if limits is None else limits
    if not isinstance(limits, SkeletonLimits):
        raise ValueError('Hip limits require SkeletonLimits')
    geometry = shape.geometry
    if geometry.edges or geometry.points or not geometry.polygons:
        raise NotImplementedError('Hip roofs require logical polygon faces without wire/point primitives')
    allowed = {'faces','parts','materials','groups','face_tag_mask','normals','uv'}|{'uv'+str(i) for i in range(1,10)}
    if set(geometry.fields)-allowed:
        raise NotImplementedError('Hip fields other than normals and UV0–9 remain pending')
    began = time.perf_counter()
    working = replace(shape, scope=replace(shape.scope, object_frame=Frame()))
    mesh = working.emit(); mesh.setdefault('uv', np.zeros((len(mesh['vertices']), 2)))
    channels = (0,)+tuple(sorted(int(k[2:]) for k in mesh if k.startswith('uv') and k[2:].isdigit()))
    products, faces, owners, plans, cells, precisions, normals, surfaces, adjustments, insets, collapsed = [], [], [], [], [], [], [], [], [], [], []
    vertex_offset = face_offset = 0
    for number in range(len(geometry.polygons)):
        native, polygons, normal, plan, count, precision, surface, adjustment, inset = hip_face(working, mesh, number, value, mode, channels, limits, max_cells, even,overhang)
        plans.append(plan);cells.append(count);precisions.append(precision);normals.append(normal);surfaces.append(surface)
        adjustments.append(adjustment);insets.append(inset)
        if time.perf_counter()-began > limits.seconds:
            raise SkeletonBudgetExceeded('Hip shape exceeded its cooperative time budget')
        if native is None:
            collapsed.append(number);continue
        native['faces'] += vertex_offset; native['parts'] += face_offset; products.append(native)
        faces.extend(replace(f, outer=tuple(i+vertex_offset for i in f.outer),
                             holes=tuple(tuple(i+vertex_offset for i in ring) for ring in f.holes)) for f in polygons)
        owners.extend([number]*len(polygons))
        vertex_offset += len(native['vertices']); face_offset += len(polygons)
    native = {key:np.concatenate([p[key] for p in products]) for key in products[0]} if products else dict(vertices=np.empty((0,3)),faces=np.empty((0,3),dtype=np.int32))
    if not products:
        native.update({'uv'+str(c) if c else 'uv':np.empty((0,2)) for c in channels})
        native.update(parts=np.empty(0,dtype=np.int32),materials=np.empty(0,dtype=np.uint16),
                      groups=np.empty(0,dtype=np.uint32),face_tag_mask=np.empty(0,dtype=np.uint8))
    bounds = native['vertices'] if products else mesh['vertices'][[geometry.polygons[0].outer[0]]]
    scope = aligned_scope(bounds, normals[0], working.scope, False)
    result = inherit(Shape.from_mesh(native, scope, faces), shape)
    if 'uv' not in geometry.fields:
        result = result.delete_uv(0)
    scope = replace(result.scope, object_frame=shape.scope.object_frame)
    return replace(result, scope=scope, comp_info=shape.comp_info,
                   roof_info=HipInfo(tuple(plans), tuple(owners), tuple(cells), tuple(precisions), 'normals' in geometry.fields,tuple(surfaces),tuple(adjustments),tuple(insets),tuple(collapsed)))
