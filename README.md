# cpp-l2d

A C++20 + SDL2 port of the Lua/LÖVE project [`l2d-im`](../l2d-im)
(immediate-mode viewport/scene experiments), split into a **reusable Love2D-like
framework** and the demo app that uses it. Renders CPU-only via SDL2's software
renderer (no GL/GPU); image decoding uses
[stb_image](https://github.com/nothings/stb) and font rasterization uses
[stb_truetype](https://github.com/nothings/stb), so **only SDL2 itself** is
required — not SDL2_image or SDL2_ttf.

## Layout

```
l2d/    reusable library (static lib target `l2d`) — knows nothing about the demo
        l2d::Graphics   love.graphics-compatible drawing over SDL_Renderer:
                        color/lineWidth state, translate + push/pop("all"),
                        intersectScissor, rect/circle/line/polygon (thick
                        strokes), print (Vera 13px, the font LÖVE embeds),
                        images (PNG/JPG), render-target canvases
        l2d::Engine     window + renderer + main loop; dispatches SDL events
        l2d::AppBase    Love2D-style callbacks to override: load/update/draw,
                        mousepressed/moved/released, wheelmoved, touch*,
                        keypressed, resize, quit
        l2d::Font       stb_truetype baked-atlas font (replaces SDL2_ttf)
        stb/            vendored stb_image.h + stb_truetype.h (header-only)
        src/stb_impl.cpp  the single TU that expands the stb implementations
app/    the l2d-im demo, transcribed module-for-module from the Lua:
        shapes.cpp   <- shapes.lua     scene.cpp    <- scene.lua
        viewport.cpp <- viewport.lua   router.cpp   <- main.lua (input router)
        demo.cpp     <- main.lua (love.load + callbacks)
```

## Build & run

Requires only `libsdl2-dev` (SDL2_image and SDL2_ttf are no longer needed —
image and font handling use the vendored stb libraries, and rendering is
CPU-only via SDL2's software renderer).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./build/app/cpp-l2d
```

## Using the library in another app

```cpp
#include <l2d/engine.hpp>

class MyApp : public l2d::AppBase {
  void draw(l2d::Graphics &g) override {
    g.setColor(1, 0, 0);
    g.rectangle(l2d::DrawMode::Fill, 100, 100, 200, 150);
    g.setColor(1, 1, 1);
    g.print("Hello", 10, 10);
  }
  void keypressed(const std::string &key) override { /* ... */ }
};

int main() {
  l2d::Engine engine({.title = "my app"});
  MyApp app;
  return engine.run(app);
}
```

Link the `l2d` CMake target and ship `l2d/resources/Vera.ttf` as
`resources/Vera.ttf` next to your binary (the app target here does this with a
post-build copy).

## Fidelity notes

- Behavior was verified side-by-side against `love l2d-im`, including the
  input-routing spec in `l2d-im/docs/expected-behavior.md` (z-order raise,
  transparent-area click fallthrough, handle hit areas, nested clipping,
  click-vs-drag threshold, wheel routing).
- Like LÖVE, `setCanvas` does **not** reset the transform or scissor (verified
  against LÖVE 11.5 with a pixel-readback probe). A scene rendered mid-frame
  therefore wraps its canvas pass in push/origin/setScissor-off — the same fix
  was applied to the Lua original's `scene.lua`, which used to render its nested
  child's content mostly outside the child frame because of this.
- Circle tessellation uses LÖVE's segment formula (`max(8, sqrt(20 r))`);
  strokes are centered on the path like LÖVE's; canvases are composited with a
  premultiplied-alpha blend to avoid dark fringes on text edges.

## License

MIT (see [LICENSE](LICENSE)). The bundled `l2d/resources/Vera.ttf` is the
Bitstream Vera Sans font, redistributed under its own
[Bitstream Vera Fonts license](https://www.gnome.org/fonts/#Final_Bitstream_Vera_Fonts).
The vendored `l2d/stb/stb_image.h` and `l2d/stb/stb_truetype.h` are
public-domain works from the [stb](https://github.com/nothings/stb) collection
by Sean Barrett.
