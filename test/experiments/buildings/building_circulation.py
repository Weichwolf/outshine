"""Reserve an architectural core and give each generic room independent common access."""

import time

import numpy as np
import shapely
from shapely.affinity import affine_transform
from shapely.ops import nearest_points

from building_core import placements
from building_corridor import CORRIDOR_ZONE_M, routes
from building_layout import (AGENT_RADIUS_M, DOOR_M, PARTITION_M, WALL_M, adjacency, contacts, frame,
                             MAX_ROOMS, bsp, canonical_geometry, polygons, rectangle, room_portals, segments, strips, transform_portal, world_floor)
from building_plan import aperture_fits, build as baseline, edges, entrance, owner, walkable, windows


MAX_SPACES = 24
MIN_PRIVATE_AREA_M2 = 5.


def private_pieces(part):
    radius = PARTITION_M/2+AGENT_RADIUS_M
    clear = part.buffer(-radius, join_style='mitre')
    if clear.geom_type == 'Polygon' and not clear.is_empty:
        return [part], []
    cores = sorted(polygons(clear), key=lambda core: (-core.area, core.bounds))
    rooms, occupied = [], shapely.Polygon()
    for core in cores:
        room = core.buffer(radius, join_style='mitre').intersection(part).difference(occupied)
        if room.geom_type == 'Polygon' and room.area >= MIN_PRIVATE_AREA_M2:
            rooms.append(room)
            occupied = occupied.union(room)
    return rooms, polygons(part.difference(occupied))


def route_inputs(interior, core, root, zones):
    inset = interior.difference(core).buffer(-CORRIDOR_ZONE_M/2, join_style='mitre')
    space = next((part for part in polygons(inset) if part.covers(shapely.Point(root))), None)
    if space is None:
        return None, None, None
    candidates = []
    for a, b in edges(core):
        tangent = (b-a)/np.linalg.norm(b-a)
        normal = np.array([tangent[1], -tangent[0]])
        centre = (a+b)/2
        target = centre+normal*(CORRIDOR_ZONE_M/2+.02)
        connector = rectangle(centre, tangent, CORRIDOR_ZONE_M, CORRIDOR_ZONE_M/2+.12, .05).difference(core)
        if space.covers(shapely.Point(target)) and connector.difference(interior).area <= 1e-8:
            candidates.append((np.linalg.norm(target-root), centre.tolist(), target.tolist(), connector))
    if not candidates:
        return None, None, None
    _, _, core_target, connector = min(candidates, key=lambda row: row[:2])
    anchors = [list(root), core_target]
    for zone in zones:
        if zone.covers(shapely.Point(root)):
            continue
        usable = polygons(zone.buffer(-CORRIDOR_ZONE_M/2-PARTITION_M/2, join_style='mitre').intersection(space))
        if not usable:
            continue
        point = list(nearest_points(shapely.union_all(usable), shapely.Point(root))[0].coords[0])
        if all(np.linalg.norm(np.asarray(point)-anchor) > 1e-7 for anchor in anchors):
            anchors.append(point)
    return space, anchors, connector


def zones_from_routes(interior, core, corridor, seeds):
    private, absorbed = [], []
    occupied = core.union(corridor)
    for seed in seeds:
        for part in polygons(seed.difference(occupied)):
            if part.area < 10.:
                absorbed.append(part)
            else:
                rooms, remainder = private_pieces(part)
                private.extend(rooms)
                absorbed.extend(remainder)
    corridor = shapely.union_all([corridor, *absorbed]).difference(core)
    for part in private:
        shared = segments(part.boundary.intersection(corridor.boundary))
        if not shared or max(line.length for line in shared) < DOOR_M+.40:
            return None, 'a usable private space lacks a full-width corridor contact'
    zones = [corridor, core, *sorted(private, key=lambda part: part.bounds)]
    if not private:
        return None, 'no usable private spaces remain after core/corridor reservation'
    if len(zones) > MAX_SPACES:
        return None, 'space count exceeds the 24-space layout budget'
    if corridor.geom_type != 'Polygon':
        return None, 'common corridor has disconnected components'
    if interior.difference(shapely.union_all(zones)).area > 1e-6:
        return None, 'derived zones do not cover the interior within the area tolerance'
    return zones, None


