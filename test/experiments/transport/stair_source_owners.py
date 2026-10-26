"""Select actual source facets from local masks or complete-control local owners."""


def source_selection(state, key, source_ids):
    owner = source_ids.index(key)
    if "road_source_faces" in state:
        return state["road_source_faces"][key]
    return (state["road"]["face_sources"] & (1 << owner)) != 0
