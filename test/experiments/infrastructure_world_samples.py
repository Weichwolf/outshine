"""Select real transport windows from the retained worldwide MVT inventory."""

import hashlib
import math

import numpy as np
import shapely

from infrastructure_building_clearance import features,street_kind,GIRTH_M


def site_roads(inventory,tiles,site,widths):
    name,latitude,longitude = site
    scale = math.cos(math.radians(latitude))
    anchor = np.array([longitude/360*GIRTH_M,math.asinh(math.tan(math.radians(latitude)))*GIRTH_M/(2*math.pi)])
    roads,receipts = [],[]
    for receipt in inventory['receipts']:
        if receipt['tile'][0]!=14 or name not in receipt['sites']:
            continue
        path = tiles/(hashlib.sha256(receipt['url'].encode()).hexdigest()+'.pbf')
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest()!=receipt['sha256']:
            raise ValueError(f'changed source tile: {path}')
        receipts.append(receipt)
        z,x,y = receipt['tile']
        span = GIRTH_M/(1<<z)
        for layer,extent,kind,properties,parts in features(data):
            if layer!='transportation' or kind!=2:
                continue
            for part in parts:
                xy = ((np.asarray(part)/extent+[x,y])*[span,-span]+[-GIRTH_M/2,GIRTH_M/2]-anchor)*scale
                if len(xy)<2:
                    raise ValueError('transport source has fewer than two coordinates')
                roads.append(dict(line=shapely.LineString(xy),width=widths.get(street_kind(properties)),
                                  properties=properties))
    if not receipts:
        raise ValueError(f'no retained level-14 source tiles for {name}')
    return roads,dict(place=name,origin=[latitude,longitude],source_tiles=receipts,
                      projection='Mercator scaled at the site latitude; sample offsets are translations.')


def window_roads(roads,tree,centre,radius):
    centre = np.asarray(centre,dtype=float)
    bounds = shapely.box(*(centre-radius),*(centre+radius))
    indices = tree.query(bounds,predicate='intersects')
    lines = shapely.transform(np.array([roads[i]['line'] for i in indices],dtype=object),lambda xy:xy-centre)
    return [dict(roads[i],line=line) for i,line in zip(indices,lines)]
