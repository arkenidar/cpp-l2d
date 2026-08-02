#include "demo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using l2d::Graphics;

namespace {
const char *const kTabLabels[] = {"Photo", "Nested", "Overlay"};
constexpr float kMinTabBarHeight = 56; // comfortable touch-target height
constexpr float kBaseFontSize = 13;    // matches l2d's baked atlas size
} // namespace

// Builds a Viewport + Scene pair and pushes it on top of a stack —
// either the given tab's root list, or (when opts.parent is given) that
// viewport's children. blocksInput defaults to the input policy
// following the visuals: a viewport with a background image is
// input-opaque everywhere, one without lets empty areas fall through.
std::shared_ptr<Entry> DemoApp::addViewport(
    EntryList &list, float x, float y, float w, float h,
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
  entry->viewport = std::make_unique<Viewport>(x, y, w, h, blocksInput, handleScale_);

  Scene *scene = entry->scene.get();
  entry->viewport->onClick = [scene](float cx, float cy) { scene->onClick(cx, cy); };
  entry->viewport->hitContent = [scene](float cx, float cy) {
    return scene->hitTestAt(cx, cy) != nullptr;
  };

  EntryList &target = opts.parent ? opts.parent->children : list;
  target.push_back(entry);
  return entry;
}

void DemoApp::load(l2d::Engine &engine) {
  engine_ = &engine;
  std::printf("Hello World! debugger test\n");

  image_ = engine.graphics().newImage(l2d::Engine::basePath() +
                                      "assets/highres-photo-4000x3000.png");
  auto [ww, wh] = engine.graphics().getDimensions();
  layout(ww, wh);
}

float DemoApp::tabBarHeight(float wh) const {
  return std::max(wh * 0.08f, kMinTabBarHeight);
}

