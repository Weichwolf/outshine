"""Independent rectangular inset geometry, collapse and fixed-height controls."""

import json
from pathlib import Path

import numpy as np
import shapely
from shapely import affinity

from evidence import revision
from outshine_ref.buildings.modelling import prepare_hip
from roof_inset_surface import half_plane, inset


def verify():
    controls = []
    for scale in (.01, 1., 100.):
        polygon = shapely.box(0, 0, 12*scale, 5*scale)
        for angle in (0., 31., 90.):
            source = affinity.translate(affinity.rotate(polygon, angle, origin=(0., 0.)), 1e4, -2e4)
            plan = prepare_hip(source)
            for even in (False, True):
                patches, level = inset(plan, 3.*scale, 'byHeight', -.5*scale, even)
                k = 3./2.5
                distance = .5*scale/np.hypot(1., k)
                expected = affinity.translate(source.buffer(-distance, join_style='mitre'), -plan.origin[0], -plan.origin[1])
                precision = max(p.precision_m for p in patches)
                actual = shapely.union_all([p.polygon for p in patches], grid_size=precision)
                area_error = actual.symmetric_difference(expected).area
                tolerance = max(1e-7, source.length*np.sqrt(2)*precision)
                assert area_error <= tolerance
                assert abs(level-.5*scale*k/np.hypot(1., k)) < 1e-10
                controls.append(dict(scale=scale, angle_deg=angle, even=even,
                                     area_error_m2=area_error, area_tolerance_m2=tolerance))
    empty, level = inset(prepare_hip(shapely.box(0, 0, 12, 5)), 3., 'byHeight', -100.)
    assert not empty and level > 3.
    flat = shapely.box(0, 0, 1, 1)
    assert half_plane(flat, [0., 0.], 2., 1.).equals(flat)
    assert half_plane(flat, [0., 0.], 2., 3.).is_empty
    for overhang in (0., 1., float('nan')):
        try:
            inset(prepare_hip(flat), 3., 'byHeight', overhang)
        except ValueError:
            pass
        else:
            raise AssertionError('Invalid inset overhang accepted')
    return revision() | dict(status='pass', controls=controls, collapse=True,
                             flat_field=True, invalid_overhang_rejected=True,
                             scope='Fixed-height inset in the local roof plane; no full Shape/UV/root-sharing proof.')


if __name__ == '__main__':
    report = verify()
    Path('build/experiments/buildings/roof-inset-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print('18 independent inset controls, collapse and flat-field cases passed')
