// The l2d-im demo application, transcribed from l2d-im/main.lua.
#pragma once

#include <l2d/engine.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>

#include "router.hpp"

class DemoApp : public l2d::AppBase {
public:
  void load(l2d::Engine &engine) override;
  void draw(l2d::Graphics &g) override;
  void keypressed(const std::string &key) override;
  void mousepressed(float x, float y, int button) override;
  void mousemoved(float x, float y, float dx, float dy) override;
  void mousereleased(float x, float y, int button) override;
  void wheelmoved(float dx, float dy) override;
  void touchpressed(std::int64_t id, float x, float y) override;
  void touchmoved(std::int64_t id, float x, float y) override;
  void touchreleased(std::int64_t id, float x, float y) override;
  void resize(int w, int h) override;
  void quit() override;

private:
  static constexpr int kTabCount = 3;

  struct AddOpts {
    // Explicit constructors sidestep a GCC quirk with brace-defaulted
    // nested aggregates as default arguments.
    AddOpts() : parent(nullptr) {}
    AddOpts(std::optional<bool> blocks, Viewport *parentVp)
        : blocksInput(blocks), parent(parentVp) {}
    std::optional<bool> blocksInput;
    Viewport *parent;
  };
  std::shared_ptr<Entry> addViewport(EntryList &list, float x, float y, float w,
                                     float h,
                                     std::vector<std::unique_ptr<Drawable>> objects,
                                     bool useBackground, const AddOpts &opts = {});

  // Builds every tab's content at the given window size. Clears any
  // existing viewports/captures first, so it doubles as the relayout
  // step on resize.
  void layout(float ww, float wh);

  // Height of the tab bar strip, given the current window height.
  float tabBarHeight(float wh) const;

  // Draws the tab bar into the top strip of the window.
  void drawTabBar(l2d::Graphics &g, float ww, float wh);

  // If (x, y) lands in the tab bar, switches the active tab (dropping
  // any in-flight capture) and returns true — callers should treat the
  // press as consumed and not also route it into the viewport stack.
  bool handleTabBarPress(float x, float y, float ww, float wh);

  l2d::Engine *engine_ = nullptr;
  l2d::Image image_;

  // Set each layout() from window size; multiplies new viewports'
  // origin/resize handle sizes (doubled on phone-sized screens).
  float handleScale_ = 1.0f;

  // One independent viewport stack per tab; only tabs_[activeTab_] is
  // drawn and receives input. Kept alive (not rebuilt) across tab
  // switches so per-tab state (button toggles, scroll position)
  // persists; only a resize rebuilds every tab.
  std::array<EntryList, kTabCount> tabs_;
  int activeTab_ = 0;

  // Active pointer captures, scoped to whichever tab was active when
  // the gesture began.
  std::optional<Capture> mouseCapture_;
  std::unordered_map<std::int64_t, Capture> touchCaptures_;
};
