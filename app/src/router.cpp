#include "router.hpp"

#include <algorithm>

void bringToFront(EntryList &list, const std::shared_ptr<Entry> &entry) {
  auto it = std::find(list.begin(), list.end(), entry);
  if (it != list.end()) {
    auto e = *it;
    list.erase(it);
    list.push_back(std::move(e));
  }
}

std::pair<float, float> toLocal(const std::vector<Viewport *> &chain, float x, float y) {
  for (Viewport *vp : chain)
    std::tie(x, y) = vp->toContent(x, y);
  return {x, y};
}

std::optional<Capture> capturePressAt(EntryList &list, float x, float y) {
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    // Copy the shared_ptr: bringToFront below reorders the vector,
    // which would invalidate a reference into it.
    auto entry = *it;
    Viewport *vp = entry->viewport.get();
    if (vp->dragMode != Viewport::DragMode::None) continue;

    // A viewport's own handles are drawn on top of its children, so a
    // press on one of them must not descend into children — otherwise
    // a child handle underneath would steal the press from the parent
    // handle covering it.
    bool onOwnHandle = vp->hitOrigin(x, y) || vp->hitResize(x, y);
    if (!onOwnHandle && vp->hitBody(x, y)) {
      auto [cx, cy] = vp->toContent(x, y);
      auto childCapture = capturePressAt(vp->children, cx, cy);
      if (childCapture) {
        childCapture->chain.insert(childCapture->chain.begin(), vp);
        return childCapture;
      }
    }
    Viewport::PressKind kind = vp->beginDrag(x, y);
    if (kind != Viewport::PressKind::None) {
      bringToFront(list, entry);
      Capture capture;
      capture.entry = entry;
      capture.list = &list;
      capture.tentative = (kind == Viewport::PressKind::PanTransparent);
      return capture;
    }
  }
  return std::nullopt;
}

void releaseCapture(Capture &capture, float x, float y) {
  Viewport *vp = capture.entry->viewport.get();
  if (capture.tentative && !vp->dragMoved) {
    vp->endDrag();
    EntryList &list = *capture.list;
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
      auto entry = *it; // copy; bringToFront below reorders the vector
      if (entry == capture.entry) continue;
      Viewport *below = entry->viewport.get();
      if (below->hitOrigin(x, y) || below->hitResize(x, y))
        return; // a handle consumes the click without any action
      Viewport::BodyKind kind = below->bodyInputKind(x, y);
      if (kind == Viewport::BodyKind::Content || kind == Viewport::BodyKind::Opaque) {
        bringToFront(list, entry);
        below->fireClickAt(x, y);
        return;
      }
    }
  } else {
    vp->maybeFireClick(x, y);
    vp->endDrag();
  }
}

bool wheelRoute(EntryList &list, float mx, float my, float dx, float dy,
                bool shiftDown) {
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    Viewport *vp = (*it)->viewport.get();
    if (vp->hitBody(mx, my)) {
      auto [cx, cy] = vp->toContent(mx, my);
      if (wheelRoute(vp->children, cx, cy, dx, dy, shiftDown)) return true;
      Viewport::BodyKind kind = vp->bodyInputKind(mx, my);
      if (kind == Viewport::BodyKind::Content || kind == Viewport::BodyKind::Opaque ||
          (kind == Viewport::BodyKind::Transparent && vp->canScroll())) {
        vp->wheelmoved(dx, dy, shiftDown);
        return true;
      }
    }
  }
  return false;
}
