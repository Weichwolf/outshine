"""Apply method compatibility before ranking bounded turning-space seeds."""

import json
import numpy as np
import shapely

from turning_space_probe import seeds
from transport_arc_line_paths import arc_line_arc_seeds


def verify():
    entry=dict(position=np.zeros(2),heading=np.array([1.,0.]))
    exit=dict(position=np.array([5.,5.]),heading=np.array([0.,1.]))
    vehicle=dict(minimumRearAxleRadiusM=4.,widthM=1.8)
    paths=[p for _,p in arc_line_arc_seeds(entry,exit,4.2) if p.is_simple]
    source=min(paths,key=lambda p:p.length);floor=source.buffer(1.);region=shapely.box(-40,-40,40,40)
    mixed=seeds(entry,exit,source,floor,region,vehicle)
    selected=seeds(entry,exit,source,floor,region,vehicle,('CCC',))
    assert len(mixed)==len(selected)==3
    assert sum(p[2]=='CCC' for p in mixed)<3
    assert all(p[2]=='CCC' and p[5].is_simple for p in selected)
    assert [(p[0],p[1]) for p in selected]==sorted((p[0],p[1]) for p in selected)
    expanded=seeds(entry,exit,source,floor,region,vehicle,limit=24)
    assert len(expanded)>len(mixed)
    assert [(p[0],p[1]) for p in expanded[:3]]==[(p[0],p[1]) for p in mixed]
    assert seeds(entry,exit,source,floor,region,vehicle,())==[]
    return dict(method_compatibility_before_budget=True,three_seed_budget_unchanged=True,
                unsupported_families_excluded=True,eligible_seeds_ranked=True)


if __name__=='__main__':print(json.dumps(verify()))
