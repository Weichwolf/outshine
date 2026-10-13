"""Check actual Blender scene cameras separately from costly reference image generation."""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector
import numpy as np

from evidence import revision
from render_blender import prepare


def centre_error(configuration):
    scene=bpy.context.scene
    point=world_to_camera_view(scene,scene.camera,Vector(configuration['target']))
    width,height=configuration['profile']
    if point.z<=0.:
        raise AssertionError('Requested target is behind the actual scene camera')
    error=max(abs(point.x-.5)*width,abs(point.y-.5)*height)
    if error>.05:
        raise AssertionError('Actual scene camera projects its requested target away from the image centre')
    return float(error)


def cases():
    base=dict(profile=[854,480],fov_deg=50.,products=[],tiles={},solar_tile=None,output='unused.png',
              ground_span_m=200.,environment_centre=[80.,120.,10.])
    rows=[]
    directions=([1.,0.,0.],[-1.,0.,0.],[0.,1.,0.],[0.,-1.,0.],
                [1.,1.,.2],[-1.,1.,-.3],[0.,0.,1.],[0.,0.,-1.])
    for direction in directions:
        eye=np.array([170.9,-1564.5,1.7])
        target=eye+np.asarray(direction)*3.6
        rows.append(base|dict(eye=eye.tolist(),target=target.tolist()))
    rows.append(base|dict(profile=[1280,720],eye=[2.,-3.,4.],target=[0.,0.,1.]))
    rows.append(base|dict(eye=[12.,-16.,9.],target=[0.,0.,2.],environment_centre=[0.,0.,2.]))
    return rows


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path('build/experiments/buildings/camera-validation.json'))
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    rows=[]
    for configuration in cases():
        result=prepare(configuration)
        bpy.context.view_layer.update()
        rows.append(result|dict(target_centre_error_px=centre_error(configuration)))
        lights=[obj for obj in bpy.context.scene.objects if obj.type=='LIGHT']
        sun=next(obj for obj in lights if obj.data.type=='SUN')
        fill=next(obj for obj in lights if obj.data.type=='AREA')
        environment=Vector(configuration['environment_centre'])
        if (Vector(sun.location[:2])-Vector(environment[:2])).length>1e-4:
            raise AssertionError('Sun placement followed the camera target instead of the lighting environment')
        forward=(-fill.matrix_world.col[2].to_3d()).normalized()
        if forward.dot((environment-fill.location).normalized())<.99999:
            raise AssertionError('Fill light no longer aims at the independent environment centre')
    configuration=cases()[0]
    prepare(configuration)
    camera=bpy.context.scene.camera
    camera.rotation_euler=(Vector(configuration['environment_centre'])-camera.location).to_track_quat('-Z','Y').to_euler()
    bpy.context.view_layer.update()
    try:
        centre_error(configuration)
    except AssertionError:
        pass
    else:
        raise AssertionError('The original environment-target camera error passed the regression control')
    result=dict(**revision(),blender_version=bpy.app.version_string,cases=rows,original_camera_error_rejected=True,
                scope='Actual scene camera and lighting placement; no rendered image or native frame admission')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(status='pass',cameras=len(rows),original_camera_error_rejected=True)),flush=True)


if __name__=='__main__':
    main()
