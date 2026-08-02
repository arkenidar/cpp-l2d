// l2d::Graphics — a Love2D-compatible 2D drawing context over SDL_Renderer.
//
// Replicates the love.graphics semantics this library commits to:
//  - translate-only transform stack with push("all")/pop state saving
//    (color, line width, translation, scissor)
//  - intersectScissor in absolute render-target pixels, composing with
//    the current scissor (SDL's clip rect does not compose by itself)
//  - render-target canvases; like LÖVE, setCanvas does NOT reset the
//    transform or scissor, only the projection (i.e. coordinates map
//    1:1 onto canvas pixels)
//  - shapes honoring setLineWidth, text via the default font (Vera 13px,
//    the same font LÖVE embeds)
//
// CPU-only: uses SDL2's software renderer (no GL/GPU). Image decoding
// is done via stb_image and font rasterization via stb_truetype, so
// neither SDL2_image nor SDL2_ttf is required.
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace l2d {

struct Color {
  float r = 1, g = 1, b = 1, a = 1;
};

// Single-line enum; ColumnLimit 0 would split it.
// clang-format off
enum class DrawMode { Fill, Line };
// clang-format on

// A drawable texture loaded from an image file. Move-only.
class Image {
public:
  Image() = default;
  Image(Image &&other) noexcept;
  Image &operator=(Image &&other) noexcept;
  ~Image();
  Image(const Image &) = delete;
  Image &operator=(const Image &) = delete;

  std::pair<float, float> getDimensions() const { return {(float)w_, (float)h_}; }
  bool valid() const { return tex_ != nullptr; }

private:
  friend class Graphics;
  SDL_Texture *tex_ = nullptr;
  int w_ = 0, h_ = 0;
};

// An offscreen render target. Move-only.
class Canvas {
public:
  Canvas() = default;
  Canvas(Canvas &&other) noexcept;
  Canvas &operator=(Canvas &&other) noexcept;
  ~Canvas();
  Canvas(const Canvas &) = delete;
  Canvas &operator=(const Canvas &) = delete;

  std::pair<float, float> getDimensions() const { return {(float)w_, (float)h_}; }
  bool valid() const { return tex_ != nullptr; }

private:
  friend class Graphics;
  SDL_Texture *tex_ = nullptr;
  int w_ = 0, h_ = 0;
};

// A baked font atlas (stb_truetype under the hood). Move-only. The
// implementation details are hidden via PIMPL so stb_truetype.h stays a
// private dependency of the library.
class Font {
public:
  Font();
  ~Font();
  Font(Font &&other) noexcept;
  Font &operator=(Font &&other) noexcept;
  Font(const Font &) = delete;
  Font &operator=(const Font &) = delete;

  bool valid() const { return data_ != nullptr; }

  // Bakes a font atlas from a .ttf file at the given pixel height.
  bool load(SDL_Renderer *ren, const std::string &path, float pixelHeight);

private:
  friend class Graphics;
  struct Data;
  std::unique_ptr<Data> data_;
};

class Graphics {
public:
  Graphics(SDL_Renderer *renderer, Font *defaultFont);
  ~Graphics();
  Graphics(const Graphics &) = delete;
  Graphics &operator=(const Graphics &) = delete;

  // --- state ---
  void setColor(float r, float g, float b, float a = 1.0f);
  void setColor(const Color &c);
  void setLineWidth(float width);
  void push(); // push("all") semantics
  void pop();
  void origin(); // reset translation (per-frame, like love.run)
  void translate(float dx, float dy);
  std::pair<float, float> transformPoint(float x, float y) const;
  // Absolute render-target pixels, ignoring the translation; composes
  // with the current scissor.
  void intersectScissor(float x, float y, float w, float h);
  // Replaces the current scissor outright (absolute pixels).
  void setScissor(float x, float y, float w, float h);
  // Disables the scissor, like love.graphics.setScissor() with no args.
  void setScissor();

  // --- primitives ---
  void rectangle(DrawMode mode, float x, float y, float w, float h);
  void circle(DrawMode mode, float cx, float cy, float radius);
  void line(float x1, float y1, float x2, float y2);
  void polygon(DrawMode mode, const std::vector<float> &xyPairs);
  void print(const std::string &text, float x, float y, float scale = 1.0f);

  // --- images / canvases ---
  Image newImage(const std::string &path);
  Canvas newCanvas(int w, int h);
  void setCanvas(Canvas &canvas); // draw into the canvas
  void setCanvas();               // back to the screen
  void clear(float r, float g, float b, float a);
  void draw(const Image &image, float x, float y, float rotation = 0,
            float sx = 1, float sy = 1);
  void draw(const Canvas &canvas, float x, float y);

  // Size of the current window backbuffer.
  std::pair<float, float> getDimensions() const;

private:
  struct State {
    Color color{1, 1, 1, 1};
    float lineWidth = 1;
    float tx = 0, ty = 0;
    bool scissorEnabled = false;
    float scissorX = 0, scissorY = 0, scissorW = 0, scissorH = 0;
  };

  void applyColor();
  void applyScissor();
  void fillQuad(float x1, float y1, float x2, float y2, float x3, float y3,
                float x4, float y4);
  void fillTriangleFan(const std::vector<float> &xyPairs);

  SDL_Renderer *ren_;
  Font *font_;
  State cur_;
  std::vector<State> stack_;
  Canvas *activeCanvas_ = nullptr;
};

} // namespace l2d