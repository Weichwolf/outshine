"""Independent fine-image inputs: supplied native capture or labelled source model."""

import numpy as np
from compare_directed_groups import transformed
from full_view_reference import load


def reference(path, cell, frame, *, native_input=None, native_cell=None):
    if cell == native_cell:
        source, _, _, receipt = load(native_input)
        return transformed(source, frame["basis"], frame["anchor"]), {
            "kind": "captured native geometry/UV/colour, current-policy material model",
            "receipt": receipt,
        }
    from packed_normals import unpack
    from parent_surface_inputs import open_parent
    from source_material_mesh import material_mesh
    from source_material_patches import selected_material_patches
    from source_shading_fields import wall_colours

    with open_parent(path) as parent:
        patches = selected_material_patches(
            parent, {cell}, frame["selected_sources"], frame
        )
    geometry, fields, work = material_mesh(patches)
    xyz = geometry["positions"][geometry["indices"]].reshape(-1, 3)
    face = fields["face_patches"]
    binding = np.zeros((len(face), 6), np.int32)
    binding[:, 1] = fields["patch_sources"][face]
    binding[:, 2] = fields["patch_parts"][face]
    binding[:, 3] = fields["patch_sides"][face]
    colours = np.ones((len(face), 3, 4), np.float32)
    for i, patch in enumerate(patches):
        local_normal = patch.orientation * np.cross(*patch.local_axes)
        local_normal /= np.linalg.norm(local_normal)
        stream = int(local_normal[2] > 0.5)
        selected_faces = face == i
        binding[selected_faces, 4] = stream
        colours[selected_faces] = wall_colours(
            fields["corner_uv"][selected_faces].reshape(-1, 2),
            stream,
            patch.surface["wall_colour"],
        ).reshape(-1, 3, 4)
    source = {
        "positions": xyz,
        "indices": np.arange(len(xyz)).reshape(-1, 3),
        "binding": binding,
        "uv": fields["corner_uv"].reshape(-1, 2),
        "colour": colours.reshape(-1, 4),
        "normal": np.repeat(fields["face_normal_words"], 3),
        "vertex_normal": np.repeat(unpack(fields["face_normal_words"]), 3, axis=0),
        "metres": fields["corner_metres"].reshape(-1, 2),
    }
    triangle = source["positions"].astype(float)[source["indices"]]
    nonzero = np.any(
        np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0]) != 0,
        axis=1,
    )
    source["indices"], source["binding"] = (
        source["indices"][nonzero],
        source["binding"][nonzero],
    )
    used, indices = np.unique(source["indices"], return_inverse=True)
    source["indices"] = indices.reshape(-1, 3)
    for key in ("positions", "uv", "colour", "normal", "vertex_normal", "metres"):
        source[key] = source[key][used]
    work["exact_zero_area_reference_triangles_ignored"] = int((~nonzero).sum())
    if set(np.unique(source["binding"][:, 1])) != set(frame["selected_sources"]):
        raise ValueError("fine reference must cover every selected whole source")
    return source, {
        "kind": "BSC3 analytic roof/wall geometry and semantic source-material reference; no native postweld/binding/mesher equivalence",
        "work": work,
    }
