"""Independent topology/occupancy checks and complete-source preservation."""

import numpy as np
from graph_wood_surface import joined_wood
from plant_surface import solid_from_mesh
from plant_surface_checks import topology, winding_inside
from spatial_examples import spatial_graph


def check_wood(mesh, axes, originals, detail="near"):
    checked = topology(mesh)
    assert checked["faces_against_corner_normals"] == 0, checked
    points = []
    segments = 48 if detail == "near" else 12
    for axis in axes:
        points.extend(axis.centres(np.arange(1, segments) / segments))
    points = np.asarray(points)
    assert winding_inside(mesh, points).all()
    union = solid_from_mesh(originals[0])
    for source in originals[1:]:
        union += solid_from_mesh(source)
    result = solid_from_mesh(mesh)
    lost = (union - result).volume()
    assert lost < 1e-10, lost
    assert result.volume() >= union.volume() - 1e-10
    assert (
        np.isfinite(mesh["bark_coordinates"]).all() and (mesh["bark_radius"] > 0).all()
    )
    return checked | {
        "independent_axis_interior_probes": len(points),
        "lost_original_volume_m3": lost,
        "added_volume_m3": result.volume() - union.volume(),
    }


def verify_seed(seed):
    graph = spatial_graph(seed)
    cap = sum(graph.capacities.values())
    leaves = graph.instances(3, cap)
    before = [(p.path, p.origin.copy(), p.frame.copy()) for p in leaves]
    result = joined_wood(graph)
    for pose, (path, origin, frame) in zip(graph.instances(3, 0), before):
        assert pose.path == path
        np.testing.assert_array_equal(pose.origin, origin)
        np.testing.assert_array_equal(pose.frame, frame)
    return result, check_wood(result[0], result[2], result[3])