void DemoApp::layout(float ww, float wh) {
  // Drop any in-flight drag/click captures first: they hold
  // shared_ptr<Entry>/raw Viewport* pointers into stacks that are about
  // to be torn down and rebuilt below.
  mouseCapture_.reset();
  touchCaptures_.clear();
  for (auto &tab : tabs_) tab.clear();

  // Below this window size, treat the screen as phone-sized and double
  // the origin/resize handles so they stay comfortable touch targets.
  constexpr float kMobileBreakpoint = 600;
  handleScale_ = std::min(ww, wh) < kMobileBreakpoint ? 2.0f : 1.0f;

  float tbh = tabBarHeight(wh);
  float sw = ww;              // each tab's screen fills the full width
  float sh = wh - tbh;        // and the height below the tab bar

  // Every top-level screen's origin/resize handles sit centered on its
  // (0,0) and (w,h) corners (Viewport::draw, viewport.cpp), so content
  // is laid out inside a margin-inset sub-rectangle that keeps clear of
  // both handles — otherwise buttons placed near a corner render behind
  // (and are hard to click without instead grabbing) the handle there.
  // Scaled to screen size (with a floor at the handle's own clearance
  // need) so it doesn't eat a disproportionate chunk of a narrow phone
  // screen's width the way a flat pixel margin would.
  float handleClearance =
      std::max(Viewport::HANDLE_SIZE, Viewport::HANDLE_RADIUS) * handleScale_ * 0.7f;
  float margin = std::max(std::min(sw, sh) * 0.08f, handleClearance);
  float iw = std::max(sw - 2 * margin, 1.0f);
  float ih = std::max(sh - 2 * margin, 1.0f);

  // Tab 0: full-screen photo-background demo. Opaque background, so it
  // blocks all input to anything below its frame. The two overlapping
  // rects demo scene-level z: the z = 1 rect draws on top and wins
  // clicks in the overlap. All child object coordinates are fractions
  // of this screen's own inset content area, so the layout holds
  // together whether the window ends up wide (desktop) or narrow/tall
  // (phone portrait), and nothing sits under a corner handle.
  // Rect buttons are sized off a common unit — the geometric mean of
  // iw/ih, not iw and ih independently — so they read as squarish
  // buttons instead of stretching into thin bars on a wide screen.
  // Geometric mean (rather than min) also keeps the unit from
  // collapsing to the narrow axis on a tall phone-portrait screen,
  // where plain min(iw, ih) would shrink everything to fit the width.
  float pu = std::sqrt(iw * ih); // "photo" screen's sizing unit
  std::vector<std::unique_ptr<Drawable>> photoObjects;
  photoObjects.push_back(shapes::newRectButton(
      {margin + iw * 0.1f, margin + ih * 0.15f, pu * 0.35f, pu * 0.3f}));
  photoObjects.push_back(shapes::newRectButton(
      {margin + iw * 0.1f + pu * 0.2f, margin + ih * 0.15f + pu * 0.2f, pu * 0.35f,
       pu * 0.3f, 1}));
  float photoCircleR = pu * 0.15f;
  photoObjects.push_back(shapes::newCircleButton(
      {.cx = margin + iw * 0.65f, .cy = margin + ih * 0.55f,
       .sizes = {photoCircleR * 0.6f, photoCircleR, photoCircleR * 1.4f}}));
  photoObjects.push_back(shapes::newDecorGroup(
      {margin + iw * 0.15f, margin + ih * 0.75f, pu * 0.002f}));
  auto photoScreen = addViewport(tabs_[0], 0, tbh, sw, sh,
                                 std::move(photoObjects), true);

  // Nested child inside tab 0's screen — still demonstrates a viewport
  // nested inside another, scrolling/clipping with its parent while
  // handling its own drag/resize/input independently, just sized
  // relative to the now much larger parent screen. Centered, so its own
  // handles stay clear of the parent's.
  float cw = pu * 0.6f, ch = pu * 0.4f;
  float cu = std::sqrt(cw * ch);
  std::vector<std::unique_ptr<Drawable>> childObjects;
  childObjects.push_back(
      shapes::newRectButton({cw * 0.1f, ch * 0.15f, cu * 0.4f, cu * 0.35f}));
  float cCircleR = cu * 0.35f;
  childObjects.push_back(shapes::newCircleButton(
      {.cx = cw * 0.65f, .cy = ch * 0.55f,
       .sizes = {cCircleR * 0.6f, cCircleR, cCircleR * 1.4f}, .sizeIndex = 1}));
  addViewport(tabs_[0], (sw - cw) / 2, (sh - ch) / 2, cw, ch,
              std::move(childObjects), false,
              {true, photoScreen->viewport.get()});

  // Tab 1: a standalone full-screen version of the nested-content demo
  // (small rect + circle button), so it's reachable on its own screen.
  std::vector<std::unique_ptr<Drawable>> nestedObjects;
  nestedObjects.push_back(shapes::newRectButton(
      {margin + iw * 0.1f, margin + ih * 0.15f, pu * 0.4f, pu * 0.35f}));
  float nestedCircleR = pu * 0.2f;
  nestedObjects.push_back(shapes::newCircleButton(
      {.cx = margin + iw * 0.65f, .cy = margin + ih * 0.55f,
       .sizes = {nestedCircleR * 0.6f, nestedCircleR, nestedCircleR * 1.4f},
       .sizeIndex = 1}));
  addViewport(tabs_[1], 0, tbh, sw, sh, std::move(nestedObjects), false);

  // Tab 2: full-screen version of the "overlay" demo — no background,
  // so empty areas are input-transparent (there's nothing beneath it on
  // its own screen, but the behavior still demos the transparent-body
  // input policy).
  std::vector<std::unique_ptr<Drawable>> overlayObjects;
  float overlayCircleR = pu * 0.2f;
  overlayObjects.push_back(shapes::newCircleButton(
      {.cx = margin + iw * 0.3f, .cy = margin + ih * 0.3f,
       .sizes = {overlayCircleR * 0.6f, overlayCircleR, overlayCircleR * 1.4f},
       .sizeIndex = 1}));
  overlayObjects.push_back(shapes::newRectButton(
      {margin + iw * 0.1f, margin + ih * 0.5f, pu * 0.3f, pu * 0.25f}));
  addViewport(tabs_[2], 0, tbh, sw, sh, std::move(overlayObjects), false);
}

void DemoApp::drawTabBar(Graphics &g, float ww, float wh) {
  float tbh = tabBarHeight(wh);
  float tabW = ww / kTabCount;
  // Scale text well above the baked 13px atlas size for legibility on a
  // phone screen; ~40% of the bar height reads clearly without
  // crowding it.
  float textScale = std::max(tbh * 0.4f / kBaseFontSize, 1.0f);
  for (int i = 0; i < kTabCount; ++i) {
    float x = i * tabW;
    bool active = i == activeTab_;
    g.setColor(active ? 0.3f : 0.15f, active ? 0.3f : 0.15f, active ? 0.35f : 0.18f);
    g.rectangle(l2d::DrawMode::Fill, x, 0, tabW, tbh);
    g.setColor(1, 1, 1);
    g.setLineWidth(1);
    g.rectangle(l2d::DrawMode::Line, x, 0, tabW, tbh);
    // print()'s y is the text baseline, not its top, so vertical
    // centering needs the baseline offset down from the box's vertical
    // center by roughly the font's ascent (~0.7em for Vera Sans).
    float textW = (float)std::strlen(kTabLabels[i]) * kBaseFontSize * 0.55f * textScale;
    float baselineY = tbh * 0.5f + kBaseFontSize * textScale * 0.35f;
    g.print(kTabLabels[i], x + (tabW - textW) * 0.5f, baselineY, textScale);
  }
}

