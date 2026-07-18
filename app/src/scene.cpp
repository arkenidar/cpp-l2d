#include "scene.hpp"

#include <algorithm>

Scene::Scene(l2d::Graphics &gfx, std::vector<std::unique_ptr<Drawable>> objects,
             BackgroundFn background)
    : backgroundFn(std::move(background)), gfx_(gfx),
      sceneObjects_(std::move(objects)),
      canvas_(gfx.newCanvas(kCanvasCapacityW, kCanvasCapacityH)) {
  rebuildDrawOrder();
}

// Painter's-algorithm ordering: ascending z (default 0), insertion
// order breaking ties. Drawing walks this forward; hit testing walks it
// backward, so the object painted on top also receives input.
void Scene::rebuildDrawOrder() {
  drawOrder_.clear();
  drawOrder_.reserve(sceneObjects_.size());
  int i = 0;
  for (auto &obj : sceneObjects_) {
    obj->sceneIndex = i++;
    drawOrder_.push_back(obj.get());
  }
  std::stable_sort(drawOrder_.begin(), drawOrder_.end(),
                   [](const Drawable *a, const Drawable *b) { return a->z < b->z; });
}

void Scene::renderToCanvas() {
  // Rebuilt each frame so dynamic z changes and added/removed objects
  // just work (object counts are tiny).
  rebuildDrawOrder();
  // Render in a clean coordinate space: setCanvas does NOT reset the
  // current transform or scissor (LÖVE semantics), so a nested scene
  // rendered mid-frame (inside its parent's translated, scissored
  // content pass) would otherwise bake the parent's offset into the
  // canvas and clip against the parent's screen-space scissor.
  gfx_.push();
  gfx_.origin();
  gfx_.setScissor();
  gfx_.setCanvas(canvas_);
  gfx_.clear(0, 0, 0, 0);
  maxX_ = 0;
  maxY_ = 0;
  for (Drawable *obj : drawOrder_) {
    obj->draw(gfx_);
    auto [x2, y2] = obj->bounds();
    maxX_ = std::max(maxX_, x2);
    maxY_ = std::max(maxY_, y2);
  }
  gfx_.setCanvas();
  gfx_.pop();
}

void Scene::drawContent() {
  gfx_.setColor(1, 1, 1, 1);
  gfx_.draw(canvas_, 0, 0);
}

Drawable *Scene::hitTestAt(float cx, float cy) {
  for (auto it = drawOrder_.rbegin(); it != drawOrder_.rend(); ++it) {
    Drawable *obj = *it;
    if (obj->hittable() && obj->hitTest(cx, cy)) return obj;
  }
  return nullptr;
}

bool Scene::onClick(float cx, float cy) {
  Drawable *obj = hitTestAt(cx, cy);
  if (obj) {
    if (obj->clickable()) obj->onClick();
    return true;
  }
  return false;
}
