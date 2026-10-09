# OpenFreeMap: gelieferter Klassenkatalog

Stand: 2026-10-09. [Messdaten](openfreemap-inventory-20261009.json),
[TileJSON](openfreemap-tilejson-20261009.json). Stichprobe; keine weltweite Vollständigkeit.
Featurezahlen zählen Kachelpuffer und wiederholte Zoomabdeckung mit. Keine eindeutigen Objektzahlen.

| Layer | Feature-Vorkommen | Gelieferte Schlüssel ohne Namen |
|---|---:|---|
| `aerodrome_label` | 136 | `class`, `ele`, `ele_ft`, `iata`, `icao` |
| `aeroway` | 4854 | `class`, `ref` |
| `boundary` | 544 | `adm0_l`, `adm0_r`, `admin_level`, `class`, `disputed`, `maritime` |
| `building` | 20227 | `colour`, `hide_3d`, `render_height`, `render_min_height` |
| `housenumber` | 91842 | `housenumber` |
| `landcover` | 48536 | `class`, `subclass` |
| `landuse` | 20164 | `class` |
| `mountain_peak` | 2975 | `class`, `customary_ft`, `ele`, `ele_ft`, `rank` |
| `park` | 1916 | `class`, `rank` |
| `place` | 18738 | `capital`, `class`, `iso_a2`, `rank` |
| `poi` | 547964 | `agg_stop`, `class`, `indoor`, `layer`, `level`, `rank`, `subclass` |
| `transportation` | 182843 | `access`, `bicycle`, `brunnel`, `class`, `expressway`, `foot`, `horse`, `indoor`, `layer`, `level`, `mtb_scale`, `network`, `official`, `oneway`, `ramp`, `service`, `subclass`, `surface`, `toll` |
| `transportation_name` | 46004 | `class`, `indoor`, `layer`, `level`, `network`, `ref`, `ref_length`, `route_1_colour`, `route_1_name`, `route_1_network`, `route_1_ref`, `route_2_colour`, `route_2_name`, `route_2_network`, `route_2_ref`, `route_3_colour`, `route_3_name`, `route_3_network`, `route_3_ref`, `route_4_name`, `route_4_network`, `route_4_ref`, `route_5_name`, `route_5_network`, `route_5_ref`, `subclass` |
| `water` | 10066 | `brunnel`, `class`, `id`, `intermittent` |
| `water_name` | 3098 | `class`, `intermittent` |
| `waterway` | 3918 | `brunnel`, `class`, `intermittent` |

## aerodrome_label: class

`international`, `military`, `other`, `private`


## aeroway: class

`aerodrome`, `apron`, `gate`, `helipad`, `heliport`, `runway`, `taxiway`


## boundary: class

`aboriginal_lands`


## landcover: class

`farmland`, `grass`, `ice`, `rock`, `sand`, `wetland`, `wood`


## landcover: subclass

`allotments`, `bare_rock`, `beach`, `farmland`, `fell`, `flowerbed`, `forest`, `garden`, `glacier`, `golf_course`, `grass`, `grassland`, `heath`, `meadow`, `orchard`, `park`, `plant_nursery`, `recreation_ground`, `sand`, `scree`, `scrub`, `shrubbery`, `village_green`, `vineyard`, `wetland`, `wood`


## landuse: class

`animal_shelter`, `attraction`, `bus_station`, `cemetery`, `college`, `commercial`, `community_centre`, `construction`, `dam`, `driver_training`, `education`, `garages`, `garden`, `grass`, `highway`, `hospital`, `industrial`, `kindergarten`, `library`, `military`, `music_venue`, `neighbourhood`, `official_residence`, `parade_ground`, `park`, `pitch`, `playground`, `quarry`, `quarter`, `railway`, `recreation_ground`, `residential`, `retail`, `school`, `stadium`, `student_accommodation`, `suburb`, `theme_park`, `track`, `university`, `village_green`, `zoo`


## mountain_peak: class

`arete`, `cliff`, `peak`, `ridge`, `saddle`, `volcano`


## park: class

