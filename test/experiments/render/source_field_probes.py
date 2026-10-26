"""Source-owned bounds and reconstruction probes; bounds never mean opacity."""

import numpy as np
import shapely
from shapely.affinity import affine_transform


def sample_packets(support, points, sampler, work):
    values = []
    packet = min(4096, 2**24 // len(support.domains))
    if packet < 1:
        raise ValueError("source domain packet exceeds sampler work budget")
    for begin in range(0, len(points), packet):
        result, current = sampler(support, points[begin : begin + packet])
        values.append(result)
        for key, value in current.items():
            work[key] = work.get(key, 0) + value
    return np.concatenate(values) if values else np.empty(0)


def boundary_probes(domain, plane, bottom, frame, inverse, origin, offset):
    xy = shapely.get_coordinates(domain)
    centre = np.array(domain.representative_point().coords)[0]
    roof_xy = np.concatenate((xy, centre[None]))
    roof = np.column_stack((roof_xy, roof_xy @ plane[:2] + plane[2]))
    floor = np.column_stack((centre[None], [bottom]))
    points = [roof, floor]
    normals = [
        np.tile(np.r_[-plane[:2], 1.0], (len(roof), 1)),
        np.array([[0.0, 0.0, -1.0]]),
    ]
    holes = []
    for part in shapely.get_parts(domain):
        for ring in [part.exterior, *part.interiors]:
            vertices = shapely.get_coordinates(ring)
            edges = np.diff(vertices, axis=0)
            good = np.linalg.norm(edges, axis=1) > 0
            edges = edges[good]
            middle = ((vertices[:-1] + vertices[1:]) / 2)[good]
            top = middle @ plane[:2] + plane[2]
            points.append(np.column_stack((middle, (top + bottom) / 2)))
            normals.append(
                np.column_stack((edges[:, 1], -edges[:, 0], np.zeros(len(edges))))
            )
        for ring in part.interiors:
            point = np.array(shapely.Polygon(ring).representative_point().coords)[0]
            holes.append([*point, (point @ plane[:2] + plane[2] + bottom) / 2])
    points = np.concatenate(points) @ frame + origin
    normal = np.concatenate(normals) @ inverse.T
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    interior = (
        np.r_[centre, (centre @ plane[:2] + plane[2] + bottom) / 2] @ frame + origin
    )
    batches = [points + offset * normal, points - offset * normal, interior[None]]
    if holes:
        batches.append(np.asarray(holes) @ frame + origin)
    return np.concatenate(batches)


def geometry_probes(support, *, offset=0.125):
    boxes, probes, footprints, floors = [], [], [], []
    for source in range(len(support.source_ids)):
        inverse = support.inverses[source]
        frame, origin = np.linalg.inv(inverse), support.origins[source]
        begin, end = support.offsets[source : source + 2]
        source_footprints, floor_heights = [], []
        for i in range(int(begin), int(end)):
            domain, plane, bottom = (
                support.domains[i],
                support.planes[i],
                support.bottoms[i],
            )
            xy = shapely.get_coordinates(domain)
            local = np.concatenate(
                (
                    np.column_stack((xy, xy @ plane[:2] + plane[2])),
                    np.column_stack((xy, np.full(len(xy), bottom))),
                )
            )
            world = local @ frame + origin
            boxes.append(np.stack((world.min(0), world.max(0))))
            probes.append(
                boundary_probes(domain, plane, bottom, frame, inverse, origin, offset)
            )
            shift = origin[:2] + bottom * frame[2, :2]
            source_footprints.append(
                affine_transform(
                    domain, [frame[0, 0], frame[1, 0], frame[0, 1], frame[1, 1], *shift]
                )
            )
            floor_heights.append(float(world[len(xy) :, 2].mean()))
        footprints.append(shapely.union_all(source_footprints))
        floors.append(float(np.median(floor_heights)))
    boxes = np.asarray(boxes)
    owners = np.repeat(support.source_ids, np.diff(support.offsets))
    if len(footprints) > 1:
        pairs = shapely.STRtree(footprints).query_nearest(
            footprints, exclusive=True, all_matches=False
        )
        for a, b in zip(*pairs, strict=True):
            line = shapely.shortest_line(footprints[a], footprints[b])
            if line.length > 0:
                middle = shapely.get_coordinates(line).mean(0)
                probes.append(np.array([[*middle, max(floors[a], floors[b]) + 1.0]]))
    union = shapely.union_all(footprints)
    for polygon in shapely.get_parts(union):
        for ring in polygon.interiors:
            middle = np.array(shapely.Polygon(ring).representative_point().coords)[0]
            probes.append(np.array([[*middle, np.median(floors) + 1.0]]))
    return boxes, owners, np.unique(np.concatenate(probes), axis=0)
