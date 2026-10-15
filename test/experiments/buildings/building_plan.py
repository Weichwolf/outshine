"""Construct the first storey from one fixed envelope and actual room/opening polygons."""

import itertools

import numpy as np
import shapely
from building_layout import (
    AGENT_RADIUS_M,
    DOOR_M,
    PARTITION_M,
    WALL_M,
    canonical_geometry,
    partitions,
    polygons,
    rectangle,
)
from outshine_ref.buildings.interiors import geometry as interior_geometry
from provider_contract import attributes, number
from recipes import choose

edges = interior_geometry.edges


FLOOR_M = .16
CEILING_M = .18
AGENT_HEIGHT_M = 1.90


def owner(rooms, centre, tangent):
    normal = np.array([tangent[1], -tangent[0]])
    point = shapely.Point(centre-normal*(WALL_M+PARTITION_M/2+.16))
    return next((index for index, room in enumerate(rooms) if room.covers(point)), None)


def aperture_fits(room, centre, tangent, width):
    centre, tangent = np.asarray(centre), np.asarray(tangent)
    normal = np.array([tangent[1], -tangent[0]])
    middle = centre-normal*(WALL_M+PARTITION_M/2+.16)
    return room.covers(shapely.LineString([middle-tangent*width/2, middle+tangent*width/2]))


def entrance(polygon, rooms, context):
    candidates = []
    frontage = [shapely.Point(row['position_xy']) for row in context.get('evidence', []) if row.get('usable_frontage')]
    for index, (a, b) in enumerate(edges(polygon, holes=False)):
        width = np.linalg.norm(b-a)
        if width < DOOR_M+.8:
            continue
        tangent = (b-a)/width
        normal = np.array([tangent[1], -tangent[0]])
        street = [point for point in frontage if (np.array(point.coords[0])-(a+b)/2)@normal > 0
                  and shapely.LineString([a, b]).distance(point) <= 12.]
        for fraction in (.5, .25, .75):
            if min(fraction, 1-fraction)*width < DOOR_M/2+.30:
                continue
            centre = a+(b-a)*fraction
            room = owner(rooms, centre, tangent)
            if room is None or not aperture_fits(rooms[room], centre, tangent, DOOR_M):
                continue
            opening = rectangle(centre, tangent, DOOR_M, .12, WALL_M+PARTITION_M/2+.20)
            landing = rectangle(centre, tangent, 1.80, 1.20, .06)
            if landing.intersection(polygon).area > .12:
                continue
            rank = (not bool(street), min((shapely.Point(centre).distance(p) for p in street), default=0.),
                    -width, index, fraction)
            door = {'id': 'entrance', 'rooms': [room], 'polygon': opening, 'centre': centre.tolist(),
                        'tangent': tangent.tolist(), 'height_m': 2.15, 'kind': 'entrance', 'landing': landing,
                        'origin': 'context-frontage' if street else 'procedural longest usable facade'}
            candidates.append((rank, door))
    return min(candidates, key=lambda row: row[0])[1] if candidates else None


def windows(polygon, rooms, doors):
    result = []
    for edge, (a, b) in enumerate(edges(polygon)):
        length = np.linalg.norm(b-a)
        count = min(8, int(length/3.4))
        tangent = (b-a)/length
        for column in range(count):
            centre = a+(b-a)*(column+.5)/count
            room = owner(rooms, centre, tangent)
            if room is None or not aperture_fits(rooms[room], centre, tangent, 1.20):
                continue
            opening = rectangle(centre, tangent, 1.20, .10, WALL_M+PARTITION_M/2+.20)
            if any(opening.distance(door['polygon']) < .25 for door in doors):
                continue
            result.append({'id': f'window-{edge}-{column}', 'rooms': [room], 'polygon': opening,
                               'centre': centre.tolist(), 'tangent': tangent.tolist(), 'sill_m': .85, 'head_m': 2.05, 'kind': 'window'})
            if len(result) == 64:
                return result
    return result


def wall_at(plan, height):
    active = [opening['polygon'] for opening in plan['doors'] if height < plan['floor_z_m']+opening['height_m']]
    active += [opening['polygon'] for opening in plan['windows']
               if plan['floor_z_m']+opening['sill_m'] < height < plan['floor_z_m']+opening['head_m']]
    return plan['walls'].difference(shapely.union_all(active)) if active else plan['walls']


def wall_bands(plan):
    limits = {plan['floor_z_m'], plan['ceiling_z_m']}
    limits.update(plan['floor_z_m']+door['height_m'] for door in plan['doors'])
    limits.update(plan['floor_z_m']+window[key] for window in plan['windows'] for key in ('sill_m', 'head_m'))
    levels = sorted(value for value in limits if plan['floor_z_m'] <= value <= plan['ceiling_z_m'])
    result = []
    for low, high in itertools.pairwise(levels):
        walls = wall_at(plan, (low+high)/2)
        result.append((canonical_geometry(walls), low, high))
    return result


