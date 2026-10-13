"""Rank bounded court combinations before full neighboring-owner construction."""

import heapq
import shapely

from transport_turn_roadbeds import preserves_voids


def select_layout(floor,groups,maximum_trials=64):
    if not groups or any(not group for group in groups):
        return None,dict(reason='missing_candidates',trials=0)
    if maximum_trials<1:raise ValueError('positive selection budget required')
    groups=[sorted(group,key=lambda c:(c['cost'],str(c['id']))) for group in groups]
    initial=(0,)*len(groups)
    cost=lambda indices:sum(group[i]['cost'] for group,i in zip(groups,indices))
    pending=[(cost(initial),initial)];seen={initial};trials=0
    while pending and trials<maximum_trials:
        _,indices=heapq.heappop(pending);trials+=1
        selected=[group[i] for group,i in zip(groups,indices)]
        planned=shapely.union_all([floor,*[c['addition'] for c in selected]])
        if preserves_voids(floor,planned):
            return selected,dict(reason='compatible_topology',trials=trials,
                                 added_area_m2=planned.difference(floor).area)
        for axis,group in enumerate(groups):
            if indices[axis]+1>=len(group):continue
            neighbor=list(indices);neighbor[axis]+=1;neighbor=tuple(neighbor)
            if neighbor in seen:continue
            seen.add(neighbor);heapq.heappush(pending,(cost(neighbor),neighbor))
    return None,dict(reason='selection_budget_exhausted' if pending else 'no_compatible_layout',trials=trials)
