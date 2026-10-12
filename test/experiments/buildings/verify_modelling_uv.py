"""Check UV edits against analytic values while retaining geometry and source appearance."""

import argparse
import json
from pathlib import Path
import warnings

import numpy as np

from evidence import revision
from modelling_uv_cases import adjacent,courtyard
from verify_modelling_extrude import near


def unchanged(source,result):
    assert source.scope==result.scope and source.comp_info==result.comp_info and source.split_info==result.split_info
    assert source.geometry.group_names==result.geometry.group_names and source.geometry.material_defs==result.geometry.material_defs
    near(source.emit()['vertices'][source.emit()['faces']],result.emit()['vertices'][result.emit()['faces']])
    for key in ('materials','groups','parts'):assert np.array_equal(source.emit()[key],result.emit()[key])


def affine_cases(source):
    uv=source.emit()['uv'];a=source.scale_uv(0,2.,-.5);near(a.emit()['uv'],uv*[2.,-.5])
    b=a.rotate_uv(0,90.);near(b.emit()['uv'],np.column_stack((-a.emit()['uv'][:,1],a.emit()['uv'][:,0])))
    c=b.translate_uv(0,.25,-.5);near(c.emit()['uv'],b.emit()['uv']+[.25,-.5])
    composed=source.transform_uv(0,[[0.,.5],[2.,0.]],[.25,-.5]);near(composed.emit()['uv'],c.emit()['uv'])
    for result in (a,b,c):
        unchanged(source,result);assert result.geometry.unit_vertices is source.geometry.unit_vertices
        assert result.geometry.fields['normals'] is source.geometry.fields['normals']
        for face in result.geometry.polygons:near(face.uvs[0],result.emit()['uv'][list(face.outer)])
    copied=source.copy_uv(0.,9.);assert copied.geometry.fields['uv9'] is source.geometry.fields['uv']
    edited=copied.translate_uv(9,1.,2.);near(edited.emit()['uv'],uv);near(edited.emit()['uv9'],uv+[1.,2.])
    restored=copied.delete_uv(0).copy_uv(9,0);near(restored.emit()['uv'],uv)
    assert [f.uvs for f in restored.geometry.polygons]==[f.uvs for f in source.geometry.polygons]
    assert source.scale_uv(5,2.,2.) is source and source.delete_uv(5) is source
    cleared=copied.copy_uv(5,9);assert 'uv9' not in cleared.emit() and not any(f.uv_sets for f in cleared.geometry.polygons)
    return 9


def normalized_cases(source):
    uv=source.emit()['uv'];count=0
    for mode,expected in [('uv',(uv-[2.,3.])/[6.,4.]),('uvUniform',(uv-[2.,3.])/6.),
                          ('u',np.column_stack(((uv[:,0]-2.)/6.,uv[:,1]))),('v',np.column_stack((uv[:,0],(uv[:,1]-3.)/4.)))]:
        result=source.normalize_uv(0,mode);near(result.emit()['uv'],expected);unchanged(source,result);count+=1
    for mode in ('uv','uvUniform','u','v'):
        result=source.copy_uv(0,1).normalize_uv(1,mode,'separatePerFace');unchanged(source,result)
        assert len(result.geometry.unit_vertices)==8
        for i,face in enumerate(result.geometry.polygons):
            ids=np.unique(result.emit()['faces'][result.emit()['parts']==i]);original=result.emit()['uv'][ids]
            low=np.array([2. if i==0 else 4.,3.]);span=np.array([2. if i==0 else 4.,4.]);expected=original.copy()
            if mode=='uv':expected=(original-low)/span
            elif mode=='uvUniform':expected=(original-low)/max(span)
            elif mode=='u':expected[:,0]=(original[:,0]-low[0])/span[0]
            else:expected[:,1]=(original[:,1]-low[1])/span[1]
            near(result.emit()['uv1'][ids],expected);near(dict(face.uv_sets)[1][0],result.emit()['uv1'][list(face.outer)])
        count+=1
    return count


def hole_cases():
    source=courtyard();copied=source.copy_uv(0,1);restored=copied.delete_uv(0).copy_uv(1,0)
    assert restored.geometry.polygons[0].uvs==source.geometry.polygons[0].uvs
    result=copied.normalize_uv(1,'uv','separatePerFace');unchanged(source,result)
    assert len(result.geometry.polygons[0].holes)==1 and len(dict(result.geometry.polygons[0].uv_sets)[1])==2
    emitted=result.emit();used=np.unique(emitted['faces']);near(emitted['uv1'][used].min(axis=0),[0.,0.]);near(emitted['uv1'][used].max(axis=0),[1.,1.])
    blank=source.delete_uv(0);assert 'uv' not in blank.extrude(2.).emit()
    for child in blank.comp('h'):assert 'uv' not in child.emit() and not child.geometry.polygons[0].uvs
    return dict(corner_copy_roundtrip=True,outer_and_hole_normalized_together=True,no_phantom_uv_after_geometry=True)


def invalid_cases(source):
    calls=[lambda:source.scale_uv(index,1,1) for index in (-1,10,.5,True,float('nan'))]
    calls.extend([lambda:source.rotate_uv(0,float('inf')),lambda:source.normalize_uv(0,'bad'),lambda:source.normalize_uv(0,'uv','bad')])
    calls.extend([lambda:source.transform_uv(0,np.eye(3)),lambda:source.transform_uv(0,np.eye(2),[0,float('inf')])])
    for call in calls:
        try:call()
        except ValueError:pass
        else:raise AssertionError('Invalid UV operation accepted')
    degenerate=source.scale_uv(0,0.,1.)
    with warnings.catch_warnings(record=True) as rows:
        warnings.simplefilter('always');result=degenerate.normalize_uv(0,'uv')
    assert result is degenerate and len(rows)==1
    separate=degenerate.normalize_uv(0,'uv','separatePerFace');unchanged(degenerate,separate)
    near(separate.emit()['uv'][separate.emit()['faces']],degenerate.emit()['uv'][degenerate.emit()['faces']])
    return len(calls)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/uv-verification.json'));args=parser.parse_args()
    source=adjacent();record=revision()|dict(status='pass',affine=affine_cases(source),normalization=normalized_cases(source),
        holes=hole_cases(),invalid=invalid_cases(source),limits='Analytic surface controls and Ghidra numeric paths; independent CGA runtime rules, wire normalization, projection, shader rendering and native frames remain open.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(record,indent=2)+'\n');print(json.dumps({k:record[k] for k in ('status','affine','normalization','invalid')}))


if __name__=='__main__':main()
