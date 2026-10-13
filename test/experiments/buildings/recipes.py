"""Small CGA-like facade recipes selected from supplied tags and scoped provider context."""

import math
import shapely

from context_catalog import LOCAL_USES

FAMILIES = {
    'residential': dict(axis_m=3.2, floor_m=2.9, width=.48, height=.53),
    'commercial': dict(axis_m=3.5, floor_m=3.3, width=.68, height=.65),
    'institutional': dict(axis_m=3.6, floor_m=3.4, width=.57, height=.62),
    'industrial': dict(axis_m=5., floor_m=4.5, width=.3, height=.22),
    'garage': dict(axis_m=4., floor_m=3., width=0., height=0.),
    'religious': dict(axis_m=5., floor_m=4., width=.25, height=.55),
    'utility': dict(axis_m=5., floor_m=4., width=0., height=0.),
    'mixed-use': dict(axis_m=3.2, floor_m=3., width=.52, height=.56),
    'unknown': dict(axis_m=3.2, floor_m=2.9, width=0., height=0.),
}
KINDS = {
    'residential': 'residential house detached semidetached_house terraced_house apartments bungalow dormitory farm cabin',
    'commercial': 'commercial retail office hotel supermarket kiosk',
    'institutional': 'school college university kindergarten hospital civic public government library train_station fire_station sports_hall',
    'industrial': 'industrial warehouse hangar',
    'garage': 'garage garages shed',
    'religious': 'church cathedral chapel mosque synagogue temple religious',
    'utility': 'chimney silo storage_tank water_tower roof cooling_tower service',
}
KIND_FAMILY = {kind: family for family, kinds in KINDS.items() for kind in kinds.split()}


def choose(properties, context, polygon, top, bottom):
    supplied = next((str(properties[k]) for k in ('building:use', 'man_made', 'building:part', 'building', 'subclass', 'class')
                     if k in properties and properties[k] not in ('yes', 'building')), None)
    site = context.get('site_use', {})
    evidence = context.get('evidence', [])
    local = [r for r in evidence if r.get('role') in LOCAL_USES]
    if supplied:
        family, origin, reason = KIND_FAMILY.get(supplied, 'unknown'), 'supplied', 'explicit building attribute'
    else:
        family = KIND_FAMILY.get(site.get('value'), 'unknown')
        origin, reason = ('context-heuristic', 'facade rhythm inferred from site use; building type unproven') if family != 'unknown' else (
                          'unavailable', 'no usable building or site classification')
        if family == 'unknown' and local:
            family, origin, reason = 'mixed-use', 'context-heuristic', 'partial local use; no claim about the whole building'
        elif family == 'unknown' and top-bottom > 3*polygon.area**.5:
            origin, reason = 'shape-heuristic', 'unknown slender body; closed replacement, not a proven utility type'
        if family != 'unknown':
            corners = list(polygon.minimum_rotated_rectangle.exterior.coords)
            width = min(math.dist(a, b) for a, b in zip(corners, corners[1:]))
            if width <= 1.9:
                family, origin, reason = 'unknown', 'shape-heuristic', 'body too narrow for an inferred facade; site use retained'
    return dict(version=1, family=family, origin=origin, reason=reason,
                supplied_kind=supplied, site_use=site, partial_uses=sorted({r['role'] for r in local}),
                parameters=FAMILIES[family].copy(),
                openings='procedural-context' if FAMILIES[family]['width'] else 'unavailable',
                roof='flat fallback unless supplied', geometry='supplied footprint and height interval')


def frontage(facade, context):
    a, b = facade['a'], facade['b']
    segment = shapely.LineString([a, b])
    tangent = (b-a)/facade['repeat'].width_m
    normal = [tangent[1], -tangent[0]]
    candidates = []
    for row in context.get('evidence', []):
        if not row.get('usable_frontage'):
            continue
        point = shapely.Point(row['position_xy'])
        direction = [point.x-(a[0]+b[0])*.5, point.y-(a[1]+b[1])*.5]
        if direction[0]*normal[0]+direction[1]*normal[1] > 0 and segment.distance(point) <= 12.:
            candidates.append(row['id'])
    return sorted(candidates)


def cga_attributes(recipe, context):
    rows = context.get('evidence', [])
    return dict(recipeFamily=recipe['family'], recipeOrigin=recipe['origin'], siteUse=recipe['site_use'].get('value', 'unknown'),
                facadeAxis=recipe['parameters']['axis_m'], floorHeight=recipe['parameters']['floor_m'],
                openingWidth=recipe['parameters']['width'], openingHeight=recipe['parameters']['height'],
                localUses=[r['properties'].get('subclass', r['properties'].get('class', '')) for r in rows if r.get('role') in LOCAL_USES],
                localUseLevels=[str(r.get('level')) if r.get('level') is not None else 'unknown' for r in rows if r.get('role') in LOCAL_USES],
                houseNumbers=sorted({str(r['properties']['housenumber']) for r in rows if r.get('role') == 'address'}))
