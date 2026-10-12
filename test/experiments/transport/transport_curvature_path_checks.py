"""Check periodic pose selection, batch isolation and reproducible G2 recipes."""

import numpy as np

from transport_curvature_paths import fit_curvature_paths,sample_curvature_paths
from transport_source_roadbeds import roadbed_piece


def verify_periodic_paths():
    start=np.array([[0.,0.],[275.00869659,533.12536203],[275.00869659,-533.12536203]])
    end=np.array([[10.,0.],[270.68602144,543.44350039],[270.68602144,-543.44350039]])
    initial=np.array([0.,np.arctan2(.3962626153510168,-.9181372117909022),
                      np.arctan2(-.3962626153510168,-.9181372117909022)])
    final=np.array([0.,np.arctan2(-.7566892306749863,.6537747381021215),
                    np.arctan2(.7566892306749863,.6537747381021215)])
    closest=fit_curvature_paths(start,end,initial,final)
    selected=fit_curvature_paths(start,end,initial,final,minimum_radius=4.)
    np.testing.assert_array_equal(selected['heading_turns'],[0,-1,1])
    assert np.all(selected['valid']) and np.all(selected['maximum_curvature_per_m']*4<=1)
    assert np.all(~closest['valid'][1:]|(closest['maximum_curvature_per_m'][1:]*4>1))
    for key in closest:np.testing.assert_array_equal(closest[key][0],selected[key][0])
    for values in (start,end,initial,final):values.setflags(write=False)
    readonly=fit_curvature_paths(start,end,initial,final,minimum_radius=4.)
    for key in selected:np.testing.assert_array_equal(selected[key],readonly[key])
    positions,headings,curvature=sample_curvature_paths(selected,np.array([0.,1.]))
    np.testing.assert_allclose(positions[:,0],start,atol=1e-7,rtol=0)
    np.testing.assert_allclose(positions[:,-1],end,atol=1e-7,rtol=0)
    np.testing.assert_allclose(np.cos(headings[:,[0,-1]]),np.cos(np.stack((initial,final),axis=1)),atol=1e-12,rtol=0)
    np.testing.assert_allclose(np.sin(headings[:,[0,-1]]),np.sin(np.stack((initial,final),axis=1)),atol=1e-12,rtol=0)
    np.testing.assert_allclose(curvature,0,atol=1e-12,rtol=0)
    for index,turns in enumerate(selected['heading_turns']):
        replayed=fit_curvature_paths(start[index:index+1],end[index:index+1],initial[index:index+1],
                                     final[index:index+1],heading_turns=int(turns))
        for key in selected:np.testing.assert_array_equal(selected[key][index:index+1],replayed[key])
        note=dict(profile='G2-polynomial',heading_turns=int(turns),
                  entry_pose=dict(position=start[index],heading=[np.cos(initial[index]),np.sin(initial[index])]),
                  exit_pose=dict(position=end[index],heading=[np.cos(final[index]),np.sin(final[index])]))
        piece=roadbed_piece(note)
        xy,angle,_=sample_curvature_paths(piece['fit'],np.linspace(0,1,33))
        reference=sample_curvature_paths(replayed,np.linspace(0,1,33))
        np.testing.assert_allclose(xy,reference[0],atol=1e-12,rtol=0)
        np.testing.assert_allclose(np.sin(angle),np.sin(reference[1]),atol=1e-12,rtol=0)
    return dict(periodic_turns_checked=True,stored_angle_branch_replayed=True,
                valid_short_turn_unchanged=True,mixed_branches_checked=True,readonly_poses_checked=True)
