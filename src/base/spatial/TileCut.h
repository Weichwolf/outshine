#ifndef OUTSHINE_BASE_SPATIAL_TILECUT_H
#define OUTSHINE_BASE_SPATIAL_TILECUT_H

namespace outshine {
enum class TileCutAction { Keep, Refine };

template <class Node, class Decide, class Children, class Keep>
void VisitTileCut(const Node &root, Decide &&decide, Children &&children, Keep &&keep) {
  const auto visit = [&](const auto &self, const Node &node) -> void {
    switch (decide(node)) {
      case TileCutAction::Keep: keep(node); return;
      case TileCutAction::Refine:
        for (const auto &child : children(node)) { self(self, child); }
        return;
    }
  };
  visit(visit, root);
}
}

#endif
