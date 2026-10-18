"""Caller-owned, bounded reuse of unchanged selected facets during one handoff."""

from functools import lru_cache

from stair_occupancy_candidates import CandidateVolumes, VolumeSource


def retained(catalog, maximum):
    if type(maximum) is not int or not 1 <= maximum <= 8192:
        raise ValueError("explicit bounded retained-facet capacity required")
    sources = {s.key: s for s in catalog.sources}

    @lru_cache(maxsize=maximum)
    def make(key, index):
        return sources[key].materialize(index)

    def source(original):
        def selected(index):
            return make(original.key, index)

        return VolumeSource(original.key, original.bounds, selected, original.always)

    return (
        CandidateVolumes(catalog.frame, tuple(source(s) for s in catalog.sources)),
        make.cache_info,
    )
