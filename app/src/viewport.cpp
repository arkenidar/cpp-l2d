#include "viewport.hpp"

#include <algorithm>

using l2d::DrawMode;
using l2d::Graphics;

Viewport::Viewport(float x_, float y_, float w_, float h_, bool blocksInput_,
                   float handleScale)
    : x(x_), y(y_), w(w_), h(h_), handleSize(HANDLE_SIZE * handleScale),
      handleRadius(HANDLE_RADIUS * handleScale), blocksInput(blocksInput_),
      contentW(w_), contentH(h_) {}

Viewport::BodyKind Viewport::bodyInputKind(float px, float py) const {
  if (!hitBody(px, py)) return BodyKind::Outside;
  if (hitContent) {
    auto [cx, cy] = toContent(px, py);
    if (hitContent(cx, cy)) return BodyKind::Content;
  }
  if (blocksInput) return BodyKind::Opaque;
  return BodyKind::Transparent;
}

void Viewport::setContentSize(float cw, float ch) {
  contentW = cw;
  contentH = ch;
  clampScroll();
}

bool Viewport::hitOrigin(float mx, float my) const {
  float half = handleSize / 2;
  return mx >= x - half && mx <= x + half && my >= y - half && my <= y + half;
}

bool Viewport::hitResize(float mx, float my) const {
  float cx = x + w, cy = y + h;
  float dx = mx - cx, dy = my - cy;
  return dx * dx + dy * dy <= handleRadius * handleRadius;
}

bool Viewport::hitBody(float mx, float my) const {
  return mx >= x && mx <= x + w && my >= y && my <= y + h;
}

std::pair<float, float> Viewport::childrenExtent() const {
  float maxX = 0, maxY = 0;
  for (auto &child : children) {
    maxX = std::max(maxX, child->viewport->x + child->viewport->w);
    maxY = std::max(maxY, child->viewport->y + child->viewport->h);
  }
  return {maxX, maxY};
}

void Viewport::clampScroll() {
  float maxScrollX = std::max(0.0f, contentW - w);
  float maxScrollY = std::max(0.0f, contentH - h);
  scrollX = std::max(0.0f, std::min(scrollX, maxScrollX));
  scrollY = std::max(0.0f, std::min(scrollY, maxScrollY));
}

Viewport::PressKind Viewport::beginDrag(float px, float py) {
  PressKind kind = PressKind::None;
  if (hitOrigin(px, py)) {
    dragMode = DragMode::Move;
    kind = PressKind::Move;
    // Snap the handle to the click point rather than preserving
    // wherever within its hit-tolerance radius the press landed, so it
    // stays centered under the cursor for the whole drag instead of
    // rigidly keeping the initial grab offset.
    x = px;
    y = py;
  } else if (hitResize(px, py)) {
    dragMode = DragMode::Resize;
    kind = PressKind::Resize;
    w = std::max(minW, px - x);
    h = std::max(minH, py - y);
    clampScroll();
  } else {
    switch (bodyInputKind(px, py)) {
    case BodyKind::Content:
      dragMode = DragMode::Pan;
      kind = PressKind::PanContent;
      break;
    case BodyKind::Opaque:
      dragMode = DragMode::Pan;
      kind = PressKind::PanOpaque;
      break;
    case BodyKind::Transparent:
      dragMode = DragMode::Pan;
      kind = PressKind::PanTransparent;
      break;
    case BodyKind::Outside:
      dragMode = DragMode::None;
      break;
    }
  }
  lastX = px;
  lastY = py;
  dragStartX = px;
  dragStartY = py;
  dragMoved = false;
  return kind;
}

