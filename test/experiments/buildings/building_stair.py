"""Construct closed stair flights and landing geometry in a fixed metric core."""

import math
import time

import numpy as np
import shapely
from shapely.affinity import affine_transform

from building_layout import AGENT_RADIUS_M, DOOR_M, PARTITION_M, canonical_geometry, polygons, rectangle
from building_mesh import solid
from building_plan import FLOOR_M, walkable


FLIGHT_WIDTH_M = 1.10
LANDING_M = 1.10
TREAD_M = .28
MAX_RISER_M = .18
PLATE_M = .20


def tread_top(risers, rise, tread=TREAD_M):
    points = [(0., rise)]
    for index in range(1, risers):
        points.append((index*tread, index*rise))
        if index < risers-1:
            points.append((index*tread, (index+1)*rise))
    return np.asarray(points)


def profile(risers, rise, tread=TREAD_M):
    top = tread_top(risers, rise, tread)
    run = (risers-1)*tread
    switch = (PLATE_M-FLOOR_M)*tread/rise
    bottom = [[run,(risers-1)*rise-PLATE_M],[switch,-FLOOR_M],[0.,-FLOOR_M]]
    return shapely.Polygon(np.r_[top,bottom])


def flight(storage, descriptor, method='profile'):
    began = time.perf_counter()
    first = len(storage[0])
    count, rise = descriptor['risers'], descriptor['riser_m']
    if method == 'profile':
        solid(storage, profile(count, rise), -FLIGHT_WIDTH_M/2, FLIGHT_WIDTH_M/2, 5)
    elif method == 'tread-parts':
        section = profile(count,rise)
        for index in range(1, count):
            face = section.intersection(shapely.box((index-1)*TREAD_M,-FLOOR_M,index*TREAD_M,count*rise))
            solid(storage, face, -FLIGHT_WIDTH_M/2, FLIGHT_WIDTH_M/2, 5)
    else:
        raise ValueError('Unknown stair-flight construction method')
    place(storage, first, descriptor)
    return (time.perf_counter()-began)*1000


def place(storage, first, descriptor):
    origin, tangent = np.asarray(descriptor['origin']), np.asarray(descriptor['direction'])
    normal = np.array([tangent[1], -tangent[0]])
    local = np.asarray(storage[0][first:])
    xy = origin+local[:, :1]*tangent+local[:, 2:3]*normal
    transformed = np.c_[xy, local[:, 1]+descriptor['base_z_m']]
    storage[0][first:] = transformed.tolist()


def guard(storage, descriptor, layout):
    top = tread_top(descriptor['risers'],descriptor['riser_m'])
    face = shapely.Polygon(np.r_[top+[0.,1.],top[::-1]])
    tangent = np.asarray(descriptor['direction'])
    normal = np.array([tangent[1],-tangent[0]])
    centre = np.asarray(layout['origin'])+layout['size_m'][0]/2*np.asarray(layout['across'])
    side = np.sign(np.dot(centre-np.asarray(descriptor['origin']),normal))
    position = np.asarray(descriptor['origin'])+side*(FLIGHT_WIDTH_M/2-.02)*normal
    first = len(storage[0])
    solid(storage,face,-.02,.02,7)
    place(storage,first,descriptor|dict(origin=position.tolist()))


def landing_guard(layout, last=False):
    origin,along,across = (np.asarray(layout[key]) for key in ('origin','along','across'))
    width = layout['size_m'][0]
    centre = origin+width/2*across+layout['upper_front_m']*along
    middle = rectangle(centre,across,width-2*FLIGHT_WIDTH_M,.02,.02)
    if not last:
        return middle
    lower = (np.asarray(layout['flights'][0]['origin'])-origin)@across
    end = rectangle(origin+lower*across+LANDING_M*along,across,FLIGHT_WIDTH_M,.02,.02)
    difference = layout['upper_front_m']-LANDING_M
    if difference<=1e-8:
        return canonical_geometry(middle.union(end))
    side = FLIGHT_WIDTH_M if lower<width/2 else width-FLIGHT_WIDTH_M
    connector = rectangle(origin+side*across+(LANDING_M+difference/2)*along,along,difference+.04,.02,.02)
    return canonical_geometry(shapely.union_all([middle,end,connector]))


def geometry_frame(core, flip):
    points = np.asarray(core.minimum_rotated_rectangle.exterior.coords)[:4]
    vectors = np.roll(points, -1, axis=0)-points
    longest = int(np.argmax(np.linalg.norm(vectors, axis=1)))
    along = vectors[longest]/np.linalg.norm(vectors[longest])*(-1 if flip else 1)
    across = np.array([along[1], -along[0]])
    coordinates = np.c_[points@across, points@along]
    lo, hi = coordinates.min(axis=0), coordinates.max(axis=0)
    origin = lo[0]*across+lo[1]*along
    reverse = [across[0], along[0], across[1], along[1], origin[0], origin[1]]
    return origin, along, across, (hi-lo).tolist(), reverse


