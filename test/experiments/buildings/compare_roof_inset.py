"""Assess fixed-field truncation and SDK-style local repair on the same negative inputs."""

import json
from pathlib import Path

import numpy as np
import shapely

from compare_roof_oracle import cases
from evidence import revision
from outshine_ref.buildings.modelling import prepare_hip
from roof_inset_surface import inset


def compare(source):
    rows = {row['id']: row for row in cases()}
    plans = {key: prepare_hip(row['polygon']) for key, row in rows.items()}
    results = []
    for previous in source['results']:
        if previous.get('overhang_m', 0) >= 0:
            continue
        identifier = previous['id']
        plan = plans[identifier]
        patches, level = inset(plan, previous['value'], previous['mode'], previous['overhang_m'], previous['even'])
        polygons = [p.polygon for p in patches]
        precision = max((p.precision_m for p in patches), default=1e-9)
        footprint = shapely.union_all(polygons, grid_size=precision)
        area_sum = sum(p.area for p in polygons)
        overlap = area_sum-footprint.area
        escaped = footprint.difference(plan.footprint).area
        tolerance = max(1e-7, plan.footprint.area*1e-10, plan.footprint.length*np.sqrt(2)*precision+2*np.pi*precision**2)
        assert footprint.is_valid and overlap <= tolerance and escaped <= tolerance, (identifier, overlap, escaped, tolerance)
        minimum = float('inf')
        for patch in patches:
            xy = np.asarray(patch.polygon.exterior.coords)
            minimum = min(minimum, float((xy@patch.gradient+patch.intercept_m).min()))
        uncertainty = np.sqrt(2)*precision*max((np.linalg.norm(p.gradient) for p in patches), default=0.)
        assert not patches or minimum >= level-max(1e-7, uncertainty)
        sdk = previous['sdk_roof_geometry']
        points = np.asarray(sdk['positions_m'])
        sdk_polygons = [shapely.Polygon(points[f, :2]-plan.origin) for f in sdk['faces']]
        sdk_invalid = sum(not p.is_valid for p in sdk_polygons)
        difference, comparison_error = None, None
        if not sdk_invalid:
            try:
                difference = footprint.symmetric_difference(shapely.union_all(sdk_polygons)).area
            except shapely.errors.GEOSException as error:
                comparison_error = str(error)
        results.append({k: previous[k] for k in ('id', 'mode', 'value', 'even', 'overhang_m')} | dict(
            status='valid' if patches else 'collapsed', eave_level_m=level, overlap_area_m2=overlap,
            precision_m=precision, area_tolerance_m2=tolerance, height_uncertainty_m=uncertainty,
            escaped_source_area_m2=escaped, patch_count=len(patches), sdk_invalid_projected_faces=sdk_invalid,
            sdk_footprint_difference_m2=difference, sdk_comparison_error=comparison_error,
            previous_repair_status=previous['status']))
    return revision() | dict(results=results, cases=len(results), invalid=0,
                             collapsed=sum(r['status']=='collapsed' for r in results),
                             scope='Alternative valid fixed-field clipping for negative overhang; deliberately not SDK-contour equivalence. No Shape UV/bottom/sides or shared mesh-root proof yet.')


if __name__ == '__main__':
    base = Path('build/experiments/buildings')
    report = compare(json.loads((base/'hip-repair-comparison.json').read_text()))
    (base/'hip-inset-comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print(report['cases'], 'valid/collapsed fixed-field cases;', report['collapsed'], 'collapsed')
