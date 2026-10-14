"""Check actual prism topology, clear doorway volumes and reachability reconstructed from the mesh."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from building_layout import AGENT_RADIUS_M, AREA_TOL_M2, DERIVED_GRID_M, polygons,room_portals
from building_mesh import mesh
from building_plan import AGENT_HEIGHT_M, build
from building_plan_cases import cases
from evidence import revision


def scope_checks():
    shared=shapely.LineString([(0,0),(0,3)])
    deep=shapely.box(.06,0,3.06,3)
    if room_portals([shapely.box(-.66,0,-.06,3),deep],[(0,1,[shared])]):
        raise AssertionError('Full-width doorway lacks the declared torso approach clearance')
    if len(room_portals([shapely.box(-.86,0,-.06,3),deep],[(0,1,[shared])]))!=1:
        raise AssertionError('A complete same-width doorway approach was rejected')
    rectangle = dict(id='scope', polygon=shapely.box(0, 0, 14, 10),
                     properties={'render_height': 9., 'building:levels': 2})
    supplied = build(rectangle)
    if supplied['storeys'] != 2 or supplied['storey_origin'] != 'supplied levels' or abs(supplied['ceiling_z_m']-4.32) > 1e-9:
        raise AssertionError('Supplied storey count was replaced by a heuristic')
    verify(supplied, mesh(supplied))
    solid = occupancy(mesh(supplied), 1, supplied['ceiling_z_m']-.02, supplied['ceiling_z_m']-.01)
    exterior_width = solid.intersection(shapely.LineString([[-.1, 2.], [.7, 2.]])).length
    inner_door = next(door for door in supplied['doors'] if door['kind'] == 'room-door')
    tangent = np.asarray(inner_door['tangent'])
    normal = np.array([tangent[1], -tangent[0]])
    centre = np.asarray(inner_door['centre'])+tangent*.9
    interior_width = solid.intersection(shapely.LineString([centre-normal*.3, centre+normal*.3])).length
    if abs(exterior_width-.24) > 1e-8 or abs(interior_width-.12) > 1e-8:
        raise AssertionError('Actual exterior/partition thickness differs from the stated dimensions')
    frontage = rectangle | dict(context=dict(evidence=[dict(usable_frontage=True, position_xy=[18., 5.])]))
    located = build(frontage)
    if located['entrance']['origin'] != 'context-frontage' or not np.allclose(located['entrance']['centre'], [14., 5.]):
        raise AssertionError('A usable provider frontage did not select the entrance')
    verify(located, mesh(located))
    rounded = build(rectangle | dict(properties={'render_height': 10.4}))
    if rounded['status'] != 'connected-first-storey' or rounded['ceiling_z_m']-rounded['floor_z_m'] < 2.30:
        raise AssertionError('Procedural storey rounding rejected a source with sufficient headroom')
    repeat = build(rectangle)
    if supplied['room_ids'] != repeat['room_ids'] or any(not a.equals_exact(b, 1e-9) for a, b in zip(supplied['rooms'], repeat['rooms'])):
        raise AssertionError('Camera-independent room identities changed on a repeat')
    angle = np.linspace(0., 2*np.pi, 300, endpoint=False)
    complex_sample = rectangle | dict(polygon=shapely.Polygon(np.c_[10*np.cos(angle), 10*np.sin(angle)]))
    limited = build(complex_sample)
    if limited['status'] != 'unplanned' or not any('256-vertex' in reason for reason in limited['omissions']):
        raise AssertionError('The bounded layout silently exceeded its source complexity limit')
    return dict(supplied_storeys=True, context_frontage=True, deterministic_room_ids=True, complexity_limit=True,
                actual_exterior_wall_m=exterior_width, actual_partition_wall_m=interior_width, derived_headroom=True,
                full_doorway_approach_clearance=True)


def closed_parts(product):
    triangles = product['vertices'][product['faces']]
    volume = 0.
    for part in np.unique(product['parts']):
        points = triangles[product['parts'] == part]
        flat, inverse = np.unique(np.round(points.reshape(-1, 3), 8), axis=0, return_inverse=True)
        faces = inverse.reshape(-1, 3)
        edge = np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]])
        _, counts = np.unique(np.sort(edge, axis=1), axis=0, return_counts=True)
        if not np.all(counts == 2):
            raise AssertionError(f'Prism {part} has open edges or T junctions')
        local = points-flat.mean(axis=0)
        signed = np.einsum('ij,ij->i', local[:, 0], np.cross(local[:, 1], local[:, 2])).sum()/6
        if signed <= 1e-9:
            raise AssertionError(f'Prism {part} has wrong winding or no volume')
        volume += signed
    return volume


def occupancy(product, material, low, high):
    triangles = product['vertices'][product['faces']]
    spans = {part: (points[:, :, 2].min(), points[:, :, 2].max())
             for part in np.unique(product['parts']) for points in [triangles[product['parts'] == part]]}
    normals = np.cross(triangles[:, 1]-triangles[:, 0], triangles[:, 2]-triangles[:, 0])
    selected = [shapely.Polygon(triangle[:, :2]) for triangle, normal, mat, part in
                zip(triangles, normals, product['materials'], product['parts'])
                if mat == material and normal[2] > 1e-10 and spans[part][0] < high and spans[part][1] > low]
    return shapely.union_all(selected)


def verify(plan, product):
    triangles = product['vertices'][product['faces']]
    if not np.isfinite(triangles).all():
        raise AssertionError('Mesh contains nonfinite vertices')
    volume = closed_parts(product)
    floor_z = plan['floor_z_m']
    floor = occupancy(product, 0, floor_z-.01, floor_z+.01).union(occupancy(product, 4, floor_z-.01, floor_z+.01))
    if floor.symmetric_difference(plan['footprint'].union(plan['entrance']['landing'])).area > 1e-6:
        raise AssertionError('Actual floor mesh does not preserve the footprint and landing')
    walls = occupancy(product, 1, floor_z+.01, floor_z+AGENT_HEIGHT_M)
    region = floor.difference(walls)
    snapped = shapely.set_precision(region, DERIVED_GRID_M)
    if region.symmetric_difference(snapped).area > AREA_TOL_M2:
        raise AssertionError('Mesh projection exceeds the declared derived precision tolerance')
    free = snapped.buffer(-AGENT_RADIUS_M, join_style='mitre')
    if free.symmetric_difference(plan['walkable']).area > 1e-6:
        raise AssertionError('Mesh-derived walking clearance differs from the plan')
    landing = plan['entrance']['landing'].representative_point()
    reached = next((part for part in polygons(free) if part.covers(landing)), None)
    clear_rooms = [room.buffer(-AGENT_RADIUS_M, join_style='mitre') for room in plan['rooms']]
    if reached is None or any(room.is_empty or room.difference(reached).area > 1e-6 for room in clear_rooms):
        raise AssertionError('Actual mesh blocks a room or the exterior door')
    for opening in plan['doors']+plan['windows']:
        bottom = floor_z+opening.get('sill_m', 0.)
        top = floor_z+opening.get('head_m', opening.get('height_m'))
        solid = occupancy(product, 1, bottom+.01, top-.01)
        if opening['polygon'].intersection(solid).area > 1e-6:
            raise AssertionError(f"Actual mesh seals {opening['id']}")
    if plan['ceiling_z_m']-floor_z < AGENT_HEIGHT_M:
        raise AssertionError('Insufficient headroom')
    return dict(source_id=plan['id'], rooms=len(plan['rooms']), doors=len(plan['doors']), windows=len(plan['windows']),
                triangles=len(product['faces']), solid_volume_m3=volume, mesh_reachability=True,
                scope='First storey and same-level exterior landing; upper storeys and native gameplay unproved')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/layout-validation.json'))
    args = parser.parse_args()
    checks, omissions = [], []
    for sample in cases():
        plan = build(sample)
        if plan['status'] == 'connected-first-storey':
            checks.append(verify(plan, mesh(plan)))
            if sample['label'] in ('thin-wing', 'narrow-neck', 'low-ceiling'):
                raise AssertionError('An unusable counterexample was silently accepted')
        else:
            omissions.append(dict(source_id=plan['id'], reasons=plan['omissions']))
            if sample['source_sha256'] or sample['label'] not in ('thin-wing', 'narrow-neck', 'low-ceiling'):
                raise AssertionError(f"Required constructive example failed: {plan['id']}")
    result = dict(**revision(), checks=checks, omissions=omissions, scope_checks=scope_checks(), status='pass',
                  agent_radius_m=AGENT_RADIUS_M, agent_height_m=AGENT_HEIGHT_M)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(status='pass', connected_cases=len(checks), rejected_counterexamples=len(omissions))), flush=True)


if __name__ == '__main__':
    main()