`Biosphärenreservat Kernzone`, `FFH-Gebiet`, `Fauna-Flora-Habitat`, `Fauna-Flora-Habitat-Gebiet`, `Gartendenkmal`, `Gesamtanlage`, `Geschützte Grünanlage`, `Geschützter Landschaftsbestandteil`, `Landschaftsschutzgebiet`, `National Historical Park`, `National Monument`, `National Recreation Area`, `Natura 2000`, `Natura 2000-gebied`, `Natura2000`, `Naturdenkmal`, `Nature Reserve`, `Naturschutzgebiet`, `Naturschutzgebiet-Kernzone`, `Naturwaldreservat`, `Natuurschoonwet 1928`, `Národní přírodní rezervace (NPR)`, `Oasi Cave di Gaggio`, `Pflanzenschutzgebiet`, `Přírodní rezervace (PR)`, `Reserve`, `Ruhezone`, `Réserve biologique intégrale`, `Schongebiet`, `Special Area`, `State Park`, `UNESCO Global Geopark`, `Vogelschutzgebiet`, `Wegegebot`, `Welterbekonvention`, `Wildruhegebiet`, `Wildruhezone`, `Wildschutzgebiet`, `World Heritage Convention`, `conservation`, `estação ecológica`, `forest_reserve`, `game_land`, `geschützter Landschaftsbestandteil`, `historic`, `historic_district`, `marine`, `national_park`, `nature_monument`, `nature_reserve`, `natuurschoonwet`, `protected_area`, `přírodní rezervace (PR)`, `reserva biológica`, `sustainable`, `tombamento`, `wilderness_preserve`, `wildlife sanctuary`, `wildlife_refuge`, `zona de amortecimento`, `örtliches Schutzgebiet`, `汀角具特殊科學價值地點 Ting Kok SSSI`


## place: class

`aboriginal_lands`, `borough`, `city`, `country`, `hamlet`, `island`, `isolated_dwelling`, `neighbourhood`, `province`, `quarter`, `state`, `suburb`, `town`, `village`


## poi: class

`aerialway`, `alcohol_shop`, `aquarium`, `art_gallery`, `athletics`, `atm`, `attraction`, `australian_football`, `bakery`, `bank`, `bar`, `baseball`, `basin`, `basketball`, `beer`, `bicycle`, `bicycle_parking`, `bicycle_rental`, `billiards`, `bmx`, `bollard`, `border_control`, `boules`, `bowls`, `boxing`, `brownfield`, `bus`, `butcher`, `cafe`, `campsite`, `canoe`, `car`, `castle`, `cemetery`, `chess`, `cinema`, `climbing`, `climbing_adventure`, `clothing_store`, `college`, `cricket`, `cricket_nets`, `curling`, `cycle_barrier`, `cycling`, `dentist`, `diving`, `doctors`, `dog_park`, `drinking_water`, `enclosure`, `entrance`, `equestrian`, `escape_game`, `fast_food`, `ferry_terminal`, `field_hockey`, `fire_station`, `free_flying`, `fuel`, `garden`, `gate`, `golf`, `grocery`, `gymnastics`, `hackerspace`, `hairdresser`, `handball`, `harbor`, `horse_racing`, `hospital`, `ice_cream`, `ice_rink`, `information`, `judo`, `karting`, `laundry`, `library`, `lift_gate`, `lodging`, `model_aerodrome`, `monument`, `motor`, `motorcycle_parking`, `multi`, `museum`, `music`, `office`, `paragliding`, `park`, `parking`, `pharmacy`, `picnic_site`, `pitch`, `place_of_worship`, `playground`, `police`, `post`, `prison`, `racquet`, `railway`, `recycling`, `restaurant`, `rowing`, `rugby`, `running`, `sailing`, `sally_port`, `school`, `scuba_diving`, `shelter`, `shooting`, `shop`, `skateboard`, `skating`, `skiing`, `sports_centre`, `stadium`, `stile`, `surfing`, `swimming`, `swimming_pool`, `table_soccer`, `table_tennis`, `telephone`, `tennis`, `theatre`, `theme_park`, `toilets`, `toll_booth`, `town_hall`, `veterinary`, `volleyball`, `waste_basket`, `water_park`, `yoga`, `zoo`


## poi: subclass

