from pathlib import Path
exec(Path('test/experiments/transport/probes/roadbeds/coupled_deficit_probe.py').read_text().split('for module,pair in')[0])
from transport_hermite_route_fitting import fitted_route_candidates
from transport_fitted_roadbeds import spline_roadbed
from transport_spline_routes import checked_spline_path
from transport_source_roadbeds import sample_roadbed_piece
from transport_turn_roadbed_experiment import building_reservations,reservations,mesh_coverage
receipt=json.loads((root/'inputs'/f'{name}-input.json').read_text())['receipt']
for module,pair in ((1565,(4,8)),(1704,(0,5)),(1704,(3,5))):
    poses,_=net.poses(module,{})
    routes,_=directed_module_routes(net.graph,poses,net.shape(module),net.tree)
    route=routes[pair];error=net.curbs['arc_error_m']
    floor=motor_surface(route.envelope,net.graph,net.tree,net.mesh,net.vehicle,net.curbs['corner_radius_m'],error)
    region=route.envelope.buffer(12)
    buildings,_=building_reservations(receipt,region)
    reserved=buildings.union(reservations(net,region).difference(floor))
    nearby=reserved.intersection(floor.buffer(.85).envelope.buffer(.05))
    domain=floor.union(floor.buffer(.85,quad_segs=8).difference(nearby.difference(floor).buffer(.05,quad_segs=8)))
    path,note=fitted_route_candidates(poses[pair[0]],poses[pair[1]],route,domain,net.vehicle,error,include_controls=True)
    assert path is not None
    controls=np.array(note['curve_controls'])
    support,audit=spline_roadbed(controls,net.vehicle,error);assert support is not None
    clipped=floor.union(support.difference(reserved))
    checked,proof=checked_spline_path(controls,clipped,net.vehicle,error)
    piece=dict(profile='G2-quintic-spline',fit=dict(controls=controls,
          maximum_curvature_per_m=np.array([note['curvature_bound_per_m']])),begin=poses[pair[0]]['position'],end=poses[pair[1]]['position'])
    xy,heading=sample_roadbed_piece(piece,np.linspace(0,1,2049))
    bodies=shapely.polygons(body_outlines(xy,heading,.9,3.3,1.1))
    outside_reserved=reserved.difference(floor)
    overlap=shapely.area(shapely.intersection(bodies,outside_reserved))
    native=False
    if checked is not None:
        mesh_coverage(clipped,net.vehicle,[piece],error);native=True
    print(json.dumps(dict(module=module,pair=pair,reserved_construction_overlap_m2=support.difference(floor).intersection(reserved).area,
        sampled_body_reserved_overlap_max_m2=float(overlap.max()),clipped_proved=checked is not None,native_mesh_proved=native,
        reason=proof.get('reason'),sampled_poses=proof.get('vehicle_sweep',{}).get('sampled_poses'))),flush=True)
