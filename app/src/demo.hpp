// The l2d-im demo application, transcribed from l2d-im/main.lua.
#pragma once

#include <l2d/engine.hpp>

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
  struct AddOpts {
    // Explicit constructors sidestep a GCC quirk with brace-defaulted
    // nested aggregates as default arguments.
    AddOpts() : parent(nullptr) {}
    AddOpts(std::optional<bool> blocks, Viewport *parentVp)
        : blocksInput(blocks), parent(parentVp) {}
    std::optional<bool> blocksInput;
    Viewport *parent;
  };
  std::shared_ptr<Entry> addViewport(float x, float y, float w, float h,
                                     std::vector<std::unique_ptr<Drawable>> objects,
                                     bool useBackground, const AddOpts &opts = {});

  l2d::Engine *engine_ = nullptr;
  l2d::Image image_;

  // Viewport stack, ordered bottom -> top (list index IS the z position).
  EntryList viewports_;

  // Active pointer captures.
  std::optional<Capture> mouseCapture_;
  std::unordered_map<std::int64_t, Capture> touchCaptures_;
};
