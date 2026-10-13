"""Bind the same vertical field of view and camera basis as the Python demand model."""

import numpy as np
import mitsuba as mi

from demand import View


def camera(config,spp):
    width,height=config['profile'];view=View(config['eye'],config['target'],width=width,height=height,fov_deg=config['fov_deg'])
    matrix=np.eye(4);matrix[:3,:3]=np.column_stack((-view.basis[0],view.basis[1],view.basis[2]));matrix[:3,3]=view.eye
    sensor=mi.load_dict(dict(type='perspective',to_world=mi.ScalarTransform4f(matrix),fov=config['fov_deg'],fov_axis='y',near_clip=.001,far_clip=10000.,
        sampler=dict(type='independent',sample_count=spp),film=dict(type='hdrfilm',width=width,height=height,rfilter=dict(type='box'),pixel_format='rgb')))
    rays=[]
    for pixel in ((.5,.5),(1.,.5),(.5,0.)):
        ray,_=sensor.sample_ray(0.,.5,mi.Point2f(pixel),mi.Point2f(.5,.5));direction=np.asarray(ray.d).reshape(3)
        expected=view.basis[2]+view.basis[0]*((pixel[0]-.5)*width/view.focal_px)-view.basis[1]*((pixel[1]-.5)*height/view.focal_px)
        expected/=np.linalg.norm(expected)
        if not np.allclose(direction,expected,atol=1e-6):raise ValueError('Mitsuba camera differs from the demand projection')
        rays.append(dict(pixel=pixel,direction=direction.tolist()))
    return sensor,dict(eye=config['eye'],target=config['target'],vertical_fov_deg=config['fov_deg'],profile=config['profile'],ray_checks=rays)