def finish(plan, interior, zones, core, corridor, reverse, entry):
    zones = [canonical_geometry(zone) for zone in zones]
    shared = contacts(zones)
    material = shapely.union_all([line for _, _, lines in shared for line in lines]).buffer(
        PARTITION_M/2, cap_style='flat', join_style='mitre')
    floors = [zone.difference(material) for zone in zones]
    if any(floor.geom_type != 'Polygon' or floor.is_empty for floor in floors):
        return None, 'partition material leaves a disconnected or empty space floor'
    portals = room_portals(floors, [contact for contact in shared if contact[0] == 0])
    if len(adjacency(floors, portals)) != len(floors):
        return None, 'a private space or core has no direct full-width corridor doorway'
    rooms = [world_floor(floor, reverse) for floor in floors]
    bounds = np.asarray(rooms[1].minimum_rotated_rectangle.exterior.coords)[:4]
    dimensions = sorted(np.linalg.norm(np.roll(bounds, -1, axis=0)-bounds, axis=1)[:2]) if len(bounds) == 4 else []
    expected = sorted(value-PARTITION_M for value in core['dimensions_m'])
    if len(dimensions) != 2 or not np.allclose(dimensions, expected, atol=1e-6, rtol=0):
        return None, 'partition material does not preserve the stated clear core dimensions'
    if any(room.is_empty for room in rooms) or owner(rooms, entry['centre'], entry['tangent']) != 0:
        return None, 'room transform failed or entrance does not enter the common corridor'
    if not aperture_fits(rooms[0], entry['centre'], entry['tangent'], DOOR_M):
        return None, 'entrance backing does not fit the corridor floor'
    doors = [transform_portal(door, reverse) for door in portals]
    for door in doors:
        door['kind'] = 'corridor-door'
    front = entry | dict(rooms=[0])
    result = plan | dict(rooms=rooms, doors=[*doors, front], entrance=front,
                         walls=plan['footprint'].difference(shapely.union_all(rooms)))
    result['windows'] = windows(plan['footprint'], rooms, result['doors'])
    free = walkable(result)
    reached = next((part for part in polygons(free) if part.covers(front['landing'].representative_point())), None)
    clear = [room.buffer(-AGENT_RADIUS_M, join_style='mitre') for room in rooms]
    if reached is None or any(room.is_empty or room.difference(reached).area > 1e-6 for room in clear):
        return None, 'actual swept-body clearance does not connect every space'
    result.update(walkable=free, room_kinds=['corridor', core['kind'], *(['generic-room']*(len(rooms)-2))],
                  room_ids=[f"{plan['id']}:floor-0:access-{index}" for index in range(len(rooms))],
                  core_envelope=affine_transform(core['polygon'], reverse),
                  corridor_zone=affine_transform(corridor, reverse))
    return result, None


