"""Structural interpretations from delivered vector geometry, heights and scoped context."""

from dataclasses import dataclass
import math
from types import MappingProxyType

import numpy as np
import shapely


SPECIAL_KINDS = MappingProxyType({
    'chimney':'chimney', 'mast':'mast', 'antenna':'antenna',
    'communications_tower':'communications_tower', 'water_tower':'water_tower',
    'tower':'tower', 'observation_tower':'observation_tower', 'lighthouse':'lighthouse',
    'silo':'silo', 'storage_tank':'storage_tank', 'cooling_tower':'cooling_tower',
    'power_tower':'power_mast', 'power_pole':'power_mast', 'wind_turbine':'wind_turbine',
    'satellite_dish':'satellite_dish', 'telescope':'telescope', 'radome':'radome',
    'windmill':'windmill', 'watermill':'watermill', 'monument':'monument',
    'memorial':'memorial', 'obelisk':'monument', 'stele':'monument',
    'castle':'castle', 'fort':'fort', 'ruins':'ruins', 'city_gate':'city_gate',
    'shrine':'shrine', 'temple':'temple', 'church':'church', 'cathedral':'cathedral',
    'mosque':'mosque', 'synagogue':'synagogue', 'greenhouse':'greenhouse',
    'bunker':'bunker', 'hangar':'hangar', 'roof':'canopy', 'shelter':'shelter',
})


@dataclass(frozen=True)
class StructureMetrics:
    area_m2: float
    width_m: float
    length_m: float
    height_m: float
    bottom_m: float
    top_m: float
    compactness: float
    convexity: float
    coordinate_uncertainty_m: float
    industrial_coverage: float


@dataclass(frozen=True)
class StructureDecision:
    kind: str
    origin: str
    alternatives: tuple
    reason: str
    metrics: StructureMetrics
    source_field: str | None = None
    evidence_ids: tuple = ()


@dataclass(frozen=True)
class StructureMarker:
    kind: str
    source_field: str | None
    alternatives: tuple = ()


def classify_structure_marker(properties):
    """Interpret a delivered POI class without inventing its footprint, height or ownership."""
    explicit=[]
    for key in ('subclass','class'):
        value=properties.get(key)
        if value is not None and not isinstance(value,str):
            raise ValueError('Structural class must be a string')
        if value in SPECIAL_KINDS:
            explicit.append((key,SPECIAL_KINDS[value]))
    kinds={kind for key,kind in explicit}
    if len(kinds)>1:
        return StructureMarker('ambiguous',None,tuple(sorted(kinds)))
    if explicit:
        return StructureMarker(explicit[0][1],explicit[0][0])
    return None


def finite(value, name):
    if isinstance(value, (bool, np.bool_)):
        raise ValueError(name+' must be a finite scalar')
    try:
        number = float(value)
    except (TypeError, ValueError, OverflowError) as error:
        raise ValueError(name+' must be a finite scalar') from error
    if not math.isfinite(number):
        raise ValueError(name+' must be a finite scalar')
    return number


def footprint_metrics(footprint, properties, coordinate_step_m, industrial_coverage):
    if not isinstance(footprint, shapely.Polygon) or not footprint.is_valid or footprint.is_empty:
        raise ValueError('Structure needs one valid nonempty polygon in local XY metres')
    if shapely.get_coordinate_dimension(footprint)!=2 or not np.isfinite(shapely.get_coordinates(footprint)).all():
        raise ValueError('Structure footprint must have finite two-dimensional coordinates')
    step = finite(coordinate_step_m, 'Coordinate step')
    coverage = finite(industrial_coverage, 'Industrial coverage')
    if step<0 or not 0<=coverage<=1:
        raise ValueError('Invalid coordinate step or contextual coverage')
    top = finite(properties.get('render_height', 9.), 'Provider top height')
    bottom = finite(properties.get('render_min_height', 0.), 'Provider bottom height')
    if top<=bottom:
        raise ValueError('Structure height interval must be positive')
    rectangle = np.asarray(footprint.minimum_rotated_rectangle.exterior.coords)[:4]
    width, length = sorted(np.linalg.norm(rectangle[1:3]-rectangle[:2], axis=1))
    return StructureMetrics(float(footprint.area), float(width), float(length), top-bottom, bottom, top,
        4*math.pi*footprint.area/footprint.length**2, min(1., footprint.area/footprint.convex_hull.area),
        math.sqrt(2)*step, coverage)


