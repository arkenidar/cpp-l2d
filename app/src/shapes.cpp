#include "shapes.hpp"

namespace shapes {

using l2d::DrawMode;
using l2d::Graphics;

namespace {

class RectButton : public Drawable {
public:
  explicit RectButton(const RectButtonOpts &o) : o_(o) { z = o.z; }

  void draw(Graphics &g) override {
    g.setColor(isRed_ ? 1.0f : 0.0f, 0, isRed_ ? 0.0f : 1.0f);
    g.rectangle(DrawMode::Fill, o_.x, o_.y, o_.w, o_.h);
    g.setColor(1, 1, 1);
    g.setLineWidth(2);
    g.rectangle(DrawMode::Line, o_.x, o_.y, o_.w, o_.h);
  }

  std::pair<float, float> bounds() override { return {o_.x + o_.w, o_.y + o_.h}; }

  bool hittable() const override { return true; }
  bool hitTest(float px, float py) override {
    return px >= o_.x && px <= o_.x + o_.w && py >= o_.y && py <= o_.y + o_.h;
  }

  bool clickable() const override { return true; }
  void onClick() override { isRed_ = !isRed_; }

private:
  RectButtonOpts o_;
  bool isRed_ = true;
};

class CircleButton : public Drawable {
public:
  explicit CircleButton(const CircleButtonOpts &o) : o_(o) { z = o.z; }

  float radius() const { return o_.sizes[o_.sizeIndex - 1]; }

  void draw(Graphics &g) override {
    g.setColor(0, 0, 1);
    g.circle(DrawMode::Fill, o_.cx, o_.cy, radius());
    g.setColor(1, 1, 1);
    g.setLineWidth(2);
    g.circle(DrawMode::Line, o_.cx, o_.cy, radius());
  }

  std::pair<float, float> bounds() override {
    return {o_.cx + radius(), o_.cy + radius()};
  }

  bool hittable() const override { return true; }
  bool hitTest(float px, float py) override {
    float dx = px - o_.cx, dy = py - o_.cy;
    return dx * dx + dy * dy <= radius() * radius();
  }

  bool clickable() const override { return true; }
  void onClick() override {
    o_.sizeIndex = (o_.sizeIndex % (int)o_.sizes.size()) + 1;
  }

private:
  CircleButtonOpts o_;
};

class CoverRect : public Drawable {
public:
  explicit CoverRect(const CoverRectOpts &o) : o_(o) { z = o.z; }

  void draw(Graphics &g) override {
    g.setColor(o_.color);
    g.rectangle(DrawMode::Fill, o_.x, o_.y, o_.w, o_.h);
  }

  std::pair<float, float> bounds() override { return {o_.x + o_.w, o_.y + o_.h}; }

  bool hittable() const override { return true; }
  bool hitTest(float px, float py) override {
    return px >= o_.x && px <= o_.x + o_.w && py >= o_.y && py <= o_.y + o_.h;
  }
  // No onClick: consumes clicks without doing anything.

private:
  CoverRectOpts o_;
};

class DecorGroup : public Drawable {
public:
  void draw(Graphics &g) override {
    g.setColor(1, 1, 1);
    g.print("Hello, L\xC3\x96VE!", 150, 200);
    g.setColor(0, 1, 0);
    g.line(500, 100, 600, 200);
    g.setColor(1, 1, 0);
    g.polygon(DrawMode::Fill, {700, 100, 750, 150, 700, 200, 650, 150});
  }

  std::pair<float, float> bounds() override { return {750, 200}; }
};

} // namespace

std::unique_ptr<Drawable> newRectButton(const RectButtonOpts &opts) {
  return std::make_unique<RectButton>(opts);
}

std::unique_ptr<Drawable> newCircleButton(const CircleButtonOpts &opts) {
  return std::make_unique<CircleButton>(opts);
}

std::unique_ptr<Drawable> newCoverRect(const CoverRectOpts &opts) {
  return std::make_unique<CoverRect>(opts);
}

std::unique_ptr<Drawable> newDecorGroup() {
  return std::make_unique<DecorGroup>();
}

} // namespace shapes
