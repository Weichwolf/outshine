import dataclasses
import math
import unittest
from collections import Counter,defaultdict

import numpy as np

from outshine_ref.buildings.modelling.radial_profile import RadialProfile
from outshine_ref.buildings.modelling.radial_mesh import radial_mesh


def closed(result):
    roots=result.vertex_roots[result.mesh['faces']]
    directed=Counter((int(t[i]),int(t[(i+1)%3])) for t in roots for i in range(3))
    assert all(count==1 and directed[b,a]==1 for (a,b),count in directed.items())
    links=defaultdict(list)
    for a,b,c in roots:
        links[int(a)].append((int(b),int(c)));links[int(b)].append((int(c),int(a)));links[int(c)].append((int(a),int(b)))
    for edges in links.values():
        following=dict(edges);assert len(following)==len(edges)
        first=edges[0][0];seen=set();current=first
        while current not in seen:
            seen.add(current);current=following[current]
        assert current==first and len(seen)==len(edges)


def expected(profile,sides):
    values=np.asarray(profile.knots);dz=np.diff(values[:,0]);angle=math.pi/sides;volume=0.;area=0.
    for role,sign in ((1,1),(2,-1)):
        r=values[:,role]
        volume+=sign*sides*math.sin(2*angle)/2*np.sum(dz*(r[:-1]**2+r[:-1]*r[1:]+r[1:]**2)/3)
        if role==1 or profile.hollow:
            area+=sides*math.sin(angle)*np.sum((r[:-1]+r[1:])*np.sqrt(dz**2+np.diff(r)**2*math.cos(angle)**2))
    area+=sides*math.sin(2*angle)/2*sum(ro**2-ri**2 for z,ro,ri in (profile.knots[0],profile.knots[-1]))
    return volume,area