`*`, `Centro_de_Visitantes_e_Acesso_Vertical`, `Ninon de Lenclos`, `accessories`, `accountant`, `advertising_agency`, `alcohol`, `alpine_hut`, `american_football`, `american_handball`, `animist`, `antiques`, `antoinist`, `aquarium`, `archery`, `architect`, `art`, `arts_centre`, `artwork`, `association`, `athletics`, `athletics;soccer`, `athletics;soccer;basketball;volleyball`, `atm`, `attraction`, `audioguide`, `australian_football`, `australian_football;cricket`, `aviary`, `badminton`, `bag`, `bakery`, `bank`, `bar`, `baseball`, `baseball;softball`, `basin`, `basketball`, `basketball;futsal`, `basketball;handball`, `basketball;multi`, `basketball;netball`, `basketball;netball;futsal`, `basketball;running`, `basketball;soccer`, `basketball;soccer;handball`, `basketball;soccer;handball;volleyball`, `basketball;soccer;volleyball`, `basketball;streetball`, `basketball;tennis`, `basketball;volleyball;handball`, `bbq`, `beachvolleyball`, `beauty`, `bed`, `beverages`, `bicycle`, `bicycle_parking`, `bicycle_rental`, `biergarten`, `billiards`, `bmx`, `board`, `board;map`, `bodybuilding`, `bollard`, `books`, `bord`, `border_control`, `boules`, `boutique`, `bowls`, `boxing`, `brownfield`, `buddhist`, `buddhist;christian`, `builders_of_the_adytum`, `bus_station`, `bus_stop`, `butcher`, `butterfly`, `cafe`, `calisthenics`, `camera`, `camp_site`, `canoe`, `car`, `car_parts`, `car_repair`, `caravan_site`, `carpet`, `castle`, `cemetery`, `chalet`, `charging_station`, `charity`, `chemist`, `chess`, `chinese_folk`, `chocolate`, `christian`, `cinema`, `climbing`, `climbing;bouldering`, `climbing_adventure`, `clinic`, `clothes`, `coffee`, `college`, `community_centre`, `company`, `computer`, `concierge`, `confectionery`, `confucian`, `construction_company`, `consulting`, `convenience`, `cooperative`, `copyshop`, `cosmetics`, `courier`, `courthouse`, `coworking`, `cricket`, `cricket;australian_football`, `cricket;rugby_union`, `cricket;soccer`, `cricket_nets`, `croquet`, `curling`, `cycle_barrier`, `cycling`, `cycling;bmx`, `cycling;bmx;skateboard`, `cycling;skateboard;roller_skating`, `dancing`, `deli`, `dentist`, `department_store`, `description`, `diplomatic`, `diving`, `dock`, `doctors`, `dog_park`, `doityourself`, `dormitory`, `drinking_water`, `dry_cleaning`, `educational_institution`, `electronics`, `employment_agency`, `enclosure`, `energy_supplier`, `engineer`, `equestrian`, `equestrian;soccer;volleyball;athletics`, `erotic`, `escape_game`, `estate_agent`, `fabric`, `fast_food`, `fencing`, `ferry_terminal`, `field_hockey`, `field_hockey;futsal;five-a-side`, `field_hockey;soccer`, `financial`, `financial_advisor`, `fire_station`, `fitness`, `fitness;multi`, `five-a-side`, `five-a-side;basketball`, `floorball`, `florist`, `food_court`, `football`, `football;basketball`, `foundation`, `four_square`, `free_flying`, `frozen_food`, `fuel`, `furniture`, `futsal`, `futsal;volleyball`, `gaga;football`, `gallery`, `garden`, `garden_centre`, `gate`, `general`, `geodesist`, `gift`, `glass_cabinet`, `golf`, `golf_course`, `government`, `graphic_design`, `grave_yard`, `greengrocer`, `guest_house`, `guide`, `guide_touristique`, `guidepost`, `gymnastics`, `hackerspace`, `hairdresser`, `halt`, `handball`, `handball;basketball`, `handball;netball`, `handball;soccer;volleyball`, `handball;tennis`, `handball;volleyball`, `happy_science`, `harbour_master`, `hardware`, `health_insurance`, `hearing_aids`, `hifi`, `hikingmap`, `hindu`, `hopscotch`, `horse_racing`, `hospital`, `hostel`, `hotel`, `ice_cream`, `ice_rink`, `ice_stock`, `insurance`, `interior_decoration`, `interior_design`, `it`, `jewelry`, `jewish`, `judo`, `karting`, `kindergarten`, `kiosk`, `konkokyo`, `laser_tag`, `laundry`, `lawyer`, `library`, `lift_gate`, `locksmith`, `logistics`, `long_jump`, `mall`, `map`, `map;history`, `map_board`, `marina`, `marketing`, `marketplace`, `massage`, `miniature_golf`, `mobile_phone`, `model`, `model_aerodrome`, `monument`, `motel`, `motor`, `motorcycle`, `motorcycle_parking`, `moving_company`, `multi`, `multi;athletics;climbing`, `multi;basketball`, `multi;basketball;soccer`, `multi;rugby_sevens;soccer`, `multi;soccer`, `multi;soccer;basketball`, `multifaith`, `museum`, `music`, `musical_instrument`, `muslim`, `nerf`, `netball`, `netball;basketball`, `netball;tennis`, `neutral`, `newsagent`, `newspaper`, `ngo`, `nightclub`, `notary`, `notice`, `nursing_home`, `office`, `optician`, `outdoor`, `paddle_tennis`, `padel`, `paint`, `paragliding`, `parcel_locker`, `parcours`, `park`, `parking`, `parkour`, `pelota`, `perfumery`, `pet`, `petting_zoo`, `pharmacy`, `photo`, `physician`, `pickleball`, `picnic_site`, `playground`, `police`, `political_party`, `positivist`, `post_box`, `post_office`, `prison`, `private_investigator`, `property_management`, `pub`, `public_building`, `publisher`, `quango`, `racquet`, `recycling`, `religion`, `religion_of_humanity`, `research`, `restaurant`, `riversurf`, `roller_skating`, `route_marker`, `rowing`, `rugby`, `rugby;soccer`, `rugby_league`, `rugby_league;soccer;rugby_union`, `rugby_union`, `rugby_union;soccer`, `ruins`, `running`, `sailing`, `sally_port`, `school`, `schoolyard_handball`, `scientologist`, `screen`, `scuba_diving`, `second_hand`, `security`, `shed`, `shelter`, `shinto`, `shoes`, `shooting`, `shuffleboard`, `sikh`, `skateboard`, `skateboard;bmx`, `skateboard;roller_skating`, `skating`, `skiing`, `soccer`, `soccer;american_football`, `soccer;athletics`, `soccer;baseball`, `soccer;basketball`, `soccer;basketball;field_hockey`, `soccer;basketball;fitness`, `soccer;basketball;futsal`, `soccer;basketball;multi`, `soccer;basketball;running;fitness`, `soccer;basketball;volleyball;gymnastics;athletics;badminton;handball`, `soccer;cricket;rugby`, `soccer;field_hockey`, `soccer;futsal`, `soccer;gymnastics`, `soccer;handball`, `soccer;rugby`, `soccer;rugby_union`, `soccer;softball`, `soccer;touch_football`, `softball`, `softball;baseball`, `spiritist`, `spiritualist`, `sports`, `sports_centre`, `squash`, `stadium`, `station`, `stationery`, `stele`, `stile`, `street_soccer`, `streetball`, `subway`, `subway_entrance`, `sukyo_mahikari`, `supermarket`, `surfing`, `surveyor`, `swimming`, `swimming_area`, `swimming_pool`, `table_soccer`, `table_tennis`, `tactile_map`, `tactile_model`, `tailor`, `taoist`, `taoist;chinese_folk`, `tapu ae`, `tattoo`, `tax_advisor`, `taxi`, `telecommunication`, `telephone`, `tennis`, `tennis;43`, `tennis;basketball;soccer`, `tennis;basketball;soccer;handball`, `tennis;futsal;multi`, `tennis;netball`, `tenrikyo`, `terminal`, `theatre`, `theme_park`, `therapist`, `ticket`, `tobacco`, `toilets`, `toll_booth`, `townhall`, `toys`, `train_station_entrance`, `tram_stop`, `trampoline`, `translator`, `travel_agency`, `travel_agent`, `tutoring`, `union`, `unitarian_universalist`, `university`, `veterinary`, `video`, `video_games`, `viewpoint`, `visitor_centre`, `volleyball`, `volleyball;badminton`, `volleyball;basketball`, `volleyball;basketball;soccer`, `volleyball;soccer;handball`, `waste_basket`, `watches`, `water_park`, `water_utility`, `weapons`, `web_design`, `wedding_planner`, `wholesale`, `wildlife_park`, `wine`, `wrestling`, `yes`, `yoga`, `zoo`, `zoroastrian`, `相撲`


