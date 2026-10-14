"""Independent source material/group/tag checks, collisions and resource ownership."""

import argparse
from dataclasses import replace
from io import BytesIO
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image

from boolean_attributes_probe import bind_attributes
from boolean_surface_probe import boolean
from evidence import revision
from outshine_ref.buildings.modelling import Material,Texture
from verify_boolean_surfaces import Prism,controls,check_uv,measure,reference
from compare_roof_oracle import cases


COLOURS=['#74a090','#b58c4a']


def authored(shape,owner):
    values=[dict(roughness=.82,metallic=.02),dict(roughness=.27,metallic=.78)][owner]
    name=['ceramic','alloy'][owner]
    result=shape.with_metadata(group_names={3:'walls.'+str(owner)},material_defs={9:Material(name,values)}).color(COLOURS[owner])
    if owner==0:result=result.set_material({'roughness':.61})
    return replace(result,geometry=replace(result.geometry,polygons=tuple(replace(face,tags=face.tags|{'origin.'+str(owner)})
        for face in result.geometry.polygons)))


def expected_material(owner):
    code=COLOURS[owner]
    return dict(name=['ceramic','alloy'][owner],roughness=[.61,.27][owner],metallic=[.02,.78][owner],
        **{'color.'+axis:int(code[1+2*n:3+2*n],16)/255 for n,axis in enumerate('rgb')})


def check(product,bound):
    groups={key:(owner,original) for key,owner,original in bound.group_origins}
    names=dict(bound.group_names)
    for i,(owner,triangle) in enumerate(zip(product.operands,product.source_triangles,strict=True)):
        source=product.sources[int(owner)];part=int(source.mesh['parts'][triangle]);face=source.shape.geometry.polygons[part]
        assert bound.source_faces[i]==part
        assert int(bound.groups[i]) in groups
        assert groups[int(bound.groups[i])]==(int(owner),face.group)
        assert names[int(bound.groups[i])]=='walls.'+str(owner)
        binding=bound.material_bindings[int(bound.materials[i])]
        assert dict(binding.material.attributes)==expected_material(int(owner))
        assert (int(owner),face.material) in binding.source_materials
        assert bound.tags[i]==face.tags|{'bool.A' if owner==0 else 'bool.B'}
    if np.any(product.operands==0) and np.any(product.operands==1):
        assert set(bound.groups[product.operands==0]).isdisjoint(set(bound.groups[product.operands==1]))
        assert set(bound.materials[product.operands==0]).isdisjoint(set(bound.materials[product.operands==1]))
    return len(product.mesh['faces'])


def rejected(call):
    try:call()
    except (ValueError,AssertionError):return
    raise AssertionError('Missing semantic corruption/capacity rejection')


def extra():
    _,a,b=next(controls());product=boolean(authored(a.shape(),0),authored(b.shape(),1));bound=bind_attributes(product)
    check(product,bound)
    for values in [bound.materials,bound.groups,bound.source_faces]:
        assert not values.flags.writeable
        rejected(lambda:values.setflags(write=True))
    wrong=bound.groups.copy();wrong[0]=4294967295
    rejected(lambda:check(product,replace(bound,groups=wrong)))
    rejected(lambda:bind_attributes(product,max_materials=1))
    for size in [0,-1,True,65537]:rejected(lambda:bind_attributes(product,max_materials=size))
    first=a.shape();second=b.shape();equal=bind_attributes(boolean(first,second))
    assert len(equal.material_bindings)==1 and set(equal.material_bindings[0].source_materials)=={(0,9),(1,9)}
    textures=[]
    for colour in ['#ff0000','#0000ff']:
        stream=BytesIO();Image.new('RGB',(4,4),colour).save(stream,format='PNG')
        textures.append(Texture('procedural/shared-name.png',stream.getvalue()))
    captured=bind_attributes(boolean(first.texture(textures[0]),second.texture(textures[1])))
    assert len(captured.material_bindings)==2
    for binding in captured.material_bindings:
        owner=binding.source_materials[0][0]
        assert binding.resources==(textures[owner],) and binding.resources[0] is textures[owner]
        assert dict(binding.material.attributes)['colormap']=='procedural/shared-name.png'
    return dict(status='pass',controls=['immutable arrays','group corruption','material capacity','equal material deduplication',
        'conflicting same-path procedural texture bytes','captured resource identity'])


def run(real):
    rows=list(controls())
    if real:
        import shapely
        for row in cases():
            if row['control']:continue
            polygon=row['polygon'];x,y,a,b=polygon.bounds
            rows.append((row['id'],Prism(polygon,0.,5.),Prism(shapely.box(x+(a-x)*.3,y+(b-y)*.3,x+(a-x)*.7,y+(b-y)*.7),-1.,7.)))
    results=[]
    for identifier,a,b in rows:
        for operation in ['union','subtract','intersect']:
            record=dict(id=identifier,operation=operation)
            try:
                first,second=authored(a.shape(),0),authored(b.shape(),1)
                before=[first,second];product=boolean(first,second,operation)
                np.testing.assert_allclose(measure(product),reference(a,b,operation),atol=1e-7,rtol=1e-9);check_uv(product)
                started=time.perf_counter();bound=bind_attributes(product);elapsed=(time.perf_counter()-started)*1000
                record.update(status='pass',triangles_checked=check(product,bound),binding_ms=elapsed,
                              materials=len(bound.material_bindings),groups=len(bound.group_origins))
                assert [s.shape for s in product.sources]==before
            except Exception as error:
                record.update(status='error',reason=str(error));print(record,flush=True)
            results.append(record)
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--real',action='store_true')
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/boolean-attributes-verification.json'))
    args=parser.parse_args();results=run(args.real);extra_report=extra()
    report=revision()|dict(results=results,cases=len(results),passed=sum(r['status']=='pass' for r in results),extra=extra_report,
        scope='Retained native triangle-owner material/group/tag bindings only; not coplanar dual-ownership/A-priority, transitive lineage, resource namespace rewriting or general Shape-API approval.')
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(report['passed'],'of',report['cases'],extra_report,flush=True)
    raise SystemExit(0 if report['passed']==report['cases'] else 1)
