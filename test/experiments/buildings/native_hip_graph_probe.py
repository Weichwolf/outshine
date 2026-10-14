"""Probe the same measured native graph reconstruction used by the library."""

import time

from outshine_ref.buildings.modelling.skeleton_graph import graph_data
from outshine_ref.buildings.modelling.skeleton_plan import (
    SkeletonLimits,face_boundaries,finish,input_data,
)


def prepare_graph(polygon,limits=SkeletonLimits()):
    began=time.perf_counter();origin,local,rings,boundaries=input_data(polygon,limits)
    data,metadata=graph_data(local,rings,limits.nodes)
    owners=face_boundaries(data[0],data[2],boundaries)
    plan=finish(origin,local,boundaries,data,'native-graph',limits,began,owners=owners)
    return plan,metadata
