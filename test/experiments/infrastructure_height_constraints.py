"""Conservative cell bounds; common landing cells own explicit equality contacts."""

import numpy as np
import shapely

from infrastructure_building_clearance import street_kind


def grid(centre, extent, spacing):
    x = np.arange(centre[0]-extent, centre[0]+extent+spacing*.5, spacing)
    y = np.arange(centre[1]-extent, centre[1]+extent+spacing*.5, spacing)
    xx, yy = np.meshgrid(x,y)
    xy = np.stack([xx,yy],axis=-1)
    cells = shapely.box(xx[:-1,:-1],yy[:-1,:-1],xx[1:,1:],yy[1:,1:]).ravel()
    return xy, cells


def vertex_mask(selected, shape):
    mask = np.zeros(shape,dtype=bool)
    rows,cols = np.unravel_index(selected,(shape[0]-1,shape[1]-1))
    for dx,dy in ((0,0),(1,0),(0,1),(1,1)):
        mask[rows+dy,cols+dx] = True
    return mask


def constraints(roads, bands, plan, transitions, xy, cells, rules):
    tiers = sorted(plan,key=lambda tier:(tier[0],{'tunnel':0,'ground':1,'bridge':2}.get(tier[1],1)))
    height_shape = xy.shape[:2]
    maximum = np.full((len(tiers),height_shape[0]-1,height_shape[1]-1,2),np.inf)
    clearance = np.zeros((len(tiers),*height_shape))
    for tier_index,tier in enumerate(tiers):
        selected = [i for i,r in enumerate(roads) if r['tier']==tier]
        local = bands[selected]
        pairs = shapely.STRtree(local).query(cells,predicate='intersects')
        grades = np.array([rules.get(street_kind(roads[i]['properties']),{}).get('maxGradient',.08)
                           for i in selected])
        values = np.full(len(cells),np.inf)
        np.minimum.at(values,pairs[0],grades[pairs[1]])
        maximum[tier_index] = values.reshape((*maximum.shape[1:3],1))
        gaps = np.array([rules.get(street_kind(roads[i]['properties']),{}).get('clearanceM',2.5)
                         for i in selected])
        values = np.zeros(len(cells))
        np.maximum.at(values,pairs[0],gaps[pairs[1]])
        values = values.reshape(maximum.shape[1:3])
        for dx,dy in ((0,0),(1,0),(0,1),(1,1)):
            view = clearance[tier_index,dy:dy+values.shape[0],dx:dx+values.shape[1]]
            np.maximum(view,values,out=view)
    point = np.arange(np.prod(height_shape)).reshape(height_shape)
    lower,upper,gaps,equal,counts = [],[],[],[],[]
    for low in range(len(tiers)):
        for high in range(low+1,len(tiers)):
            overlap = plan[tiers[low]].intersection(plan[tiers[high]])
            if overlap.is_empty:
                continue
            bindings = []
            for transition in transitions:
                kinds = {tuple(t[1:]) for t in transition['tiers']}
                if tiers[low] in kinds and tiers[high] in kinds:
                    radius = max(roads[i]['width']*.5 for i in transition['sources']) + 1
                    bindings.append(shapely.Point(transition['position_m']).buffer(radius))
            tied = np.zeros(height_shape,dtype=bool)
            if bindings:
                landing = shapely.union_all(bindings)
                pieces = shapely.get_parts(overlap)
                overlap = shapely.union_all(pieces[~shapely.intersects(pieces,landing)])
                selected = np.flatnonzero(shapely.intersects(cells,landing))
                tied = vertex_mask(selected,height_shape)
            covered = np.flatnonzero(shapely.intersects(cells,overlap))
            contact = vertex_mask(covered,height_shape)
            active = contact | tied
            ids = point[active]
            lower.extend(ids+low*point.size)
            upper.extend(ids+high*point.size)
            flags = tied[active]
            gaps.extend(np.where(flags,0,clearance[low][active]+.3))
            equal.extend(flags)
            counts.append(dict(low=list(tiers[low]),high=list(tiers[high]),
                               clearance_nodes=int(np.count_nonzero(contact & ~tied)),
                               shared_landing_nodes=int(np.count_nonzero(tied))))
    return (tiers,maximum,np.array(lower,dtype=int),np.array(upper,dtype=int),
            np.array(gaps),np.array(equal,dtype=bool),counts)
