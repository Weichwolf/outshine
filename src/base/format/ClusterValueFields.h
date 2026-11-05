#ifndef OUTSHINE_BASE_FORMAT_CLUSTERVALUEFIELDS_H
#define OUTSHINE_BASE_FORMAT_CLUSTERVALUEFIELDS_H

namespace outshine {
template <class Archive, class Cluster> bool ClusterFields(Archive &io, Cluster &cluster) {
  return io(cluster.SelfCenter.Axis,
            cluster.SelfRadius,
            cluster.ParentCenter.Axis,
            cluster.ParentRadius,
            cluster.SelfErr,
            cluster.ParentErr,
            cluster.First,
            cluster.Count,
            cluster.Level);
}
}

#endif