bool DemoApp::handleTabBarPress(float x, float y, float ww, float wh) {
  float tbh = tabBarHeight(wh);
  if (y >= tbh) return false;
  int tab = std::clamp((int)(x / (ww / kTabCount)), 0, kTabCount - 1);
  if (tab != activeTab_) {
    mouseCapture_.reset();
    touchCaptures_.clear();
    activeTab_ = tab;
  }
  return true;
}

void DemoApp::draw(Graphics &g) {
  // Re-renders each scene into its own buffer every frame (immediate
  // mode, no dirty-flag caching), then lets its viewport scroll/clip
  // the resulting buffer over its own background — recursing into any
  // nested children the same way. Only the active tab is drawn.
  Viewport::drawStack(g, tabs_[activeTab_]);
  auto [ww, wh] = engine_->graphics().getDimensions();
  drawTabBar(g, ww, wh);
}

void DemoApp::keypressed(const std::string &key) {
  if (key == "escape") engine_->quitEvent();
}

void DemoApp::mousepressed(float x, float y, int button) {
  if (button != 1 || mouseCapture_) return;
  auto [ww, wh] = engine_->graphics().getDimensions();
  if (handleTabBarPress(x, y, ww, wh)) return;
  mouseCapture_ = capturePressAt(tabs_[activeTab_], x, y);
}

void DemoApp::mousemoved(float x, float y, float, float) {
  if (mouseCapture_) {
    auto [lx, ly] = toLocal(mouseCapture_->chain, x, y);
    chainDrag(*mouseCapture_, lx, ly);
  }
}

void DemoApp::mousereleased(float x, float y, int button) {
  if (button != 1 || !mouseCapture_) return;
  auto [lx, ly] = toLocal(mouseCapture_->chain, x, y);
  // Sync one last time at the release position before ending the
  // gesture: a fast flick can release before/without a matching motion
  // sample, which would leave the drag frozen short of the pointer.
  chainDrag(*mouseCapture_, lx, ly);
  releaseCapture(*mouseCapture_, lx, ly);
  mouseCapture_.reset();
}

// Wheel goes to the topmost viewport under the cursor that either has
// something there or has overflowing content to scroll.
void DemoApp::wheelmoved(float dx, float dy) {
  auto [mx, my] = engine_->mousePosition();
  wheelRoute(tabs_[activeTab_], mx, my, dx, dy, engine_->isShiftDown());
}

// Touch mirrors the mouse path, with an independent capture per touch
// id (a viewport already mid-gesture won't be claimed twice, thanks to
// the dragMode guard in capturePressAt).
void DemoApp::touchpressed(std::int64_t id, float x, float y) {
  auto [ww, wh] = engine_->graphics().getDimensions();
  if (handleTabBarPress(x, y, ww, wh)) return;
  auto capture = capturePressAt(tabs_[activeTab_], x, y);
  if (capture) touchCaptures_.emplace(id, std::move(*capture));
}

void DemoApp::touchmoved(std::int64_t id, float x, float y) {
  auto it = touchCaptures_.find(id);
  if (it != touchCaptures_.end()) {
    auto [lx, ly] = toLocal(it->second.chain, x, y);
    chainDrag(it->second, lx, ly);
  }
}

void DemoApp::touchreleased(std::int64_t id, float x, float y) {
  auto it = touchCaptures_.find(id);
  if (it != touchCaptures_.end()) {
    auto [lx, ly] = toLocal(it->second.chain, x, y);
    // See mousereleased: sync at the release position first.
    chainDrag(it->second, lx, ly);
    releaseCapture(it->second, lx, ly);
    touchCaptures_.erase(it);
  }
}

void DemoApp::resize(int w, int h) {
  std::printf("Window resized to: (%d, %d)\n", w, h);
  layout((float)w, (float)h);
}

void DemoApp::quit() { std::printf("Exiting the game...\n"); }
