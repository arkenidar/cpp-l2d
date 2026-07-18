#include "demo.hpp"

#include <algorithm>
#include <cstdio>

using l2d::Graphics;

// Builds a Viewport + Scene pair and pushes it on top of a stack —
// either the root list, or (when opts.parent is given) that viewport's
// children. blocksInput defaults to the input policy following the
// visuals: a viewport with a background image is input-opaque
// everywhere, one without lets empty areas fall through.
std::shared_ptr<Entry> DemoApp::addViewport(
    float x, float y, float w, float h,
    std::vector<std::unique_ptr<Drawable>> objects, bool useBackground,
    const AddOpts &opts) {
  bool blocksInput = opts.blocksInput.value_or(useBackground);

  // Background image, dynamically fit to the viewport's current size,
  // drawn fixed relative to the viewport's origin, unaffected by scroll.
  Scene::BackgroundFn backgroundFn;
  if (useBackground) {
    l2d::Image *image = &image_;
    backgroundFn = [image](Graphics &g, float w_, float h_) {
      auto [iw, ih] = image->getDimensions();
      float scale = std::min(w_ / iw, h_ / ih);
      float ix = (w_ - iw * scale) / 2;
      float iy = (h_ - ih * scale) / 2;
      g.setColor(1, 1, 1);
      g.draw(*image, ix, iy, 0, scale, scale);
    };
  }

  auto entry = std::make_shared<Entry>();
  entry->scene = std::make_unique<Scene>(engine_->graphics(), std::move(objects),
                                         std::move(backgroundFn));
  entry->viewport = std::make_unique<Viewport>(x, y, w, h, blocksInput);

  Scene *scene = entry->scene.get();
  entry->viewport->onClick = [scene](float cx, float cy) { scene->onClick(cx, cy); };
  entry->viewport->hitContent = [scene](float cx, float cy) {
    return scene->hitTestAt(cx, cy) != nullptr;
  };

  EntryList &list = opts.parent ? opts.parent->children : viewports_;
  list.push_back(entry);
  return entry;
}

void DemoApp::load(l2d::Engine &engine) {
  engine_ = &engine;
  std::printf("Hello World! debugger test\n");

  image_ = engine.graphics().newImage(l2d::Engine::basePath() +
                                      "assets/highres-photo-4000x3000.png");
  auto [ww, wh] = engine.graphics().getDimensions();

  // Bottom window: opaque photo background, so it blocks all input to
  // anything below its frame. The two overlapping rects demo scene-level
  // z: the z = 1 rect draws on top and wins clicks in the overlap.
  std::vector<std::unique_ptr<Drawable>> bottomObjects;
  bottomObjects.push_back(shapes::newRectButton({100, 100, 200, 150}));
  bottomObjects.push_back(shapes::newRectButton({200, 170, 200, 150, 1}));
  bottomObjects.push_back(shapes::newCircleButton({.cx = 400, .cy = 300}));
  bottomObjects.push_back(shapes::newDecorGroup());
  auto bottom = addViewport(ww * 0.05f, wh * 0.15f, ww * 0.45f, wh * 0.6f,
                            std::move(bottomObjects), true);

  // Nested demo: a child viewport living inside the bottom window's
  // content space — it scrolls/clips with its parent and still handles
  // its own drag, resize, and input independently.
  std::vector<std::unique_ptr<Drawable>> childObjects;
  childObjects.push_back(shapes::newRectButton({15, 15, 60, 40}));
  childObjects.push_back(shapes::newCircleButton({.cx = 100, .cy = 60, .sizeIndex = 1}));
  addViewport(10, 10, 150, 110, std::move(childObjects), false,
              {true, bottom->viewport.get()});

  // Top window: no background, overlapping the first one. Its empty
  // areas are input-transparent — clicks there fall through to the
  // window below, while drags from the same spot still pan this one.
  std::vector<std::unique_ptr<Drawable>> topObjects;
  topObjects.push_back(shapes::newCircleButton({.cx = 150, .cy = 150, .sizeIndex = 1}));
  topObjects.push_back(shapes::newRectButton({50, 250, 150, 100}));
  addViewport(ww * 0.35f, wh * 0.25f, ww * 0.45f, wh * 0.6f,
              std::move(topObjects), false);
}

void DemoApp::draw(Graphics &g) {
  // Re-renders each scene into its own buffer every frame (immediate
  // mode, no dirty-flag caching), then lets its viewport scroll/clip
  // the resulting buffer over its own background — recursing into any
  // nested children the same way.
  Viewport::drawStack(g, viewports_);
}

void DemoApp::keypressed(const std::string &key) {
  if (key == "escape") engine_->quitEvent();
}

void DemoApp::mousepressed(float x, float y, int button) {
  if (button != 1 || mouseCapture_) return;
  mouseCapture_ = capturePressAt(viewports_, x, y);
}

void DemoApp::mousemoved(float x, float y, float, float) {
  if (mouseCapture_) {
    auto [lx, ly] = toLocal(mouseCapture_->chain, x, y);
    mouseCapture_->entry->viewport->dragTo(lx, ly);
  }
}

void DemoApp::mousereleased(float x, float y, int button) {
  if (button != 1 || !mouseCapture_) return;
  auto [lx, ly] = toLocal(mouseCapture_->chain, x, y);
  // Sync one last time at the release position before ending the
  // gesture: a fast flick can release before/without a matching motion
  // sample, which would leave the drag frozen short of the pointer.
  mouseCapture_->entry->viewport->dragTo(lx, ly);
  releaseCapture(*mouseCapture_, lx, ly);
  mouseCapture_.reset();
}

// Wheel goes to the topmost viewport under the cursor that either has
// something there or has overflowing content to scroll.
void DemoApp::wheelmoved(float dx, float dy) {
  auto [mx, my] = engine_->mousePosition();
  wheelRoute(viewports_, mx, my, dx, dy, engine_->isShiftDown());
}

// Touch mirrors the mouse path, with an independent capture per touch
// id (a viewport already mid-gesture won't be claimed twice, thanks to
// the dragMode guard in capturePressAt).
void DemoApp::touchpressed(std::int64_t id, float x, float y) {
  auto capture = capturePressAt(viewports_, x, y);
  if (capture) touchCaptures_.emplace(id, std::move(*capture));
}

void DemoApp::touchmoved(std::int64_t id, float x, float y) {
  auto it = touchCaptures_.find(id);
  if (it != touchCaptures_.end()) {
    auto [lx, ly] = toLocal(it->second.chain, x, y);
    it->second.entry->viewport->dragTo(lx, ly);
  }
}

void DemoApp::touchreleased(std::int64_t id, float x, float y) {
  auto it = touchCaptures_.find(id);
  if (it != touchCaptures_.end()) {
    auto [lx, ly] = toLocal(it->second.chain, x, y);
    // See mousereleased: sync at the release position first.
    it->second.entry->viewport->dragTo(lx, ly);
    releaseCapture(it->second, lx, ly);
    touchCaptures_.erase(it);
  }
}

void DemoApp::resize(int w, int h) {
  std::printf("Window resized to: (%d, %d)\n", w, h);
}

void DemoApp::quit() { std::printf("Exiting the game...\n"); }
