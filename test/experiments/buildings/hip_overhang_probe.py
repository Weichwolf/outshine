"""Compare eave-ray extension with SDK geometry before adding Shape overhangs."""

import argparse
from dataclasses import dataclass
import json
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree
import shapely

from compare_roof_oracle import cases, point_edges
from evidence import revision
from esri_roof_oracle import reference
from outshine_ref.buildings.modelling import prepare_hip
from outshine_ref.buildings.modelling.hip_surface import edge_owners, prepare_even


@dataclass(frozen=True)
class Extension:
    points: object
    facets: tuple
    eave_drop_m: float
    invalid_facets: tuple
    overlapping_area_m2: float


def extend(plan, value, mode, overhang, even=False):
    field = prepare_even(plan, mode) if even else plan
    factor = field.factor(value, mode)
    heights = field.distances_m*factor
    original = np.c_[plan.positions_m+plan.origin, heights]
    points = original.copy()
    neighbors = {int(i): set() for i in np.flatnonzero(plan.distances_m == 0)}
    edges = edge_owners([f.node_indices for f in plan.facets])
    for (a, b), owners in edges.items():
        if len(owners) != 2:
            continue
        if a in neighbors and heights[b] > 0:
            neighbors[a].add(b)
        if b in neighbors and heights[a] > 0:
            neighbors[b].add(a)
    drop = overhang*factor/np.hypot(1., factor)
    for node, adjacent in neighbors.items():
        if len(adjacent) != 1:
            raise ValueError(f'Eave {node} has {len(adjacent)} positive incident ridge nodes; extension contract unresolved')
        ridge = next(iter(adjacent))
        points[node] -= (original[ridge]-original[node])*(drop/heights[ridge])
    polygons = tuple(shapely.Polygon(points[list(f.node_indices), :2]) for f in plan.facets)
    invalid = tuple(i for i, p in enumerate(polygons) if not p.is_valid or p.area <= 0)
    overlap = None if invalid else float(sum(p.area for p in polygons)-shapely.union_all(polygons).area)
    return Extension(points, polygons, float(drop), invalid, overlap)


def compare(row, value, mode, overhang, even):
    plan = prepare_hip(row['polygon'])
    own = extend(plan, value, mode, overhang, even)
    sdk = reference(row['polygon'], value, mode, overhang, even)
    actual = np.asarray(sdk['positions_m'])
    error = float(max(cKDTree(actual).query(own.points)[0].max(),
                      cKDTree(own.points).query(actual)[0].max()))
    own_edges = [(a, b) for f in plan.facets
                 for a, b in zip(f.node_indices, f.node_indices[1:]+f.node_indices[:1])]
    sdk_edges = [(a, b) for f in sdk['faces'] for a, b in zip(f, f[1:]+f[:1])]
    edge_error = max(point_edges(own.points, actual[np.asarray(sdk_edges)]),
                     point_edges(actual, own.points[np.asarray(own_edges)]))
    tolerance = 1e-7 if row['control'] else 5e-5
    valid = not own.invalid_facets and own.overlapping_area_m2 <= max(1e-7, row['polygon'].area*1e-10)
    status = ('pass' if error <= tolerance else 'subdivision-equivalent' if edge_error <= tolerance
              else 'mismatch') if valid else 'needs-contour-repair'
    return dict(id=row['id'], source_sha256=row.get('source_sha256'), value=value, mode=mode,
                even=even, overhang_m=overhang, status=status,
                maximum_node_distance_m=error, maximum_node_to_edge_distance_m=edge_error,
                tolerance_m=tolerance, eave_drop_m=own.eave_drop_m,
                invalid_facets=own.invalid_facets, overlapping_area_m2=own.overlapping_area_m2,
                own_positions_m=own.points.tolist(), own_faces=[f.node_indices for f in plan.facets],
                sdk_geometry=sdk)


def run(output, controls_only=False):
    results = []
    for row in cases():
        if controls_only and not row['control']:
            continue
        for mode, value in (('byHeight', 3.), ('byAngle', 25.)):
            for even in (False, True):
                for overhang in ((.5, -.5, 3.) if row['control'] else (.5, -.5)):
                    try:
                        result = compare(row, value, mode, overhang, even)
                    except Exception as error:
                        result = dict(id=row['id'], mode=mode, even=even, overhang_m=overhang,
                                      status='error', reason=str(error))
                    results.append(result)
                    if result['status'] not in ('pass', 'subdivision-equivalent'):
                        print(json.dumps({k: v for k, v in result.items()
                                          if k not in ('sdk_geometry', 'own_positions_m', 'own_faces')}), flush=True)
    counts = {status: sum(r['status'] == status for r in results)
              for status in sorted({r['status'] for r in results})}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(revision() | dict(results=results, counts=counts,
        scope='Experimental roof-kernel node/boundary comparison only. Invalid projected facets/overlaps retained; no contour repair, Shape UV/bottom/sides, trim or SDK nonplanar tessellation proof.'), indent=2)+'\n')
    print(counts, flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/hip-overhang-comparison.json'))
    parser.add_argument('--controls-only', action='store_true')
    args = parser.parse_args()
    run(args.output, args.controls_only)
