"""Direct geometry channels and independent transformed normal orientation controls."""

import numpy as np
import trimesh

from modelling_asset_cases import UV, VERTICES
from outshine_ref.buildings.modelling.asset_decode import scene
from outshine_ref.buildings.modelling.asset_load import transformed
from outshine_ref.buildings.modelling.insert import Asset
from outshine_ref.buildings.modelling.scope import Scope
from outshine_ref.buildings.modelling.shape import Shape
from modelling_checks import near


def body(asset):
    return Shape(
        asset.geometry,
        Scope(position=asset.minimum, size=asset.geometry.reference_size),
        asset=asset,
    )


def appearance():
    colours = (
        np.array(
            [
                [0, 128, 255, 64],
                [255, 16, 32, 255],
                [64, 32, 16, 128],
                [127, 129, 1, 0],
            ],
            dtype=float,
        )
        / 255
    )
    coordinates = (
        np.array([[0, 65535], [32768, 16], [65535, 32768], [64, 128]], dtype=float)
        / 65535
    )
    source = Shape.from_mesh(
        dict(
            vertices=VERTICES,
            faces=np.array([[0, 1, 2], [0, 2, 3]]),
            uv=UV,
            uv1=coordinates,
            vertex_colors=colours,
        )
    )
    asset = Asset.from_shape(source)
    for mesh in (
        source.emit(),
        body(asset).emit(),
        *[s.emit() for s in body(asset).comp("g")],
    ):
        near(mesh["vertex_colors"], colours)
        near(mesh["uv1"], coordinates)
        near(mesh["uv"], UV)
    operations = [
        lambda: body(asset).extrude(1),
        lambda: body(asset).split("x", 1),
        lambda: body(asset).split_area("x", 1),
        lambda: body(asset).comp("e"),
    ]
    for operation in operations:
        try:
            operation()
        except NotImplementedError:
            pass
        else:
            raise AssertionError("Additional appearance channel was silently dropped")
    return dict(
        native_colors=True,
        native_secondary_uv=True,
        component_channels=True,
        unsupported_edits_rejected=len(operations),
    )


def orientations():
    mesh = trimesh.Trimesh(
        VERTICES,
        [[0, 1, 2], [0, 2, 3]],
        vertex_normals=np.tile([0.0, 0.0, 1.0], (4, 1)),
        process=False,
    )
    rig = trimesh.transformations.rotation_matrix(np.pi / 4, [0, 0, 1])
    rig[:3, 3] = [3, 4, 5]
    a = np.array(
        [
            [2.0, 0.0, 1.0, 1.0],
            [0.5, 3.0, 0.0, 2.0],
            [1.0, 0.0, 1.0, 3.0],
            [0.0, 0.0, 0.0, 1.0],
        ]
    )
    b = np.diag([-1.5, 2.0, 1.0, 1.0])
    b[:3, 3] = [-2, 1, 4]
    world = scene(
        dict(
            geometry={
                "pane": dict(
                    vertices=VERTICES,
                    faces=mesh.faces,
                    vertex_normals=mesh.vertex_normals,
                )
            },
            graph=[
                dict(frame_to="rig", matrix=rig),
                dict(frame_from="rig", frame_to="a", geometry="pane", matrix=a),
                dict(frame_from="rig", frame_to="b", geometry="pane", matrix=b),
            ],
        )
    )
    for name, expected in (("a", rig @ a), ("b", rig @ b)):
        matrix, geometry = world.graph[name]
        near(matrix, expected)
        vertices, normals, faces = transformed(world.geometry[geometry], matrix)
        near(vertices, VERTICES @ expected[:3, :3].T + expected[:3, 3])
        corners = vertices[faces]
        geometric = np.cross(
            corners[:, 1] - corners[:, 0], corners[:, 2] - corners[:, 0]
        )
        geometric /= np.linalg.norm(geometric, axis=1)[:, None]
        near(normals[faces], np.repeat(geometric[:, None, :], 3, axis=1))
    for matrix in (np.diag([0.0, 0.0, 0.0, 1.0]), np.full((4, 4), np.nan)):
        try:
            transformed(mesh, matrix)
        except ValueError:
            pass
        else:
            raise AssertionError("Singular/nonfinite source transform accepted")
    return dict(
        parent_transforms=True,
        shear_inverse_transpose=True,
        reflection_fronts=True,
        invalid_transforms=2,
    )


def controls():
    return appearance() | orientations()
