"""Compare the own local repair with an isolated SDK helper and full roof output."""

import argparse
import json
from pathlib import Path

import numpy as np
from scipy.spatial import cKDTree
import shapely

from compare_roof_oracle import point_edges
from evidence import revision
from esri_roof_oracle import reference_repair
from roof_contour_repair import repair, used_points


def node_error(a, b):
    return float(max(cKDTree(a).query(b)[0].max(), cKDTree(b).query(a)[0].max()))


def edge_error(a, faces_a, b, faces_b):
    def segments(points, faces):
        return np.asarray([points[[x, y]] for f in faces for x, y in zip(f, f[1:]+f[:1])])
    return max(point_edges(used_points(a, faces_a), segments(b, faces_b)),
               point_edges(used_points(b, faces_b), segments(a, faces_a)))


def compare(row):
    points = np.asarray(row['own_positions_m'])
    own = repair(points, row['own_faces'])
    sdk = reference_repair(points, row['own_faces'])
    helper_points = np.asarray(sdk['positions_m'])
    helper_error = node_error(own.points, helper_points)
    topology_equal = own.faces == tuple(map(tuple, sdk['faces']))
    original_sdk = row['sdk_geometry']
    roof_points = np.asarray(original_sdk['positions_m'])
    boundary_error = edge_error(own.points, own.faces, roof_points, original_sdk['faces'])
    maximum = node_error(own.points, roof_points)
    invalid = [i for i, face in enumerate(own.faces) if not shapely.Polygon(own.points[list(face), :2]).is_valid]
    tolerance = row['tolerance_m']
    status = ('pass' if maximum <= tolerance else 'subdivision-equivalent' if boundary_error <= tolerance
              else 'mismatch') if not invalid else 'invalid-projection'
    return {k: row[k] for k in ('id', 'mode', 'value', 'even', 'overhang_m')} | dict(
        previous_status=row['status'], helper_node_error_m=helper_error,
        helper_topology_equal=topology_equal, helper_status='pass' if topology_equal and helper_error < 1e-7 else 'mismatch',
        status=status, node_error_m=maximum, boundary_error_m=boundary_error, tolerance_m=tolerance,
        invalid_projection_facets=invalid, repairs=len(own.changes), checked_pairs=own.checked_pairs,
        own_positions_m=own.points.tolist(), own_faces=own.faces,
        input_positions_m=row['own_positions_m'], input_faces=row['own_faces'],
        sdk_helper_geometry=sdk, sdk_roof_geometry=original_sdk)


def run(source, output):
    data = json.loads(source.read_text())
    results = []
    for row in data['results']:
        if row['status'] == 'error':
            results.append(dict(id=row['id'], status='input-error', reason=row['reason']))
            continue
        try:
            result = compare(row)
        except Exception as error:
            result = dict(id=row['id'], status='error', reason=str(error))
        results.append(result)
        if result['status'] not in ('pass', 'subdivision-equivalent') or result.get('helper_status') != 'pass':
            print(json.dumps({k: v for k, v in result.items() if k not in (
                'own_positions_m', 'own_faces', 'input_positions_m', 'input_faces', 'sdk_helper_geometry', 'sdk_roof_geometry')}), flush=True)
    counts = {s: sum(r['status'] == s for r in results) for s in sorted({r['status'] for r in results})}
    helper_counts = {s: sum(r.get('helper_status') == s for r in results)
                     for s in ('pass', 'mismatch')}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(revision() | dict(results=results, counts=counts, helper_counts=helper_counts,
        input_commit=data['commit'], input_tree_dirty=data['experiment_tree_dirty'],
        scope='Own consecutive closest-line repair versus identical-input SDK helper; end-to-end nodes/edges against full roof kernel. No UV/tessellation/Shape overhang or complete validity proof.'), indent=2)+'\n')
    print(dict(roof_counts=counts, helper_counts=helper_counts), flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, default=Path('build/experiments/buildings/hip-overhang-comparison.json'))
    parser.add_argument('--output', type=Path, default=Path('build/experiments/buildings/hip-repair-comparison.json'))
    args = parser.parse_args()
    run(args.input, args.output)