def attempt(plan, interior, seeds, core, root, entry_local, reverse, entry, method):
    began = time.perf_counter()
    space, anchors, connector = route_inputs(interior, core['polygon'], root, seeds)
    result, routing, reason = None, {}, 'core has no connected common-access approach'
    if space is not None:
        paths, routing = routes(space, anchors, method)
        if paths is not None:
            network = shapely.union_all(paths).buffer(CORRIDOR_ZONE_M/2, cap_style='square', join_style='mitre')
            corridor = shapely.union_all([network, connector, entry_local]).intersection(interior).difference(core['polygon'])
            zones, reason = zones_from_routes(interior, core['polygon'], corridor, seeds)
            if zones is not None:
                corridor = zones[0]
                result, reason = finish(plan, interior, zones, core, corridor, reverse, entry)
                if result is not None:
                    result['corridor_routes'] = affine_transform(shapely.union_all(paths), reverse)
                    if result['corridor_routes'].distance(result['rooms'][0].boundary) < .75-1e-6:
                        result, reason = None, 'routed corridor does not retain the 1.50 metre target width'
        else:
            reason = routing.get('reason')
    score = result['rooms'][0].area/interior.area+.02/(len(result['rooms'])-1) if result else None
    report = dict(method=method, valid=result is not None, score=score, reason=reason,
                  rooms=len(result['rooms']) if result else 0, elapsed_ms=(time.perf_counter()-began)*1000,
                  candidates=routing.get('candidates', 0), grid_nodes=routing.get('grid_nodes', 0),
                  grid_edges=routing.get('grid_edges', 0), core_method=core['method'], core_bounds=list(core['polygon'].bounds))
    return result, report


def build(sample):
    plan = baseline(sample)
    plan.update(circulation_status='unplanned', core_alternatives=[], circulation_alternatives=[])
    if plan['status'] != 'connected-first-storey':
        return plan
    interior = plan['footprint'].buffer(-WALL_M, join_style='mitre').intersection(plan['footprint'])
    plan['partition_alternatives'] = plan['alternatives']
    entry = entrance(plan['footprint'], [interior], sample.get('context', {}))
    if entry is None:
        plan['omissions'].append('no common-access entrance candidate')
        return plan
    forward, reverse = frame(interior)
    local = affine_transform(interior, forward)
    seed_sets = [(name, procedure(local)[0]) for name, procedure in (('strips', strips), ('bsp', bsp))]
    centre = np.asarray(entry['centre'])
    tangent = np.asarray(entry['tangent'])
    normal = np.array([tangent[1], -tangent[0]])
    root = np.asarray(affine_transform(shapely.Point(centre-normal*(WALL_M+CORRIDOR_ZONE_M/2+.15)), forward).coords[0])
    plug = affine_transform(rectangle(centre, tangent, CORRIDOR_ZONE_M, .10, WALL_M+CORRIDOR_ZONE_M), forward)
    rise = (plan['source_top_m']-plan['base_z_m'])/plan['storeys']
    cores, plan['core_alternatives'] = placements(local, shapely.Point(root), plan['storeys'], rise)
    candidates, reports = [], []
    for seed_method, seeds in seed_sets:
        if len(seeds) > MAX_ROOMS:
            continue
        for index, core in enumerate(cores):
            for method in ('doglegs', 'orthogonal-grid'):
                candidate, report = attempt(plan, local, seeds, core, root, plug, reverse, entry, method)
                report.update(core_candidate=index, seed_method=seed_method)
                reports.append(report)
                if candidate is not None:
                    candidates.append((report['score'], index, method, candidate, report, core))
    plan['circulation_alternatives'] = reports
    if not candidates:
        plan['omissions'].append('bounded core/corridor alternatives unsuitable; retain the connected baseline rooms')
        return plan
    _, _, method, result, report, core = min(candidates, key=lambda item: item[:3])
    result.update(version=2, circulation_status='common-access', method=method,
                  scope='first-storey common access and reserved core; no constructed stairs or use programme',
                  core_alternatives=plan['core_alternatives'], circulation_alternatives=reports,
                  alternatives=reports, circulation=dict(method=method, private_rooms=len(result['rooms'])-2,
                  corridor_area_m2=result['rooms'][0].area,
                  corridor_fraction=result['rooms'][0].area/interior.area, core_kind=core['kind'],
                  seed_method=report['seed_method'],
                  core_dimensions_m=core['dimensions_m'], corridor_target_width_m=1.50,
                  grid_nodes=report['grid_nodes'], grid_edges=report['grid_edges']))
    result['omissions'] = [*plan['omissions'], 'core envelope is reserved; stairs, shaft equipment and use programme remain unconstructed']
    return result
