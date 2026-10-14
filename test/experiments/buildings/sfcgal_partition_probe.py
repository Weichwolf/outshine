"""Compare strict native, Python and validated hybrid coverage on identical real sources."""

import json
from pathlib import Path

import numpy as np

from evidence import revision
from outshine_ref.buildings.modelling import prepare_hip
from outshine_ref.buildings.modelling.skeleton_plan import (HipPlan, SkeletonLimits, face_boundaries, immutable_array,
                                                          input_data, refine, validate)
from outshine_ref.buildings.modelling.skeleton_native import partition
from samples import read_site
from verify_scope_library import costs
from verify_skeleton_plan import verify


def native_plan(polygon):
    origin, local, rings, boundaries = input_data(polygon, SkeletonLimits())
    nodes, faces = partition(local, rings, 1024)
    owners = face_boundaries(nodes, faces, boundaries); heights = [[] for _ in nodes]
    for face, (_, gradient, intercept) in zip(faces, owners, strict=True):
        for index in face:
            heights[index].append(nodes[index]@gradient+intercept)
    original = {tuple(point) for ring in rings for point in ring}
    distances = np.array([0. if tuple(point) in original else np.mean(values)
                          for point, values in zip(nodes, heights, strict=True)])
    nodes, distances, correction = refine(nodes, distances, faces, owners)
    facets = validate(local, nodes, distances, faces, owners)
    return HipPlan(tuple(origin), local, immutable_array(nodes), immutable_array(distances), facets, 'native', None, correction)


if __name__ == '__main__':
    rows, measurements = [], []
    for site in ('Vienna', 'Tokyo', 'Rosenheim'):
        samples, _ = read_site(site)
        selected = sorted(samples, key=lambda s: len(s['polygon'].exterior.coords), reverse=True)[:16]
        for sample in selected:
            results = {}
            calls = dict(native=lambda: native_plan(sample['polygon']), python=lambda: prepare_hip(sample['polygon'], 'skeleton'),
                         hybrid=lambda: prepare_hip(sample['polygon']))
            for name, call in calls.items():
                try:
                    plan = call()
                except (ValueError, RuntimeError) as error:
                    results[name] = dict(status='rejected',reason=type(error).__name__+': '+str(error))
                else:
                    results[name] = verify(plan) | dict(status='generated',fallback_reason=plan.fallback_reason)
            assert results['hybrid']['status']=='generated'
            rows.append(dict(source_id=sample['id'],source_sha256=sample['source_sha256'],results=results))
            if sample is selected[0] or results['python']['status']=='rejected':
                valid = {name:call for name,call in calls.items() if results[name]['status']=='generated'}
                measurements.append(dict(source_id=sample['id'],**costs(valid)))
    result = revision() | dict(status='pass',sources=rows,comparisons=measurements,
                              scope='Same source/plane/refinement/native-cap requirements. Native retains original-edge ownership or rejects; no collinear contour erasure. Native events unavailable. No engine/device gate.')
    Path('build/experiments/buildings/hip-research/sfcgal-probe.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',sources=len(rows),comparisons=len(measurements),coverage={name:sum(r['results'][name]['status']=='generated' for r in rows) for name in ('native','python','hybrid')})))
