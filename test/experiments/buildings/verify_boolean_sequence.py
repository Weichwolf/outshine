"""Original leaf UV/material support, current tags and generated doorway clearance."""

import json
from pathlib import Path
from dataclasses import replace

import numpy as np
import shapely

from boolean_attributes_probe import BOOL_AUTO_TAGS
from boolean_sequence import leaf,combine
from boolean_sequence_reference import reference,section,generated_section
from boolean_source_coordinates import rational_weights
from boolean_surface_probe import local_mesh
from outshine_ref.buildings.modelling.material import resolved_materials
from outshine_ref.buildings.modelling.material_schema import MAPS
from verify_boolean_attributes import authored
from verify_boolean_surfaces import Prism,measure,check_uv
from verify_boolean_semantic_overlay import support_coverage
from evidence import revision


def source(name,prism,owner):
    shape=authored(prism.shape(),owner)
    faces=tuple(replace(face,tags=face.tags|{'leaf.'+name}) for face in shape.geometry.polygons)
    return leaf(name,replace(shape,geometry=replace(shape.geometry,polygons=faces)))


def verify(node,expected):
    result=node.product.overlay;product=result.product
    np.testing.assert_allclose(measure(product),expected,atol=1e-7,rtol=1e-9)
    check_uv(product);support_coverage(result)
    leaves={source.identifier:source for source in node.leaves}
    original={name:local_mesh(value.shape,product.object_frame) for name,value in leaves.items()}
    definitions=resolved_materials(node.operand.shape)
    for i,supports in enumerate(result.supports):
        tags=node.product.attributes.tags[i]
        assert ('bool.A' in tags)==any(owner==0 for owner,_ in supports)
        assert ('bool.B' in tags)==any(owner==1 for owner,_ in supports)
        for owner,triangle in supports:
            parent=node.parents[owner];part=int(parent.shape.geometry.fields['parts'][triangle])
            assert parent.shape.geometry.polygons[part].tags-BOOL_AUTO_TAGS<=tags
        selected=node.preferred[i];points=product.mesh['vertices'][product.mesh['faces'][i]]
        before=original[selected.source];triangle=before['faces'][selected.triangle]
        weights=rational_weights(before['vertices'][triangle],points)
        if weights.min()<-1e-7:raise ValueError('Recursive preferred source left its original native triangle')
        np.testing.assert_allclose(weights@before['vertices'][triangle],points,atol=1e-8,rtol=1e-10)
        for channel in range(10):
            key='uv'+str(channel) if channel else 'uv'
            np.testing.assert_allclose(product.mesh[key][product.mesh['faces'][i]],weights@before[key][triangle],atol=1e-7,rtol=1e-9)
        face=leaves[selected.source].shape.geometry.polygons[selected.face]
        wanted=dict(resolved_materials(leaves[selected.source].shape)[face.material].attributes)
        actual=dict(definitions[int(node.product.attributes.materials[i])].attributes)
        for key,value in wanted.items():
            if key not in MAPS:assert actual[key]==value
        for ancestor in node.origins[i]:
            source=leaves[ancestor.source];assert int(source.shape.geometry.fields['parts'][ancestor.triangle])==ancestor.face
    return len(product.mesh['faces'])


def room():
    prisms=dict(outer=Prism(shapely.box(0,0,12,8),0.,6.),room=Prism(shapely.box(2,2,10,6),.2,7.),
        door=Prism(shapely.box(5,-1,7,2.5),.2,3.2))
    declared={name:source(name,prism,index%2) for index,(name,prism) in enumerate(prisms.items())}
    cutter=combine('cutters',declared['room'],declared['door'])
    expression=('union','room','door');verify(cutter,reference(prisms,expression))
    shell=combine('room-shell',declared['outer'],cutter.operand,'subtract')
    expression=('subtract','outer',expression);verify(shell,reference(prisms,expression))
    route=shapely.LineString([(6,-.5),(6,4.)]).buffer(.3)
    sections=[]
    for height in (.3,1.,2.):
        actual=generated_section(shell,height);expected=section(prisms,expression,height)
        if actual.symmetric_difference(expected).area>1e-7:raise ValueError('Generated room slice disagrees with original-prism expression')
        if actual.intersection(route).area>1e-7:raise ValueError('Generated wall blocks the doorway walking clearance')
        sections.append(dict(height_m=height,wall_area_m2=actual.area,route_wall_overlap_m2=actual.intersection(route).area))
    return prisms,declared,cutter,shell,sections


if __name__=='__main__':
    prisms,declared,cutter,shell,sections=room()
    report=revision()|dict(status='pass',operations=2,triangles=len(shell.origins),
        source_ids=[source.identifier for source in shell.leaves],sections=sections,volume_surface=measure(shell.product.overlay.product),
        scope='Experimental recursive closed prism CSG and generated 0.6m-wide doorway route; no native navigation/recipe/open/tilted/normal/resource-budget admission')
    Path('build/experiments/buildings/boolean-sequence-room.json').write_text(json.dumps(report,indent=2)+'\n')
    print(report['status'],report['volume_surface'],report['triangles'],flush=True)