## transportation: class

`aerialway`, `bridge`, `busway`, `ferry`, `minor`, `minor_construction`, `motorway`, `motorway_construction`, `path`, `path_construction`, `pier`, `primary`, `primary_construction`, `raceway`, `rail`, `secondary`, `secondary_construction`, `service`, `service_construction`, `tertiary`, `tertiary_construction`, `track`, `track_construction`, `transit`, `trunk`, `trunk_construction`


## transportation: subclass

`bridleway`, `cable_car`, `chair_lift`, `corridor`, `cycleway`, `drag_lift`, `footway`, `funicular`, `gondola`, `light_rail`, `monorail`, `narrow_gauge`, `path`, `pedestrian`, `platform`, `platform_access`, `rail`, `station`, `steps`, `subway`, `tram`


## transportation_name: class

`aerialway`, `busway`, `ferry`, `minor`, `minor_construction`, `motorway`, `motorway_construction`, `path`, `path_construction`, `primary`, `primary_construction`, `raceway`, `secondary`, `secondary_construction`, `service`, `service_construction`, `tertiary`, `tertiary_construction`, `track`, `trunk`, `trunk_construction`


## transportation_name: subclass

`cable_car`, `corridor`, `cycleway`, `footway`, `junction`, `path`, `pedestrian`, `steps`


## water: class

`dock`, `lake`, `ocean`, `pond`, `river`, `swimming_pool`


## water_name: class

`bay`, `lake`, `strait`


## waterway: class

`canal`, `ditch`, `drain`, `river`, `stream`

