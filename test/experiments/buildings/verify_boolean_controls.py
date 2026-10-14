"""CSG counterexamples: corruption, cavities, thin passages and deferred frames."""

from dataclasses import replace

import numpy as np
import shapely
from shapely import affinity

from boolean_surface_probe import boolean
from outshine_ref.buildings.modelling import Frame,Shape
from verify_boolean_surfaces import Prism,check_uv,controls,measure,reference


def rejects(call,errors=(ValueError,AssertionError)):
    try:call()
    except errors:return
    raise AssertionError('Missing Boolean rejection')


def run():
    examples={key:(a,b) for key,a,b in controls()};a,b=examples['overlap']
    product=boolean(a.shape(),b.shape(),'subtract');check_uv(product);measure(product)
    altered=dict(product.mesh);altered['uv7']=altered['uv7'].copy();altered['uv7']+=.5
    rejects(lambda:check_uv(replace(product,mesh=altered)))
    altered=dict(product.mesh);altered['faces']=altered['faces'][1:]
    rejects(lambda:measure(replace(product,mesh=altered)))
    wrong=product.source_triangles.copy();wrong[:]=0
    rejects(lambda:check_uv(replace(product,source_triangles=wrong)))
    first=a.shape();second=b.shape()
    for size in [0,-1,True,1]:rejects(lambda:boolean(first,second,max_triangles=size))
    rejects(lambda:boolean(first,second,'invalid'))
    rejects(lambda:boolean(Shape.from_polygon(shapely.box(0,0,1,1)),second))
    a,b=examples['face-contact'];contact=boolean(a.shape(),b.shape(),'intersect')
    assert len(contact.mesh['faces'])==0 and len(contact.zero_volume_contacts)==1
    assert contact.zero_volume_contacts[0]['area_m2']==80.
    for key in ['nested','thin-passage']:
        a,b=examples[key];solid=boolean(a.shape(),b.shape(),'subtract')
        np.testing.assert_allclose(measure(solid),reference(a,b,'subtract'),atol=1e-7,rtol=1e-9)
        assert not solid.zero_volume_contacts
    a,b=examples['coplanar-overlap'];baseline=boolean(a.shape(),b.shape(),'subtract')
    distant=Frame((1e8,200.,-300.),(17.,31.,23.))
    placed=lambda p:replace(p,scope=replace(p.scope,object_frame=distant))
    remote=boolean(placed(a.shape()),placed(b.shape()),'subtract')
    assert remote.object_frame==distant
    for key,value in baseline.mesh.items():np.testing.assert_array_equal(remote.mesh[key],value)
    np.testing.assert_array_equal(remote.roots,baseline.roots)
    a,b=examples['overlap'];second=b.shape()
    second=replace(second,scope=replace(second.scope,object_frame=Frame((2.,0.,0.))))
    product=boolean(a.shape(),second,'subtract')
    expected=reference(a,Prism(affinity.translate(b.polygon,xoff=2.),b.low,b.high),'subtract')
    np.testing.assert_allclose(measure(product),expected,atol=1e-7,rtol=1e-9);check_uv(product)
    return dict(status='pass',controls=['UV7 corruption','missing boundary triangle','wrong source relation',
        'input/output capacity','unknown operation','open operand','nonvolumetric contact','negative cavity',
        '2mm passage','remote common frame bit identity','different translated operand frame'])


if __name__=='__main__':print(run())
