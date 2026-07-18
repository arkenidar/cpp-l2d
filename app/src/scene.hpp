// Scene: a canvas-backed, click-dispatching collection of drawables,
// transcribed from l2d-im/scene.lua. Owns its own offscreen canvas,
// renders its objects into it each frame (immediate mode, no dirty-flag
// caching), tracks the real bounding box of what was drawn, and
// dispatches clicks to whichever scene object was hit.
#pragma once

#include <l2d/graphics.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "shapes.hpp"

class Scene {
public:
  using BackgroundFn = std::function<void(l2d::Graphics &, float, float)>;

  Scene(l2d::Graphics &gfx, std::vector<std::unique_ptr<Drawable>> objects,
        BackgroundFn backgroundFn);

  // Re-renders every scene object into the canvas (bottom to top by z)
  // and re-derives the content bounding box from what was drawn.
  void renderToCanvas();

  std::pair<float, float> contentSize() const { return {maxX_, maxY_}; }

  // Blits the canvas (already holding this frame's fresh render) as
  // viewport content, with transparency.
  void drawContent();

  // First-hit test in top-down z order; nullptr for an empty spot. Any
  // hit counts as "content here", cover shapes included.
  Drawable *hitTestAt(float cx, float cy);

  // Dispatches a content-space click to the topmost hit object. True
  // when an object consumed the click (even one without onClick).
  bool onClick(float cx, float cy);

  BackgroundFn backgroundFn;

private:
  void rebuildDrawOrder();

  // Generous fixed allocation (LÖVE requires an explicit size up
  // front); the meaningful content size is the tracked bbox.
  static constexpr int kCanvasCapacityW = 2048;
  static constexpr int kCanvasCapacityH = 2048;

  l2d::Graphics &gfx_;
  std::vector<std::unique_ptr<Drawable>> sceneObjects_;
  std::vector<Drawable *> drawOrder_;
  l2d::Canvas canvas_;
  float maxX_ = 0, maxY_ = 0;
};