def walkable(plan):
    floor = plan['footprint'].union(plan['entrance']['landing'])
    obstacles = shapely.union_all([polygon for polygon, low, high in wall_bands(plan)
                                  if low < plan['floor_z_m']+AGENT_HEIGHT_M and high > plan['floor_z_m']])
    return canonical_geometry(floor.difference(obstacles)).buffer(-AGENT_RADIUS_M, join_style='mitre')


def build(sample, body_top_m=None):
    footprint = shapely.orient_polygons(sample['polygon'])
    data = attributes(sample['properties'])
    top = data['buildingHeight'] if body_top_m is None else number(body_top_m)
    if not data['minHeight'] < top <= data['buildingHeight']:
        raise ValueError('reserved body interval leaves the delivered building envelope')
    body_height = top-data['minHeight']
    recipe = choose(sample['properties'], sample.get('context', {}), footprint, data['buildingHeight'], data['minHeight'])
    count = max(1, round(body_height/recipe['parameters']['floor_m']))
    count = min(count, max(1, int(body_height/(2.30+FLOOR_M+CEILING_M))))
    supplied_levels = next((sample['properties'][key] for key in ('building:levels', 'levels') if key in sample['properties']), None)
    if supplied_levels is not None:
        levels = number(supplied_levels)
        if not levels.is_integer() or levels < 1:
            raise ValueError('invalid supplied storey count')
        count = int(levels)
    storey = body_height/count
    plan = {'id': sample['id'], 'footprint': footprint, 'source_properties': sample['properties'].copy(),
                'context': sample.get('context', {}).copy(), 'source_attributes': data.copy(),
                'version': 1, 'scope': 'procedural first-storey rooms without an architectural use programme',
                'source_sha256': sample.get('source_sha256'), 'recipe': recipe, 'status': 'unplanned', 'omissions': [],
                'base_z_m': data['minHeight'], 'source_top_m': data['buildingHeight'], 'body_top_m': top, 'storeys': count,
                'body_top_origin': 'source envelope' if body_top_m is None else 'procedural roof reservation',
                'storey_origin': 'supplied levels' if supplied_levels is not None else 'procedural height/use estimate',
                'floor_z_m': data['minHeight']+FLOOR_M, 'ceiling_z_m': data['minHeight']+storey-CEILING_M,
                'wall_m': WALL_M, 'partition_m': PARTITION_M, 'rooms': [], 'doors': [], 'windows': [], 'alternatives': []}
    if sum(len(ring.coords)-1 for ring in [footprint.exterior]+list(footprint.interiors)) > 256:
        plan['omissions'].append('footprint exceeds the 256-vertex scope of the bounded layout experiment')
        return plan
    interior = footprint.buffer(-WALL_M, join_style='mitre').intersection(footprint)
    if plan['ceiling_z_m']-plan['floor_z_m'] < 2.30:
        plan['omissions'].append('insufficient first-storey headroom')
        return plan
    clear = interior.buffer(-AGENT_RADIUS_M, join_style='mitre')
    if interior.geom_type != 'Polygon' or clear.geom_type != 'Polygon' or clear.is_empty:
        plan['omissions'].append('wall offset leaves no connected walkable interior')
        return plan
    layout, plan['alternatives'] = partitions(interior)
    plan['method'], plan['rooms'], plan['doors'] = layout['method'], layout['rooms'], layout['portals']
    plan['room_ids'] = [f"{plan['id']}:floor-0:room-{index}" for index in range(len(plan['rooms']))]
    if layout.get('reason'):
        plan['omissions'].append(layout['reason'])
    entry = entrance(footprint, plan['rooms'], sample.get('context', {}))
    if entry is None:
        plan['omissions'].append('no facade supports a connected full-width entrance')
        return plan
    plan['entrance'] = entry
    plan['doors'].append(entry)
    plan['walls'] = footprint.difference(shapely.union_all(plan['rooms']))
    plan['windows'] = windows(footprint, plan['rooms'], plan['doors'])
    free = walkable(plan)
    components = polygons(free)
    landing = entry['landing'].representative_point()
    connected = next((part for part in components if part.covers(landing)), None)
    clear_rooms = [room.buffer(-AGENT_RADIUS_M, join_style='mitre') for room in plan['rooms']]
    if connected is None or any(room.is_empty or room.difference(connected).area > 1e-6 for room in clear_rooms):
        plan['omissions'].append('actual swept-body clearance does not connect every room to the landing')
        return plan
    plan['status'] = 'connected-first-storey'
    plan['walkable'] = free
    if count > 1:
        plan['omissions'].append(f'{count-1} upper storeys and vertical circulation remain unplanned')
    if data['minHeight'] != 0:
        plan['omissions'].append('elevated or underground landing lacks a connection to the terrain datum')
    plan['omissions'].append('street/terrain contact and native collision remain unproved')
    return plan