class RadialTests(unittest.TestCase):
    def test_closed_solids_hollows_tips_and_concave_profiles(self):
        profiles=[((0,2,0),(5,2,0)),((0,2,0),(5,0,0)),((0,0,0),(5,2,0)),
                  ((0,0,0),(2,3,0),(5,0,0)),((0,2,1),(5,2,1)),
                  ((0,5,4.8),(3,2.5,2.3),(6,3,2.8))]
        for knots in profiles:
            for sides in (3,7,32):
                with self.subTest(knots=knots,sides=sides):
                    p=RadialProfile('source',(4,-3),knots);m=radial_mesh(p,sides=sides);closed(m)
                    volume,area=expected(p,sides)
                    self.assertAlmostEqual(m.volume_m3,volume,places=10)
                    self.assertAlmostEqual(m.area_m2,area,places=10)
                    n=m.mesh['normals'];np.testing.assert_allclose(np.linalg.norm(n,axis=1),1,rtol=0,atol=1e-14)
                    t=m.mesh['vertices'][m.mesh['faces']];face=np.cross(t[:,1]-t[:,0],t[:,2]-t[:,0])
                    self.assertTrue(np.all(np.einsum('ij,ikj->ik',face,n[m.mesh['faces']])>0))

    def test_materials_uv_seams_and_end_creases(self):
        p=RadialProfile('pipe',(0,0),((1,2,1),(5,1.6,.6)),materials=(4,5,6));m=radial_mesh(p,sides=8)
        self.assertEqual(set(m.mesh['materials']),{4,5,6})
        self.assertEqual(set(m.mesh['parts']),{0,1,2,3})
        self.assertTrue(np.isfinite(m.mesh['uv']).all())
        sides=m.mesh['parts']<2;uv=m.mesh['uv'][m.mesh['faces'][sides]]
        self.assertLessEqual(np.max(np.ptp(uv[:,:,0],axis=1)),1/8+1e-14)
        caps=m.mesh['parts']>=2
        np.testing.assert_array_equal(abs(m.mesh['normals'][m.mesh['faces'][caps],2]),1)

    def test_direct_detail_reduces_without_losing_channel_or_height(self):
        z=np.linspace(12,150,65);ro=20*np.sqrt(1+((z-110)/70)**2)
        p=RadialProfile('tower',(0,0),tuple(zip(z,ro,ro-.4,strict=True)))
        near=radial_mesh(p,error_m=.01,max_sides=512);far=radial_mesh(p,error_m=1.)
        for m,limit in ((near,.01),(far,1.)):
            closed(m);self.assertLessEqual(m.error_m,limit)
            self.assertEqual(m.mesh['vertices'][:,2].min(),12)
            self.assertEqual(m.mesh['vertices'][:,2].max(),150)
            self.assertIn(1,m.mesh['parts'])
            selected=np.asarray(p.knots)[list(m.profile_indices)]
            fine_z=np.linspace(12,150,10001)
            for role in (1,2):
                reference=np.interp(fine_z,z,np.asarray(p.knots)[:,role]);approx=np.interp(fine_z,selected[:,0],selected[:,role])
                self.assertLessEqual(np.max(abs(reference-approx)),m.profile_error_m+1e-12)
        self.assertLess(len(far.mesh['faces']),len(near.mesh['faces']))
        self.assertEqual(p.knots[0][0],12)
        tips=radial_mesh(RadialProfile('double-tip',(0,0),((0,0,0),(2,3,0),(5,0,0))),error_m=10.)
        closed(tips);self.assertGreater(tips.volume_m3,0)

    def test_replay_immutable_buffers_and_local_translation(self):
        p=RadialProfile('source',(1000000,-2000000),((7,3,0),(23,1,0)))
        a=radial_mesh(p);b=radial_mesh(p)
        for key in a.mesh:
            np.testing.assert_array_equal(a.mesh[key],b.mesh[key])
            with self.assertRaises(ValueError):a.mesh[key].setflags(write=True)
        with self.assertRaises(ValueError):a.vertex_roots.setflags(write=True)
        with self.assertRaises(dataclasses.FrozenInstanceError):p.identifier='other'
        q=radial_mesh(RadialProfile('local',(0,0),p.knots));self.assertAlmostEqual(a.volume_m3,q.volume_m3,places=7)

    def test_exact_triangle_budget_is_checked_before_emission(self):
        p=RadialProfile('cone',(0,0),((0,2,0),(5,0,0)))
        self.assertEqual(len(radial_mesh(p,sides=3,max_triangles=6).mesh['faces']),6)
        with self.assertRaises(ValueError):radial_mesh(p,sides=3,max_triangles=5)
        for kwargs in ({'error_m':1e-12,'max_sides':3},{'sides':2},{'max_triangles':True},{'max_sides':4.5}):
            with self.assertRaises(ValueError):radial_mesh(p,**kwargs)
        tiny=radial_mesh(RadialProfile('tiny-valid',(0,0),((0,1e-4,8e-5),(.01,1e-4,8e-5))))
        closed(tiny);self.assertGreater(tiny.volume_m3,0)

    def test_invalid_collapsed_pinched_and_mixed_channels_reject(self):
        cases=[((0,0,0),(5,0,0)),((0,2,0),(0,2,0)),((0,2,-1),(5,2,-1)),
               ((0,2,2),(5,2,1)),((0,2,1),(5,2,0)),((0,2,0),(2,0,0),(5,2,0)),
               ((0,float('nan'),0),(5,2,0))]
        for knots in cases:
            with self.assertRaises(ValueError):RadialProfile('invalid',(0,0),knots)
        with self.assertRaises(ValueError):RadialProfile('',(0,0),((0,1,0),(1,1,0)))
        with self.assertRaises(ValueError):RadialProfile('bad',(0,0),((0,1,0),(1,1,0)),(1,2,65536))
        with self.assertRaises(ValueError):radial_mesh(RadialProfile('unresolved',(1e12,1e12),((0,1e-10,0),(1,1e-10,0))))


if __name__=='__main__':unittest.main()
