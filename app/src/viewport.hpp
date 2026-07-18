// Viewport: a movable, resizable, scrollable, clipping widget frame,
// transcribed from l2d-im/viewport.lua.
//
// A Viewport does not listen to raw input itself: the router walks the
// viewport stack top-down, uses beginDrag / bodyInputKind to find the
// first viewport that consumes an event, and then forwards the rest of
// that gesture (dragTo / endDrag / maybeFireClick / wheelmoved) only to
// the capturing viewport.
#pragma once

#include <l2d/graphics.hpp>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "scene.hpp"

class Viewport;

// One stack entry, in the same { viewport, scene } shape as the Lua.
// Held by shared_ptr so a live capture survives bringToFront's
// remove-then-append reordering.
struct Entry {
  std::unique_ptr<Viewport> viewport;
  std::unique_ptr<Scene> scene;
};

using EntryList = std::vector<std::shared_ptr<Entry>>;

class Viewport {
public:
  static constexpr float HANDLE_SIZE = 14 * 3;  // square origin handle side
  static constexpr float HANDLE_RADIUS = 8 * 3; // resize circle handle radius
  static constexpr float CLICK_MOVE_THRESHOLD = 6; // max px still counted as a click

  enum class DragMode { None, Move, Resize, Pan };
  // Press hit kinds returned by beginDrag ("move" / "resize" /
  // "pan-content" / "pan-opaque" / "pan-transparent" / nil in the Lua).
  enum class PressKind { None, Move, Resize, PanContent, PanOpaque, PanTransparent };
  // Body classification ("content" / "opaque" / "transparent" / nil).
  enum class BodyKind { Outside, Content, Opaque, Transparent };

  // blocksInput: when true, the whole body consumes input even where no
  // content was hit; when false, empty areas let input fall through.
  Viewport(float x, float y, float w, float h, bool blocksInput);

  std::pair<float, float> getDimensions() const { return {w, h}; }
  std::pair<float, float> getContentSize() const { return {contentW, contentH}; }

  std::pair<float, float> toContent(float px, float py) const {
    return {px - x + scrollX, py - y + scrollY};
  }

  BodyKind bodyInputKind(float px, float py) const;

  // Reports the current bounding size of the drawn content, so h/v
  // scrolling can reach every part of it. Safe to call every frame.
  void setContentSize(float cw, float ch);

  bool hitOrigin(float mx, float my) const;
  bool hitResize(float mx, float my) const;
  bool hitBody(float mx, float my) const;

  void clampScroll();

  PressKind beginDrag(float px, float py);
  void dragTo(float px, float py);
  void endDrag() { dragMode = DragMode::None; }

  // Fires onClick at a position translated into content-space, if
  // inside the body. Returns whether a click was fired.
  bool fireClickAt(float px, float py);

  // If the just-finished drag was actually a click (started as body
  // pan, moved less than the threshold), fire onClick.
  void maybeFireClick(float px, float py);

  // True when the content is larger than the frame.
  bool canScroll() const { return contentW > w || contentH > h; }

  // Scrolls for a wheel event the router already routed here. Shift
  // forces horizontal; otherwise vertical, falling back to horizontal
  // when that is the only overflowing axis.
  void wheelmoved(float dx, float dy, bool shiftDown);

  // contentFn(w, h) is clipped to the viewport and scrolls with it.
  // backgroundFn(w, h), if given, is clipped but stays fixed relative
  // to the origin regardless of scroll.
  void draw(l2d::Graphics &g, const std::function<void(float, float)> &contentFn,
            const Scene::BackgroundFn &backgroundFn);

  // Re-renders every entry's scene and draws it, bottom -> top. Shared
  // by the root stack and by draw() for nested children.
  static void drawStack(l2d::Graphics &g, EntryList &entries);

  float x, y, w, h;
  float scrollX = 0, scrollY = 0;
  float minW = 60, minH = 60;
  bool blocksInput;
  float contentW, contentH;
  DragMode dragMode = DragMode::None;
  bool dragMoved = false;
  std::function<void(float, float)> onClick;      // content-space click
  std::function<bool(float, float)> hitContent;   // "is there content here?"
  EntryList children; // nested child viewports, in parent content-space

private:
  float lastX = 0, lastY = 0;
  float dragStartX = 0, dragStartY = 0;
};
