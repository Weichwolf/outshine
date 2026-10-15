"""Independent whole-guard/body measures and all slab sections of the far union."""

from types import SimpleNamespace

import numpy as np
from boolean_sequence_reference import reference, section
from stepped_source import layers, roof_volume, terrace_volumes
from structural_shell_geometry import mesh_section, native
from verify_boolean_surfaces import measure


def declared(plan):
    values = [*layers(plan), roof_volume(plan, 1), *terrace_volumes(plan, 1)]
    prisms = {
        s.identifier: SimpleNamespace(polygon=s.polygon, low=s.low_m, high=s.high_m)
        for s in values
    }
    tree = values[0].identifier
    for source in values[1:]:
        tree = ("union", tree, source.identifier)
    return values, prisms, tree


def geometry(plan, product):
    _, prisms, tree = declared(plan)
    expected = reference(prisms, tree)
    actual = measure(SimpleNamespace(mesh=product.mesh, roots=product.vertex_roots))
    np.testing.assert_allclose(actual, expected, atol=1e-7, rtol=1e-9)
    native(product.mesh, product.vertex_roots)
    limits = sorted({z for p in prisms.values() for z in (p.low, p.high)})
    error = 0.0
    for low, high in zip(limits[:-1], limits[1:], strict=True):
        height = (low + high) / 2
        wanted = section(prisms, tree, height)
        actual = mesh_section(product.mesh, product.vertex_roots, height)
        error = max(error, wanted.symmetric_difference(actual).area)
    if error > 1e-7:
        raise ValueError("Complete far body/guard union lost a planned slab section")
    return dict(
        volume_m3=expected[0],
        area_m2=expected[1],
        maximum_section_error_m2=error,
        slabs=len(limits) - 1,
    )
