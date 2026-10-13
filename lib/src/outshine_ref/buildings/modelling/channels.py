"""Identify per-vertex appearance channels without confusing primitive-level fields."""


def vertex_channel(name):
    return name in ('uv','normals','vertex_colors') or (name.startswith('uv') and name[2:].isdigit())


def additional_channels(geometry):
    return tuple(name for name in geometry.fields if vertex_channel(name) and name not in ('uv','normals'))
