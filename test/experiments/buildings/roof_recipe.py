"""Reserve an explicit roof within delivered heights before constructive room/storey planning."""

import numpy as np

from building_plan import CEILING_M, FLOOR_M
from building_vertical import build as storeys, mesh as body_mesh
from provider_contract import attributes, number, srgb
from recipes import choose
from roof_components import attach
from roof_planes import build as planes


ALIASES = {'flat':'flat','hipped':'hip','hip':'hip','gabled':'gable','gable':'gable','skillion':'shed','shed':'shed'}
COMPASS = {name:index*22.5 for index,name in enumerate(('N','NNE','NE','ENE','E','ESE','SE','SSE',
                                                      'S','SSW','SW','WSW','W','WNW','NW','NNW'))}


def parameters(sample, kind=None, rise=None):
    experiment_shape,experiment_rise = kind is not None,rise is not None
    props = sample['properties']
    data = attributes(props)
    if data['hide3d']:
        raise ValueError('provider hide_3d suppresses constructive building generation')
    family = choose(props,sample.get('context',{}),sample['polygon'],data['buildingHeight'],data['minHeight'])['family']
    supplied = props.get('roof:shape',props.get('roof_shape'))
    if supplied is not None:
        supplied_kind = ALIASES.get(str(supplied).lower())
        if supplied_kind is None:
            raise ValueError('supplied roof shape has no implemented constructive recipe')
        if kind is not None and kind!=supplied_kind:
            raise ValueError('experiment roof shape contradicts the supplied roof shape')
        kind = supplied_kind
    kind = kind or ('flat' if data['bodyHeight']>12. else 'shed' if family=='industrial' else
                    'flat' if sample['polygon'].area>450. else 'gable' if family=='residential' else 'hip')
    source_rise = props.get('roof:height',props.get('roof_height'))
    if source_rise is not None:
        source_rise = number(source_rise)
        if rise is not None and abs(source_rise-rise)>1e-9:
            raise ValueError('experiment roof height contradicts the supplied roof height')
        rise = source_rise
    if rise is None:
        minimum_body = 2.30+FLOOR_M+CEILING_M+.01
        levels = props.get('building:levels',props.get('levels'))
        if levels is not None:
            minimum_body *= number(levels)
        rise = 0. if kind=='flat' else min(1.6,max(0.,data['bodyHeight']-minimum_body))
    if not np.isfinite(rise) or rise<0 or rise>=data['bodyHeight'] or (kind=='flat' and rise!=0) or (kind!='flat' and rise<=0):
        raise ValueError('roof reservation has no valid height interval')
    direction = props.get('roof:direction')
    direction = (COMPASS[str(direction).upper()] if str(direction).upper() in COMPASS else number(direction)%360.) if direction is not None else None
    orientation = props.get('roof:orientation')
    if orientation is not None and orientation not in ('along','across'):
        raise ValueError('supplied roof orientation is unsupported')
    return dict(kind=kind,rise_m=rise,eave_m=data['buildingHeight']-rise,
                shape_origin='supplied' if supplied is not None else 'procedural experiment' if experiment_shape else 'procedural size/height estimate',
                rise_origin='supplied' if source_rise is not None else 'procedural experiment' if experiment_rise else 'procedural height budget',
                family=family,direction_deg=direction,across=orientation=='across',
                colour=srgb({'colour':props['roof:colour']}).tolist() if 'roof:colour' in props else [.43,.46,.48],
                colour_origin='supplied roof colour' if 'roof:colour' in props else 'procedural roof material')


def build(sample, kind=None, rise=None, method='auto'):
    roof = parameters(sample,kind,rise)
    design = planes(sample['polygon'],roof['kind'],method,roof['direction_deg'],roof['across'])
    angle = sample['properties'].get('roof:angle')
    if angle is not None:
        angle = number(angle)
        if not 0.<=angle<80. or (roof['kind']=='flat' and angle!=0.):
            raise ValueError('supplied roof angle is outside the implemented planar range')
        if roof['kind']!='flat':
            gradients = np.array([np.linalg.norm(facet['plane'][:2]) for facet in design['facets']])
            if gradients.min()<=0. or gradients.max()-gradients.min()>1e-6:
                raise ValueError('a common roof angle cannot describe these facets')
            derived_rise = np.tan(np.deg2rad(angle))/float(gradients.mean())
            if (rise is not None or roof['rise_origin']=='supplied') and abs(derived_rise-roof['rise_m'])>1e-6:
                raise ValueError('supplied roof angle conflicts with the roof height')
            roof.update(rise_m=derived_rise,eave_m=attributes(sample['properties'])['buildingHeight']-derived_rise,
                        rise_origin='supplied angle and source geometry',angle_deg=angle)
            if roof['eave_m']<=attributes(sample['properties'])['minHeight'] or derived_rise<=0.:
                raise ValueError('supplied roof angle consumes the delivered building-height interval')
    plan = storeys(sample,body_top_m=roof['eave_m'])
    plan['roof'] = roof|design
    plan['version'] = 4
    plan['roof_status'] = 'connected-roof' if plan['vertical_status']=='connected-storeys' else 'unplanned'
    plan['omissions'].append('roof component prototype; roof space is not fitted or reached, overhang/drainage and native LOD remain unproved')
    return plan


def mesh(plan):
    if plan['roof_status']!='connected-roof':
        raise ValueError('constructive roof/body plan is incomplete: '+'; '.join(plan['omissions']))
    roof = plan['roof']
    return attach(body_mesh(plan),roof,plan['footprint'],roof['eave_m'],roof['rise_m'],plan['storeys'])
