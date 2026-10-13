"""Deterministic metric panel material; generated maps contain no acquired imagery."""

from io import BytesIO
import hashlib

import numpy as np
from PIL import Image

from modelling_material_schema import linear_to_srgb,srgb_to_linear
from modelling_texture import Texture
from modelling_metadata import Material


def encode(name,pixels):
    image=Image.fromarray(np.asarray(np.rint(np.clip(pixels,0.,1.)*255.),dtype=np.uint8))
    stream=BytesIO();image.save(stream,format='PNG');encoded=stream.getvalue()
    return Texture('procedural/panels-v1/'+name+'-'+hashlib.sha256(encoded).hexdigest()+'.png',encoded)


def panels(resolution=512,size_m=(2.4,1.2),joint_m=.012,seed=0,color=(.72,.69,.62),depth_m=.003):
    if not isinstance(resolution,int) or resolution<16:raise ValueError('Panel resolution must be at least 16')
    size=np.asarray(size_m,dtype=float)
    if size.shape!=(2,) or not np.all(np.isfinite(size)) or np.any(size<=0):raise ValueError('Positive metric tile dimensions required')
    if not np.isfinite(joint_m) or not 0<joint_m<min(size)/4:raise ValueError('Invalid joint width')
    if not np.isfinite(depth_m) or not 0<=depth_m<=joint_m:raise ValueError('Relief depth must fit within joint width')
    rgb=np.asarray(color,dtype=float)
    if rgb.shape!=(3,) or not np.all(np.isfinite(rgb)) or np.any((rgb<0)|(rgb>1)):raise ValueError('Invalid sRGB panel colour')
    u,v=np.meshgrid((np.arange(resolution)+.5)/resolution,(np.arange(resolution)+.5)/resolution)
    distance=np.minimum(np.minimum(u,1-u)*size[0],np.minimum(v,1-v)*size[1])
    width=max(size)/resolution
    coverage=np.clip((distance-joint_m/2)/width+.5,0.,1.)
    rng=np.random.default_rng(seed);variation=np.zeros_like(u)
    for frequency,amplitude in ((3,.025),(11,.012),(37,.006)):
        phases=rng.uniform(0,2*np.pi,2)
        variation+=amplitude*np.sin(2*np.pi*frequency*u+phases[0])*np.sin(2*np.pi*(frequency+2)*v+phases[1])
    linear=srgb_to_linear(rgb)[None,None,:]*(1+variation[:,:,None])
    linear=linear*coverage[:,:,None]+.045*(1-coverage[:,:,None])
    base=encode('basecolor',linear_to_srgb(linear))
    rough=encode('roughness',np.clip(.68+variation+(.9-.68)*(1-coverage),0,1))
    height=depth_m*coverage*coverage*(3-2*coverage)
    du=(np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))/(2*size[0]/resolution)
    dv=(np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))/(2*size[1]/resolution)
    normal=np.stack((-du,-dv,np.ones_like(du)),axis=-1)
    normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
    normals=encode('normal',normal*.5+.5)
    material=Material('procedural-mineral-panels',{'colormap':base.path,'roughnessmap':rough.path,'normalmap':normals.path,'roughness':1.,'metallic':0.})
    return material,(base,rough,normals),dict(algorithm='panels-v1',seed=seed,tile_size_m=size.tolist(),resolution=[resolution,resolution],
        joint_m=joint_m,depth_m=depth_m,colour_space='basecolor sRGB; roughness and tangent normals linear',
        limits='Base level only; shading relief does not change silhouette; mip/normal-variance filtering and weathering open')