def suppressed(properties):
    value = properties.get('hide_3d', False)
    if type(value) is bool:
        return value
    if type(value) in (float, int) and value in (0,1):
        return bool(value)
    if value in ('true','false'):
        return value=='true'
    raise ValueError('Invalid provider hide_3d')


def classify_structure(footprint, properties, *, coordinate_step_m=0., industrial_coverage=0.,
                       clipped=False, evidence_ids=()):
    """Choose a plausible structural family; heuristic alternatives remain explicitly unproven.

    Inputs use only provider fields ``render_height``, ``render_min_height``, ``hide_3d``,
    ``class`` and ``subclass``. Context coverage is the union of delivered industrial areas.
    POI classes belong here only after an unambiguous spatial ownership decision by the caller.
    IDs, names and locations never select a kind. A tile fragment cannot establish slenderness.
    """
    m = footprint_metrics(footprint, properties, coordinate_step_m, industrial_coverage)
    def decision(kind, origin, reason, alternatives=(), field=None):
        return StructureDecision(kind, origin, tuple(alternatives), reason, m, field, tuple(evidence_ids))
    if suppressed(properties):
        return decision('omit', 'provider', 'Provider hide_3d suppresses this source')
    marker=classify_structure_marker(properties)
    if marker and marker.kind=='ambiguous':
        return decision('ordinary', 'ambiguous-provider', 'Conflicting delivered structural classes', marker.alternatives)
    if marker:
        return decision(marker.kind, 'provider', 'Explicit delivered structural class', field=marker.source_field)
    if clipped:
        return decision('ordinary', 'incomplete-geometry', 'Tile fragment cannot establish a special structural form')
    if footprint.interiors or m.convexity<.95:
        return decision('ordinary', 'geometry', 'Courtyard or articulated footprint keeps its building programme')
    long_upper = m.length_m+m.coordinate_uncertainty_m
    short_lower = max(m.width_m-m.coordinate_uncertainty_m, 0.)
    aspect_upper = long_upper/short_lower if short_lower else math.inf
    industrial = m.industrial_coverage>=.8
    compact = aspect_upper<=1.8
    round_form = m.compactness>=.86 and aspect_upper<=1.35
    if m.length_m/m.width_m<=1.8 and long_upper<=3.5 and m.height_m>=6 and m.height_m/long_upper>=3:
        if industrial:
            return decision('chimney', 'shape-context', 'Compact slender shaft inside supplied industrial area', ('mast','tower'))
        return decision('mast', 'shape', 'Very small tall footprint favours a shaft interpretation', ('chimney','spire','narrow_building'))
    if round_form and industrial and m.height_m>=70 and m.width_m>=35 and .75<=m.height_m/long_upper<=2.5:
        return decision('cooling_tower', 'shape-context', 'Large tall round industrial body', ('storage_tank','industrial_tower'))
    if compact and industrial and m.height_m>=18 and long_upper<=16 and m.height_m/long_upper>=4:
        return decision('chimney', 'shape-context', 'Tall compact industrial shaft', ('silo','mast'))
    if round_form and m.height_m>=18 and ((long_upper<=30 and m.height_m/long_upper>=3) or m.height_m/long_upper>=8):
        return decision('tower', 'shape', 'Tall round shaft; specific function and occupancy remain unproven',
                        ('communications_tower','water_tower','chimney','observation_tower','round_highrise'))
    if round_form and m.bottom_m>=18 and m.top_m>=35:
        return decision('tower_section', 'shape', 'Elevated round component with supplied vertical interval',
                        ('tower_room','tank','roof_component'))
    if round_form and industrial and m.width_m>=4:
        if m.width_m>=35:
            return decision('industrial_round_body', 'shape-context', 'Large round industrial body without process/type evidence',
                            ('domed_industrial_building','storage_tank','reactor_building'))
        if m.height_m/long_upper>=1.5:
            return decision('silo', 'shape-context', 'Round industrial storage-scale shaft', ('storage_tank','water_tower'))
        return decision('storage_tank', 'shape-context', 'Round industrial body with moderate height', ('domed_industrial_building','silo'))
    return decision('ordinary', 'geometry', 'No defensible special-form evidence; preserve normal building interpretation')