std::pair<float, float> Viewport::dragTo(float px, float py) {
  if (dragMode == DragMode::None) return {0, 0};
  float dx = px - lastX, dy = py - lastY;
  lastX = px;
  lastY = py;

  if (!dragMoved) {
    float sdx = px - dragStartX, sdy = py - dragStartY;
    if (sdx * sdx + sdy * sdy > CLICK_MOVE_THRESHOLD * CLICK_MOVE_THRESHOLD)
      dragMoved = true;
  }

  if (dragMode == DragMode::Move) {
    x += dx;
    y += dy;
  } else if (dragMode == DragMode::Resize) {
    float newW = std::max(minW, w + dx);
    float newH = std::max(minH, h + dy);
    // Bank whatever part of the raw mouse delta the min-size clamp
    // didn't apply, so lastX/lastY don't outrun w/h — otherwise
    // reversing direction after hitting the minimum needs to first
    // retrace the unapplied slack before the handle starts moving again.
    lastX -= (w + dx) - newW;
    lastY -= (h + dy) - newH;
    w = newW;
    h = newH;
    clampScroll();
  } else if (dragMode == DragMode::Pan) {
    return panBy(dx, dy);
  }
  return {0, 0};
}

std::pair<float, float> Viewport::panBy(float dx, float dy) {
  float oldX = scrollX, oldY = scrollY;
  scrollX -= dx;
  scrollY -= dy;
  clampScroll();
  float appliedX = oldX - scrollX;
  float appliedY = oldY - scrollY;
  return {dx - appliedX, dy - appliedY};
}

bool Viewport::fireClickAt(float px, float py) {
  if (onClick && hitBody(px, py)) {
    auto [cx, cy] = toContent(px, py);
    onClick(cx, cy);
    return true;
  }
  return false;
}

void Viewport::maybeFireClick(float px, float py) {
  if (dragMode == DragMode::Pan && !dragMoved) fireClickAt(px, py);
}

void Viewport::wheelmoved(float /*dx*/, float dy, bool shiftDown) {
  bool horizontal = shiftDown || (contentH <= h && contentW > w);
  if (horizontal)
    scrollX -= dy * 30;
  else
    scrollY -= dy * 30;
  clampScroll();
}

void Viewport::drawStack(Graphics &g, EntryList &entries) {
  for (auto &entry : entries) {
    entry->scene->renderToCanvas();
    auto [cw, ch] = entry->scene->contentSize();
    auto [childW, childH] = entry->viewport->childrenExtent();
    entry->viewport->setContentSize(std::max(cw, childW), std::max(ch, childH));
    Scene *scene = entry->scene.get();
    entry->viewport->draw(
        g, [scene](float, float) { scene->drawContent(); },
        scene->backgroundFn);
  }
}

// Scissor rectangles are always in absolute render-target pixels,
// ignoring the current transform, so x/y (which may already be inside
// an ancestor viewport's translated content-space) are converted with
// transformPoint first. intersectScissor composes with any ancestor
// clip, and push/pop restores that ancestor clip afterwards.
void Viewport::draw(Graphics &g, const std::function<void(float, float)> &contentFn,
                    const Scene::BackgroundFn &backgroundFn) {
  g.push();
  auto [sx, sy] = g.transformPoint(x, y);
  g.intersectScissor(sx, sy, w, h);
  g.translate(x, y);

  if (backgroundFn) backgroundFn(g, w, h);

  g.translate(-scrollX, -scrollY);
  contentFn(w, h);
  drawStack(g, children);

  g.pop();

  // Frame border.
  g.setColor(1, 1, 1);
  g.rectangle(DrawMode::Line, x, y, w, h);

  // Origin square handle.
  float half = handleSize / 2;
  g.setColor(0.9f, 0.6f, 0.1f);
  g.rectangle(DrawMode::Fill, x - half, y - half, handleSize, handleSize);
  g.setColor(0, 0, 0);
  g.setLineWidth(2);
  g.rectangle(DrawMode::Line, x - half, y - half, handleSize, handleSize);

  // Resize circle handle.
  g.setColor(0.1f, 0.7f, 0.9f);
  g.circle(DrawMode::Fill, x + w, y + h, handleRadius);
  g.setColor(0, 0, 0);
  g.setLineWidth(2);
  g.circle(DrawMode::Line, x + w, y + h, handleRadius);

  g.setColor(1, 1, 1);
}
