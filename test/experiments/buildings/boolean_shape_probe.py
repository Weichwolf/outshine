"""Experimental logical Shape assembly; full resource and recursive contracts pending."""

from dataclasses import dataclass,replace

from boolean_logical_faces import recover,mesh_fields
from boolean_semantic_attributes import attributes,cut_edges
from boolean_shape_resources import bind_resources
from outshine_ref.buildings.modelling import Frame,Material,Shape


@dataclass(frozen=True)
class BooleanShape:
    shape: object
    regions: object
    attributes: object
    overlay: object
    cut_edges: object
    texture_origins: tuple


def assemble(result):
    bound=attributes(result);regions=recover(result,bound);first=result.product.sources[0].shape
    definitions,textures,origins=bind_resources(bound)
    scope=replace(first.scope,object_frame=Frame())
    shape=Shape.from_mesh(mesh_fields(result.product,bound,regions),scope,regions.faces,
        group_names=bound.group_names,material_defs=definitions)
    shape=replace(first,geometry=shape.geometry,material=Material(),textures=textures,asset=None)
    return BooleanShape(shape,regions,bound,result,cut_edges(result),origins)
