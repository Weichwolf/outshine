"""Controlled SDK repair cases, immutability and bounded failure contracts."""

import json
from pathlib import Path

import numpy as np

from compare_roof_repair import node_error
from esri_roof_oracle import reference_repair
from evidence import revision
from roof_contour_repair import repair


def verify():
    results = []
    for skew in (False, True):
        for scale in (.01, 1., 100.):
            points = np.array([[0., 0., 0.], [1., 1., 0.], [0., 1., 0.], [1., 0., 0.]])*scale
            if skew:
                points[2:, 2] = .5*scale
            for offset in (np.zeros(3), np.array([1e8, -1e8, 1e8])):
                for winding in (1, -1):
                    face = list(range(4))[::winding]
                    shifted = points+offset
                    before = shifted.copy()
                    own = repair(shifted, [face])
                    sdk = reference_repair(shifted, [face])
                    assert np.array_equal(shifted, before)
                    assert own.faces == tuple(map(tuple, sdk['faces']))
                    error = node_error(own.points, np.asarray(sdk['positions_m']))
                    assert error <= 5e-8
                    assert len(own.changes) == 1 and own.checked_pairs <= 4
                    assert len(own.faces[0]) == 3
                    results.append(dict(skew=skew, scale_m=scale, offset_m=offset.tolist(), winding=winding,
                                        maximum_node_distance_m=error, checked_pairs=own.checked_pairs))
    clean = np.array([[0., 0., 0.], [1., 0., 0.], [1., 1., 0.], [0., 1., 0.]])
    untouched = repair(clean, [[0, 1, 2, 3]])
    assert not untouched.changes and np.array_equal(untouched.points, clean)
    try:
        untouched.points[0, 0] = 1.
    except ValueError:
        pass
    else:
        raise AssertionError('Mutable repaired coordinates')
    try:
        repair([[0, 0, 0], [1, 1, 0], [0, 1, 0], [1, 0, 0]], [[0, 1, 2, 3]], max_nodes=4)
    except ValueError as error:
        assert 'capacity' in str(error)
    else:
        raise AssertionError('Repair ignored the node budget')
    for faces in ([[0, 1]], [[0, 1, 4]], [[0, 1, 2.5]]):
        try:
            repair(clean, faces)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid ring accepted')
    corrupted = untouched.points.copy()
    corrupted[0, 2] += .01
    assert node_error(untouched.points, corrupted) > 5e-8
    return revision() | dict(status='pass', controls=results, immutable=True,
                             budget_exhaustion_rejected=True, invalid_indices_rejected=True,
                             intentional_1_cm_corruption_detected=True,
                             scope='Local closest-line repair policy, including skew edges; not a general intersection or valid complete-roof proof.')


if __name__ == '__main__':
    report = verify()
    Path('build/experiments/buildings/roof-contour-repair-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print('24 SDK controls, immutability, budget, indices and corruption verified')
