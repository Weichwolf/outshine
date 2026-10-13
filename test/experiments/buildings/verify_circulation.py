"""Verify exclusive common access, fixed core containment and actual mesh doorway clearance."""

import argparse
import json
from pathlib import Path

import numpy as np
import shapely

from building_core import placements
from building_circulation import MAX_SPACES, build
from building_corridor import MAX_GRID_AXIS
from building_layout import AGENT_RADIUS_M, AREA_TOL_M2, DERIVED_GRID_M, PARTITION_M, WALL_M, polygons
from building_mesh import mesh
from building_plan import AGENT_HEIGHT_M
from building_plan_cases import cases
from evidence import revision
from verify_layout import occupancy, verify as verify_layout


def verify(plan, product):
    result = verify_layout(plan, product)
    if plan.get('circulation_status') != 'common-access':
        raise AssertionError('No independently connected common-access plan')
    if len(plan['rooms']) > MAX_SPACES or plan['room_kinds'][0] != 'corridor':
        raise AssertionError('Invalid common-access space contract')
    linked = set()
    for door in plan['doors']:
        if door['kind'] == 'entrance':
            if door['rooms'] != [0]:
                raise AssertionError('Entrance does not enter common access')
            continue
        if len(door['rooms']) != 2 or 0 not in door['rooms']:
            raise AssertionError('A doorway requires traversing a private space')
        tangent, centre = np.asarray(door['tangent']), np.asarray(door['centre'])
        normal = np.array([tangent[1], -tangent[0]])
        owners = {index for sign in (-1, 1) for index, room in enumerate(plan['rooms'])
                  if room.covers(shapely.Point(centre+normal*sign*.46))}
        if owners != set(door['rooms']):
            raise AssertionError('Doorway ownership disagrees with its spatial sides')
        linked.update(door['rooms'])
    if linked != set(range(len(plan['rooms']))):
        raise AssertionError('A space lacks a direct common-access doorway')
    floor_z = plan['floor_z_m']
    floor = occupancy(product, 0, plan['base_z_m'], floor_z)
    floor = floor.union(occupancy(product, 4, plan['base_z_m'], floor_z))
    walls = occupancy(product, 1, floor_z+.01, floor_z+AGENT_HEIGHT_M-.01)
    entry = plan['entrance']
    support = shapely.union_all([plan['rooms'][0], entry['polygon'], entry['landing']]).intersection(floor)
    region = support.difference(walls)
    snapped = shapely.set_precision(region, DERIVED_GRID_M)
    if region.symmetric_difference(snapped).area > AREA_TOL_M2:
        raise AssertionError('Exclusive-access projection exceeds the precision contract')
    free = snapped.buffer(-AGENT_RADIUS_M, join_style='mitre')
    reached = next((part for part in polygons(free) if part.covers(entry['landing'].representative_point())), None)
    corridor_clear = plan['rooms'][0].buffer(-AGENT_RADIUS_M, join_style='mitre')
    if reached is None or corridor_clear.difference(reached).area > 1e-6:
        raise AssertionError('Common access is disconnected when private floors are excluded')
    actual_corridor = plan['rooms'][0].intersection(floor).difference(walls)
    clearance = plan['corridor_routes'].distance(actual_corridor.boundary)
    if clearance < .75-1e-6:
        raise AssertionError('Actual routed corridor falls below its 1.50 m target width')
    if not plan['footprint'].covers(plan['core_envelope']):
        raise AssertionError('Core envelope leaves the source footprint or fills a courtyard')
    bounds = np.array(plan['rooms'][1].minimum_rotated_rectangle.exterior.coords)[:4]
    expected = sorted(value-PARTITION_M for value in plan['circulation']['core_dimensions_m'])
    actual = sorted(float(value) for value in np.linalg.norm(np.roll(bounds, -1, axis=0)-bounds, axis=1)[:2])
    if not np.allclose(actual, expected, atol=1e-6, rtol=0):
        raise AssertionError('Reserved core floor does not retain its stated clear dimensions')
    if len(plan['circulation_alternatives']) > 12:
        raise AssertionError('Common-access planning exceeded the twelve-attempt budget')
    if any(row['grid_nodes'] > MAX_GRID_AXIS**2 or row['grid_edges'] > 2*MAX_GRID_AXIS*(MAX_GRID_AXIS-1)
           for row in plan['circulation_alternatives']):
        raise AssertionError('Orthogonal routing exceeded its explicit grid budget')
    return result | dict(exclusive_common_access=True, corridor_min_route_clearance_m=clearance,
                         private_spaces=len(plan['rooms'])-2, core_clear_dimensions_m=actual,
                         corridor_fraction=plan['rooms'][0].area/plan['footprint'].buffer(-WALL_M, join_style='mitre').area,
                         constructed_stairs=False, native_collision=False)


def scope_checks():
    interior = shapely.Polygon(shapely.box(0, 0, 14, 10).exterior.coords,
                               [shapely.box(6, 4, 8, 6).exterior.coords])
    misleading = shapely.box(5.5, 2.7, 8.5, 7.3)
    if not all(interior.covers(shapely.Point(point)) for point in misleading.exterior.coords) or interior.covers(misleading):
        raise AssertionError('The hole control does not distinguish corners from complete containment')
    candidates, _ = placements(interior, shapely.Point(1, 1), 2, 3.)
    if not candidates or any(not interior.covers(row['polygon']) for row in candidates):
        raise AssertionError('Core placement admitted a rectangle across a courtyard')
    sample = dict(id='scope', polygon=shapely.box(0, 0, 14, 10),
                  properties={'render_height': 9., 'building:levels': 2})
    plan, repeat = build(sample), build(sample)
    verify(plan, mesh(plan))
    if plan['room_ids'] != repeat['room_ids'] or any(not a.equals_exact(b, 1e-9) for a, b in zip(plan['rooms'], repeat['rooms'])):
        raise AssertionError('Common-access identities changed without source changes')
    doorway = next(door for door in plan['doors'] if door['kind'] == 'corridor-door' and 2 in door['rooms'])
    sealed = mesh(plan | dict(doors=[door for door in plan['doors'] if door is not doorway]))
    try:
        verify(plan, sealed)
    except AssertionError:
        pass
    else:
        raise AssertionError('A physically sealed room doorway passed common-access verification')
    return dict(whole_rectangle_not_corners=True, deterministic_plans=True, actual_sealed_door_rejected=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/circulation-validation.json'))
    args = parser.parse_args()
    rows, omissions = [], []
    for sample in cases():
        plan = build(sample)
        if plan['status'] == 'unplanned':
            omissions.append(dict(label=sample['label'], reasons=plan['omissions']))
        else:
            checked = verify(plan, mesh(plan)) | dict(label=sample['label'])
            if sample['label'] in ('courtyard', 'tokyo-compact') and checked['corridor_fraction'] > .30:
                raise AssertionError('Common access consumes more than 30% of a room-preservation fixture')
            rows.append(checked)
    if len(rows) != 8 or len(omissions) != 3:
        raise AssertionError('The constructive controls or pinned real layouts lost coverage')
    result = dict(**revision(), cases=rows, omissions=omissions, controls=scope_checks(),
                  scope='First storey, exclusive common corridor and reserved core; no constructed stairs or native gameplay')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n')
    print(json.dumps(dict(status='pass', common_access=len(rows), explicit_omissions=len(omissions), controls=result['controls'])))


if __name__ == '__main__':
    main()
