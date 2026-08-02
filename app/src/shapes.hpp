// Demo scene objects, transcribed from l2d-im/shapes.lua.
//
// Every shape implements the per-drawable protocol used by Scene:
// draw() and bounds() (required), plus optional hitTest/onClick if it
// should react to clicks (hittable/clickable mirror the Lua's
// "does the object have a hitTest/onClick field at all" checks), and a
// z (default 0) controlling paint order — higher z draws on top and is
// hit-tested first.
#pragma once

#include <l2d/graphics.hpp>

#include <memory>
#include <utility>
#include <vector>

class Drawable {
public:
  virtual ~Drawable() = default;
  virtual void draw(l2d::Graphics &g) = 0;
  virtual std::pair<float, float> bounds() = 0; // (maxX, maxY)
  virtual bool hittable() const { return false; }
  virtual bool hitTest(float, float) { return false; }
  virtual bool clickable() const { return false; }
  virtual void onClick() {}

  float z = 0;
  int sceneIndex = 0; // insertion-order tiebreak for the stable z-sort
};

namespace shapes {

struct RectButtonOpts {
  float x = 0, y = 0, w = 0, h = 0, z = 0;
};

struct CircleButtonOpts {
  float cx = 0, cy = 0, z = 0;
  std::vector<float> sizes = {30, 50, 70};
  int sizeIndex = 2; // 1-based, like the Lua
};

struct CoverRectOpts {
  float x = 0, y = 0, w = 0, h = 0, z = -1;
  l2d::Color color{0.15f, 0.15f, 0.2f, 0.6f};
};

// Text/line/polygon decor block, anchored at (x, y) and scaled by
// `scale` so callers can fit it to their own viewport size.
struct DecorGroupOpts {
  float x = 0, y = 0, scale = 1;
};

// Single shape, clickable: toggles red/blue.
std::unique_ptr<Drawable> newRectButton(const RectButtonOpts &opts);

// Single shape, clickable: cycles its own radius on click.
std::unique_ptr<Drawable> newCircleButton(const CircleButtonOpts &opts);

// Inert full-area cover: hit-testable but does nothing on click.
std::unique_ptr<Drawable> newCoverRect(const CoverRectOpts &opts);

// Group of shapes as one object; non-interactive.
std::unique_ptr<Drawable> newDecorGroup(const DecorGroupOpts &opts = {});

} // namespace shapes