def core_layout(core, rise, floor_z, flip=False, mirror=False):
    origin, along, across, size, reverse = geometry_frame(core, flip)
    width, depth = size
    total = math.ceil(rise/MAX_RISER_M)
    lower, upper = math.ceil(total/2), total//2
    back = LANDING_M+(lower-1)*TREAD_M
    upper_front = LANDING_M+(lower-upper)*TREAD_M
    if width < 2*FLIGHT_WIDTH_M+.10 or depth < back+LANDING_M-1e-7 or upper < 2:
        return None
    left, right = FLIGHT_WIDTH_M/2, width-FLIGHT_WIDTH_M/2
    if mirror:
        left, right = right, left
    riser = rise/total
    first = dict(origin=(origin+left*across+LANDING_M*along).tolist(), direction=along.tolist(),
                 base_z_m=floor_z, risers=lower, riser_m=riser)
    second = dict(origin=(origin+right*across+back*along).tolist(), direction=(-along).tolist(),
                  base_z_m=floor_z+lower*riser, risers=upper, riser_m=riser)
    mid = affine_transform(shapely.box(0., back, width, depth), reverse)
    lower_opening = shapely.box(0.,LANDING_M,FLIGHT_WIDTH_M,depth) if left<width/2 else shapely.box(width-FLIGHT_WIDTH_M,LANDING_M,width,depth)
    opening = canonical_geometry(affine_transform(shapely.union_all([
        shapely.box(0.,upper_front,width,depth),lower_opening]),reverse))
    front = affine_transform(shapely.box(0., 0., width, LANDING_M), reverse)
    path = [[left, LANDING_M/2, floor_z]]
    path.extend([left, LANDING_M+(i-.5)*TREAD_M, floor_z+i*riser] for i in range(1, lower))
    path.extend([[left, back+LANDING_M/2, floor_z+lower*riser],
                 [right, back+LANDING_M/2, floor_z+lower*riser]])
    path.extend([right, back-(i-.5)*TREAD_M, floor_z+(lower+i)*riser] for i in range(1, upper))
    path.append([right, LANDING_M/2, floor_z+rise])
    points = np.asarray(path)
    xy = origin+points[:, :1]*across+points[:, 1:2]*along
    return dict(flights=[first, second], mid_landing=mid, mid_z_m=floor_z+lower*riser,
                opening=opening, front_landing=front, path=np.c_[xy,points[:, 2]],
                origin=origin.tolist(), along=along.tolist(), across=across.tolist(),
                size_m=size, rise_m=rise, risers=total, riser_m=riser,
                upper_front_m=upper_front, back_m=back, flip=flip, mirror=mirror)


def portals(plan, layout):
    origin, along, across = (np.asarray(layout[key]) for key in ('origin','along','across'))
    width, _ = layout['size_m']
    candidates = [(origin+x*across-PARTITION_M/2*along, across) for x in (.60, width/2, width-.60)]
    candidates.extend((origin+sign*width*across+LANDING_M/2*along+
                       (2*sign-1)*PARTITION_M/2*across, along) for sign in (0, 1))
    previous = next(door for door in plan['doors'] if set(door['rooms']) == {0, 1})
    for centre, tangent in sorted(candidates, key=lambda row: np.linalg.norm(row[0]-previous['centre'])):
        normal = np.array([tangent[1], -tangent[0]])
        owners = []
        for sign in (-1, 1):
            line = shapely.LineString([centre-tangent*DOOR_M/2+sign*normal*.46,
                                      centre+tangent*DOOR_M/2+sign*normal*.46])
            owner = next((index for index, room in enumerate(plan['rooms'][:2])
                          if room.buffer(1e-7).covers(line)), None)
            owners.append(owner)
        if set(owners) != {0, 1}:
            continue
        polygon = rectangle(centre, tangent, DOOR_M, .20, .20)
        yield previous | dict(centre=centre.tolist(), tangent=tangent.tolist(), polygon=polygon)


def connect(plan):
    rise = (plan['body_top_m']-plan['base_z_m'])/plan['storeys']
    for flip in (False, True):
        for mirror in (False, True):
            layout = core_layout(plan['rooms'][1], rise, plan['floor_z_m'], flip, mirror)
            if layout is None:
                continue
            for door in portals(plan,layout):
                doors = [door if set(old['rooms']) == {0, 1} else old for old in plan['doors']]
                result = plan | dict(doors=doors, stair=layout)
                result['walkable'] = walkable(result)
                entry = plan['entrance']['landing'].representative_point()
                reached = next((part for part in polygons(result['walkable']) if part.covers(entry)),None)
                clear = [room.buffer(-AGENT_RADIUS_M,join_style='mitre') for room in plan['rooms']]
                if reached is None or any(room.is_empty or room.difference(reached).area>1e-6 for room in clear):
                    continue
                return result, None
    return None, 'reserved core has no full-width doorway at a valid stair approach landing'
