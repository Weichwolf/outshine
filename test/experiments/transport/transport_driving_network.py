"""Keep one lane frame for each physical cut and solve only its affected modules."""

import hashlib
import json
from pathlib import Path

import numpy as np
import shapely

from transport_corridor_experiment import corridor_ports
from transport_directed_routes import directed_module_routes
from transport_flat_surfaces import load_graphs
from transport_junction_surfaces import mesh_surface_index,motor_surface,same_island_winding
from transport_junction_turn_experiment import solve_module
from transport_junction_turns import connect_lanes
from transport_port_lanes import junction_interfaces,module_face_index,module_shape,lane_poses,align_vehicle_ports
from transport_shared_frames import frame_key,heading_choices,choose_shared_frames,section_pose_domain,pose_cost
from transport_travel import MODES,bound_source_travel


class DrivingNetwork:
    def __init__(self,root,name,vehicle,*,motor_class='motorcar'):
        metadata=json.loads((root/'directed-graphs'/f'{name}-flat-network.json').read_text())
        source_data=(root/'inputs'/f'{name}-input.json').read_bytes()
        assert hashlib.sha256(source_data).hexdigest()==metadata['input_sha256'],'graph source input changed'
        recipes=json.loads(Path(__file__).with_name('transport_network_recipes.json').read_text())
        public,restricted,self.access_notes=bound_source_travel(json.loads(source_data)['roads'],
            metadata['source_ids'],metadata['usage'],recipes['travelRecipes'],motor_class=motor_class)
        graphs = load_graphs(root/'directed-graphs'/f'{name}-flat-network.npz',
                            metadata,travel_permissions=(public,restricted))
        self.motor_class=motor_class
        self.graph = next(g for g in graphs if g['tier']==['land',0,'ground'])
        self.tree = shapely.STRtree(self.graph['lines'])
        source = root/'directed-surfaces'/f'{name}-flat-surfaces.npz'
        with np.load(source) as archive:
            self.product = {key:archive[key] for key in archive.files}
        self.curbs = json.loads(source.with_suffix('.json').read_text())['curbs']
        self.vehicle,self.name = vehicle,name
        self.junctions = junction_interfaces(self.product)
        self.faces,self.mesh = module_face_index(self.product),mesh_surface_index(self.product)
        self.corridors = {}
        for junction,ports in self.junctions.items():
            for port in ports:
                self.corridors.setdefault(port['corridor'],[]).append((junction,port['section']))
        self.registry,self.anchors,self.owners,self.frames = {},{},{},{}
        self.shapes,self.records = {},{}
        self.original_poses,self.domains = {},{}
        self.sections = {p['section']:p['line'] for ports in self.junctions.values() for p in ports}

    def shape(self,module):
        if module not in self.shapes:
            self.shapes[module] = module_shape(self.product,module,self.faces)
        return self.shapes[module]

    def prepare_junction(self,module):
        if module in self.registry:
            return
        shape = self.shape(module)
        poses,unused = lane_poses(self.graph,self.junctions[module],shape,self.vehicle,tree=self.tree)
        for pose in poses:
            key = frame_key(pose)
            self.anchors[key] = pose['heading']
            self.owners[key] = {module,pose['corridor']}
        if poses:
            surface = motor_surface(shape,self.graph,self.tree,self.mesh,self.vehicle,
                                    self.curbs['corner_radius_m'],self.curbs['arc_error_m'])
            poses,_ = align_vehicle_ports(poses,surface,self.vehicle,self.curbs['arc_error_m'])
        self.registry[module] = poses,unused
        self.original_poses.update((frame_key(p),p) for p in poses)

    def poses(self,module,overrides):
        if module in self.junctions:
            self.prepare_junction(module)
            poses,unused = self.registry[module]
        else:
            interfaces = self.corridors[module]
            for junction,_ in interfaces:
                self.prepare_junction(junction)
            poses = corridor_ports(interfaces,{j:self.registry[j][0] for j,_ in interfaces})
            unused = []
        return [{**p,**self.frames.get(frame_key(p),{}),**overrides.get(frame_key(p),{})} for p in poses],unused

    def pose_options(self,pose):
        key = frame_key(pose)
        if key not in self.domains:
            origin = np.array(self.graph['lines'][pose['edge']].interpolate(pose['station']).coords[0])
            both = bool(np.all(self.graph['travel_public'][pose['edge']]&MODES['motor']!=0))
            lane = dict(section=self.sections[pose['section']],heading=self.anchors[key],origin=origin,
                        half_width=self.graph['half_widths'][pose['edge']],separated=both and not pose['shared_lane'])
            self.domains[key] = section_pose_domain(self.original_poses[key],lane,
                               [self.record(k)[1][1] for k in self.owners[key]],self.vehicle,self.curbs['arc_error_m'])
        return [(p,pose_cost(p,pose,self.vehicle)) for p in self.domains[key]]

    def solve(self,module,overrides=None):
        poses,unused = self.poses(module,{} if overrides is None else overrides)
        return solve_module(module,self.shape(module),poses,unused,self.graph,self.tree,
                            self.mesh,self.vehicle,self.curbs,adjust_ports=False)

    def record(self,module):
        if module not in self.records:
            self.records[module] = self.solve(module)
        return self.records[module]

    def improve_connection(self,module,entry_index,exit_index,maximum_candidates,recipe='heading'):
        report,view = self.record(module)
        shape,surface,poses,_,_ = view
        entry,exit = poses[entry_index],poses[exit_index]
        affected = set.union(*(self.owners[frame_key(p)] for p in (entry,exit)))
        before = {k:self.record(k)[0] for k in sorted(affected)}
        if recipe=='heading':
            choices = [[(dict(position=p['position'],heading=h),cost) for h,cost in
                        heading_choices(p,self.anchors[frame_key(p)],
                        [self.record(k)[1][1] for k in self.owners[frame_key(p)]],
                        self.vehicle,self.curbs['arc_error_m'])] for p in (entry,exit)]
        elif recipe=='position':
            choices = [self.pose_options(p) for p in (entry,exit)]
        else:
            raise ValueError('unknown frame recipe')
        routes,_ = directed_module_routes(self.graph,poses,shape,self.tree)
        def curve_fits(a,b):
            path,_ = connect_lanes(a,b,surface,self.vehicle,self.curbs['arc_error_m'])
            return path is not None and (not report['directed_island_winding'] or
                    same_island_winding(path,routes[entry_index,exit_index],surface))
        evaluated = {}
        def evaluate(overrides):
            evaluated.clear()
            evaluated.update((k,self.solve(k,overrides)) for k in sorted(affected))
            return {k:r[0] for k,r in evaluated.items()}
        overrides,_,stats = choose_shared_frames(entry,exit,choices,before,curve_fits,evaluate,maximum_candidates,
                                                 required_gain=(module,entry_index,exit_index))
        if overrides:
            self.frames.update(overrides)
            self.records.update(evaluated)
        stats.update(module=module,entry=entry_index,exit=exit_index,affected_modules=sorted(affected),recipe=recipe)
        return stats

    def improve(self,module,maximum_candidates=64,recipe='heading'):
        report,_ = self.record(module)
        failed = [(t['entry'],t['exit']) for t in report['turns'] if not t['accepted']]
        stats,remaining = [],maximum_candidates
        for index,(entry,exit) in enumerate(failed):
            current,_ = self.record(module)
            if any(t['entry']==entry and t['exit']==exit and t['accepted'] for t in current['turns']):
                continue
            if not remaining:
                break
            trial = self.improve_connection(module,entry,exit,max(1,remaining//(len(failed)-index)),recipe)
            remaining -= trial['tried']
            stats.append(trial)
        return stats
