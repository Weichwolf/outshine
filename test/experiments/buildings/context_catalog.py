"""Architecture roles for all provider layers; source semantics stay separate from recipes."""

from collections import Counter


LAYER_ROLES = {
    'building': 'supplied footprint, height interval, wall colour and parent suppression',
    'landuse': 'area coverage and site-use hint; not a supplied building type',
    'landcover': 'ground surroundings; never a facade or roof material',
    'park': 'protected/public landscape context; names and rank are metadata',
    'poi': 'local uses, entrances and amenities; unique host and floor scope required',
    'housenumber': 'address of a uniquely containing footprint',
    'transportation': 'nearby access/frontage candidates; levels and bridges limit use',
    'transportation_name': 'nearby street/route labels; not a supplied street width',
    'aeroway': 'airfield surroundings; class/ref do not prove a hangar',
    'water': 'water contact/surroundings; no invented platform or stilt construction',
    'waterway': 'nearby channel context; layer/brunnel constrain physical association',
    'water_name': 'water labels retained in regional source metadata',
    'boundary': 'administrative/provenance metadata; not building geometry',
    'place': 'settlement names/classification retained as regional metadata',
    'aerodrome_label': 'airport identity/elevation retained as regional metadata',
    'mountain_peak': 'terrain landmark/elevation retained as regional metadata',
}

POI_GROUPS = {
    'retail': 'alcohol_shop bakery bicycle butcher car clothing_store grocery hairdresser laundry pharmacy shop',
    'food': 'bar beer cafe fast_food ice_cream restaurant',
    'office': 'office bank post',
    'lodging': 'lodging',
    'education': 'college library school',
    'health': 'dentist doctors hospital veterinary',
    'civic': 'fire_station police prison town_hall',
    'culture': 'aquarium art_gallery attraction castle cinema museum music theatre place_of_worship',
    'entrance': 'entrance gate lift_gate sally_port',
    'street-furniture': 'atm bicycle_parking bollard drinking_water information picnic_site recycling shelter stile telephone toilets waste_basket',
    'transport': 'aerialway bicycle_rental border_control bus ferry_terminal harbor motorcycle_parking parking railway fuel toll_booth',
    'landscape': 'basin brownfield campsite cemetery dog_park enclosure garden monument park theme_park zoo',
    'recreation': 'athletics australian_football baseball basketball billiards bmx boules bowls boxing canoe chess climbing climbing_adventure cricket cricket_nets curling cycle_barrier cycling diving equestrian escape_game field_hockey free_flying golf gymnastics hackerspace handball horse_racing ice_rink judo karting model_aerodrome motor multi paragliding pitch playground racquet rowing rugby running sailing scuba_diving shooting skateboard skating skiing sports_centre stadium surfing swimming swimming_pool table_soccer table_tennis tennis volleyball water_park yoga',
}
POI_ROLES = {value: group for group, values in POI_GROUPS.items() for value in values.split()}
LOCAL_USES = {'retail', 'food', 'office', 'lodging', 'education', 'health', 'civic', 'culture'}
AREA_LAYERS = {'landuse', 'landcover', 'park', 'water', 'aeroway'}
POINT_LAYERS = {'poi', 'housenumber'}
NEAR_LAYERS = {'transportation', 'transportation_name', 'waterway', 'aeroway', 'water'}
REGIONAL_LAYERS = set(LAYER_ROLES)-AREA_LAYERS-POINT_LAYERS-NEAR_LAYERS-{'building'}
ACCESS_CLASSES = {'motorway', 'trunk', 'primary', 'secondary', 'tertiary', 'minor', 'service', 'path', 'track'}


def poi_role(properties):
    return POI_ROLES.get(properties.get('class'), 'unclassified-source-metadata')


def field_role(layer, key):
    if key.startswith('name') or key.startswith('route_') or key in ('rank', 'ref', 'ref_length', 'iata', 'icao'):
        return 'label/source metadata; no extra geometry or claim about building form'
    if layer in REGIONAL_LAYERS:
        return LAYER_ROLES[layer]
    if key in ('level', 'layer', 'indoor', 'brunnel', 'access', 'foot', 'bicycle', 'horse', 'oneway', 'ramp'):
        return 'scope/access constraint; level/layer is not a measured height in metres'
    if key in ('class', 'subclass', 'housenumber') or layer == 'building':
        return LAYER_ROLES.get(layer, 'unclassified source metadata')
    return 'preserved source parameter; another world product may consume it'


def catalog(layers):
    rows = []
    for name, layer in sorted(layers.items()):
        features = layer.get('features', [])
        keys = Counter(k for feature in features for k in feature['properties'])
        classes = Counter(str(feature['properties'].get('class', '')) for feature in features)
        rows.append(dict(layer=name, features=len(features), role=LAYER_ROLES.get(name, 'unclassified source metadata'),
                         fields={key:dict(features=count, role=field_role(name, key)) for key, count in sorted(keys.items())},
                         classes=dict(sorted(classes.items()))))
    return rows
