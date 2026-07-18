// Input router, transcribed from the capture logic in l2d-im/main.lua.
//
// Once a press is claimed by a viewport, all following move/release
// events of that pointer go only to it. `tentative` means the press
// landed on a transparent empty area and the click (if it stays a
// click) will be re-dispatched to sibling layers below.
#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "viewport.hpp"

struct Capture {
  std::shared_ptr<Entry> entry;
  EntryList *list = nullptr;      // the stack the entry belongs to
  std::vector<Viewport *> chain;  // ancestor viewports, outer -> inner
  bool tentative = false;
};

// Moves a stack entry to the top (end) of its own list, like a desktop
// window raised on click.
void bringToFront(EntryList &list, const std::shared_ptr<Entry> &entry);

// Converts a raw screen (x, y) into the coordinate space capture.list /
// capture.entry operate in, applying each ancestor's toContent in
// outer-to-inner order.
std::pair<float, float> toLocal(const std::vector<Viewport *> &chain, float x, float y);

// Walks a stack top-down for the first viewport that claims a press at
// (x, y), descending into children first so the topmost nested content
// wins. Only the list the capture landed in gets reordered. Descending
// into children is gated on hitBody, but the viewport's own beginDrag
// is NOT — the handles stick out past the body rectangle. A press on
// the viewport's own move/resize handle skips the children descent
// entirely: its handles are drawn on top of its children.
std::optional<Capture> capturePressAt(EntryList &list, float x, float y);

// Ends a captured gesture at (x, y). A tentative gesture that stayed a
// click is forwarded to the topmost sibling below that actually has
// something there, raising it.
void releaseCapture(Capture &capture, float x, float y);

// Routes a wheel event to the topmost viewport under (mx, my) that
// should consume it, descending into children first. Returns true once
// consumed.
bool wheelRoute(EntryList &list, float mx, float my, float dx, float dy,
                bool shiftDown);
