"""Isolated experiment injection; the shared library and its constructor stay unchanged."""

import contextlib
import contextvars
from unittest.mock import patch

import shapely
from outshine_ref.geometry.surface_patches import SurfacePatch
from stair_coverage_boundary import GENERAL_COVERAGE, mesh_witness, polygon_witness


@contextlib.contextmanager
def validation(mode, records):
    if mode not in ("general", "boundary", "mesh"):
        raise ValueError(
            "explicit general/polygon-adapter/mesh coverage alternative required"
        )
    current = contextvars.ContextVar("coverage_research_mesh", default=None)
    original_init = SurfacePatch.__post_init__

    def initialise(instance):
        token = current.set(instance.mesh)
        try:
            return original_init(instance)
        finally:
            current.reset(token)

    def coverage(polygons):
        mesh = current.get()
        if mode == "general" or mesh is None:
            proof = {"proved": False, "reason": "general_or_outside_constructor"}
        elif mode == "mesh":
            proof = mesh_witness(mesh, polygons)
        else:
            proof = polygon_witness(polygons)
        valid = proof["proved"] or bool(GENERAL_COVERAGE(polygons))
        records.append(dict(proof, valid=valid, input_polygons=len(polygons)))
        return valid

    with (
        patch.object(SurfacePatch, "__post_init__", initialise),
        patch.object(shapely, "coverage_is_valid", coverage),
    ):
        yield
