#include "l2d/graphics.hpp"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace l2d {

namespace {

Uint8 toByte(float v) {
  return (Uint8)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}

SDL_Color toSDLColor(const Color &c) {
  return SDL_Color{toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a)};
}

// Content rendered into a transparent canvas with BLENDMODE_BLEND ends up
// premultiplied by alpha; compositing it onto the screen must therefore
// use src*1 + dst*(1-srcA), or antialiased edges (text) darken.
SDL_BlendMode premultipliedBlend() {
  return SDL_ComposeCustomBlendMode(
      SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ONE,
      SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

// LÖVE: Graphics::calculateEllipsePoints — sqrt(r * 20), min 8.
int circleSegments(float radius) {
  int points = (int)std::sqrt(radius * 20.0f);
  return std::max(points, 8);
}

} // namespace

// --- Image / Canvas RAII ---

Image::Image(Image &&other) noexcept : tex_(other.tex_), w_(other.w_), h_(other.h_) {
  other.tex_ = nullptr;
}

Image &Image::operator=(Image &&other) noexcept {
  if (this != &other) {
    if (tex_) SDL_DestroyTexture(tex_);
    tex_ = other.tex_;
    w_ = other.w_;
    h_ = other.h_;
    other.tex_ = nullptr;
  }
  return *this;
}

Image::~Image() {
  if (tex_) SDL_DestroyTexture(tex_);
}

Canvas::Canvas(Canvas &&other) noexcept : tex_(other.tex_), w_(other.w_), h_(other.h_) {
  other.tex_ = nullptr;
}

Canvas &Canvas::operator=(Canvas &&other) noexcept {
  if (this != &other) {
    if (tex_) SDL_DestroyTexture(tex_);
    tex_ = other.tex_;
    w_ = other.w_;
    h_ = other.h_;
    other.tex_ = nullptr;
  }
  return *this;
}

Canvas::~Canvas() {
  if (tex_) SDL_DestroyTexture(tex_);
}

// --- Graphics ---

Graphics::Graphics(SDL_Renderer *renderer, TTF_Font *defaultFont)
    : ren_(renderer), font_(defaultFont) {
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
}

Graphics::~Graphics() = default;

void Graphics::setColor(float r, float g, float b, float a) {
  cur_.color = {r, g, b, a};
}

void Graphics::setColor(const Color &c) { cur_.color = c; }

void Graphics::setLineWidth(float width) { cur_.lineWidth = width; }

void Graphics::push() { stack_.push_back(cur_); }

void Graphics::pop() {
  if (stack_.empty()) return;
  cur_ = stack_.back();
  stack_.pop_back();
  applyScissor();
}

void Graphics::origin() {
  cur_.tx = 0;
  cur_.ty = 0;
}

void Graphics::translate(float dx, float dy) {
  cur_.tx += dx;
  cur_.ty += dy;
}

std::pair<float, float> Graphics::transformPoint(float x, float y) const {
  return {x + cur_.tx, y + cur_.ty};
}

void Graphics::intersectScissor(float x, float y, float w, float h) {
  if (cur_.scissorEnabled) {
    float x1 = std::max(cur_.scissorX, x);
    float y1 = std::max(cur_.scissorY, y);
    float x2 = std::min(cur_.scissorX + cur_.scissorW, x + w);
    float y2 = std::min(cur_.scissorY + cur_.scissorH, y + h);
    cur_.scissorX = x1;
    cur_.scissorY = y1;
    cur_.scissorW = std::max(0.0f, x2 - x1);
    cur_.scissorH = std::max(0.0f, y2 - y1);
  } else {
    cur_.scissorEnabled = true;
    cur_.scissorX = x;
    cur_.scissorY = y;
    cur_.scissorW = std::max(0.0f, w);
    cur_.scissorH = std::max(0.0f, h);
  }
  applyScissor();
}

void Graphics::setScissor(float x, float y, float w, float h) {
  cur_.scissorEnabled = true;
  cur_.scissorX = x;
  cur_.scissorY = y;
  cur_.scissorW = std::max(0.0f, w);
  cur_.scissorH = std::max(0.0f, h);
  applyScissor();
}

void Graphics::setScissor() {
  cur_.scissorEnabled = false;
  applyScissor();
}

void Graphics::applyColor() {
  SDL_SetRenderDrawColor(ren_, toByte(cur_.color.r), toByte(cur_.color.g),
                         toByte(cur_.color.b), toByte(cur_.color.a));
}

void Graphics::applyScissor() {
  // SDL clip rects are per render target, so this runs after every
  // scissor change, pop, and target switch.
  if (cur_.scissorEnabled) {
    SDL_Rect r{(int)std::floor(cur_.scissorX), (int)std::floor(cur_.scissorY),
               (int)std::ceil(cur_.scissorW), (int)std::ceil(cur_.scissorH)};
    // clang-format off
    if (r.w <= 0 || r.h <= 0) { r.w = 0; r.h = 0; } // fully clipped, not "no clip"
    // clang-format on
    SDL_RenderSetClipRect(ren_, &r);
  } else {
    SDL_RenderSetClipRect(ren_, nullptr);
  }
}

void Graphics::fillQuad(float x1, float y1, float x2, float y2, float x3,
                        float y3, float x4, float y4) {
  SDL_Color c = toSDLColor(cur_.color);
  float tx = cur_.tx, ty = cur_.ty;
  SDL_Vertex v[4] = {
      {{x1 + tx, y1 + ty}, c, {0, 0}},
      {{x2 + tx, y2 + ty}, c, {0, 0}},
      {{x3 + tx, y3 + ty}, c, {0, 0}},
      {{x4 + tx, y4 + ty}, c, {0, 0}},
  };
  int indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(ren_, nullptr, v, 4, indices, 6);
}

void Graphics::fillTriangleFan(const std::vector<float> &xyPairs) {
  size_t n = xyPairs.size() / 2;
  if (n < 3) return;
  SDL_Color c = toSDLColor(cur_.color);
  float tx = cur_.tx, ty = cur_.ty;
  std::vector<SDL_Vertex> verts(n);
  for (size_t i = 0; i < n; i++)
    verts[i] = {{xyPairs[i * 2] + tx, xyPairs[i * 2 + 1] + ty}, c, {0, 0}};
  std::vector<int> indices;
  indices.reserve((n - 2) * 3);
  for (size_t i = 1; i + 1 < n; i++) {
    indices.push_back(0);
    indices.push_back((int)i);
    indices.push_back((int)i + 1);
  }
  SDL_RenderGeometry(ren_, nullptr, verts.data(), (int)n, indices.data(),
                     (int)indices.size());
}

void Graphics::rectangle(DrawMode mode, float x, float y, float w, float h) {
  if (mode == DrawMode::Fill) {
    fillQuad(x, y, x + w, y, x + w, y + h, x, y + h);
  } else {
    // Stroke centered on the path, like LÖVE: four edge bands whose
    // corner squares are each covered exactly once.
    float lw = cur_.lineWidth, hlw = lw / 2;
    fillQuad(x - hlw, y - hlw, x + w + hlw, y - hlw, x + w + hlw, y + hlw, x - hlw, y + hlw); // top
    fillQuad(x - hlw, y + h - hlw, x + w + hlw, y + h - hlw, x + w + hlw, y + h + hlw, x - hlw, y + h + hlw); // bottom
    fillQuad(x - hlw, y + hlw, x + hlw, y + hlw, x + hlw, y + h - hlw, x - hlw, y + h - hlw); // left
    fillQuad(x + w - hlw, y + hlw, x + w + hlw, y + hlw, x + w + hlw, y + h - hlw, x + w - hlw, y + h - hlw); // right
  }
}

void Graphics::circle(DrawMode mode, float cx, float cy, float radius) {
  int segments = circleSegments(radius);
  SDL_Color c = toSDLColor(cur_.color);
  float tx = cur_.tx, ty = cur_.ty;

  if (mode == DrawMode::Fill) {
    std::vector<SDL_Vertex> verts(segments + 1);
    verts[0] = {{cx + tx, cy + ty}, c, {0, 0}};
    std::vector<int> indices;
    indices.reserve(segments * 3);
    for (int i = 0; i < segments; i++) {
      float a = (float)i / segments * 2.0f * (float)M_PI;
      verts[i + 1] = {{cx + radius * std::cos(a) + tx, cy + radius * std::sin(a) + ty}, c, {0, 0}};
      indices.push_back(0);
      indices.push_back(1 + i);
      indices.push_back(1 + (i + 1) % segments);
    }
    SDL_RenderGeometry(ren_, nullptr, verts.data(), (int)verts.size(),
                       indices.data(), (int)indices.size());
  } else {
    // Ring between radius - lw/2 and radius + lw/2 (stroke centered on
    // the path), as a triangle strip.
    float hlw = cur_.lineWidth / 2;
    float rIn = std::max(0.0f, radius - hlw), rOut = radius + hlw;
    std::vector<SDL_Vertex> verts((segments + 1) * 2);
    for (int i = 0; i <= segments; i++) {
      float a = (float)(i % segments) / segments * 2.0f * (float)M_PI;
      float ca = std::cos(a), sa = std::sin(a);
      verts[i * 2] = {{cx + rOut * ca + tx, cy + rOut * sa + ty}, c, {0, 0}};
      verts[i * 2 + 1] = {{cx + rIn * ca + tx, cy + rIn * sa + ty}, c, {0, 0}};
    }
    std::vector<int> indices;
    indices.reserve(segments * 6);
    for (int i = 0; i < segments; i++) {
      int o = i * 2;
      indices.push_back(o);
      indices.push_back(o + 1);
      indices.push_back(o + 2);
      indices.push_back(o + 1);
      indices.push_back(o + 3);
      indices.push_back(o + 2);
    }
    SDL_RenderGeometry(ren_, nullptr, verts.data(), (int)verts.size(),
                       indices.data(), (int)indices.size());
  }
}

void Graphics::line(float x1, float y1, float x2, float y2) {
  float dx = x2 - x1, dy = y2 - y1;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0) return;
  float hlw = cur_.lineWidth / 2;
  float nx = -dy / len * hlw, ny = dx / len * hlw;
  fillQuad(x1 + nx, y1 + ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny, x1 - nx, y1 - ny);
}

void Graphics::polygon(DrawMode mode, const std::vector<float> &xyPairs) {
  if (mode == DrawMode::Fill) {
    // Convex fan; concave polygons would need real triangulation.
    fillTriangleFan(xyPairs);
  } else {
    size_t n = xyPairs.size() / 2;
    for (size_t i = 0; i < n; i++) {
      size_t j = (i + 1) % n;
      line(xyPairs[i * 2], xyPairs[i * 2 + 1], xyPairs[j * 2], xyPairs[j * 2 + 1]);
    }
  }
}

SDL_Texture *Graphics::textTexture(const std::string &text, int &outW, int &outH) {
  auto it = textCache_.find(text);
  if (it == textCache_.end()) {
    SDL_Color white{255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font_, text.c_str(), white);
    if (!surf) return nullptr;
    auto img = std::make_unique<Image>();
    img->tex_ = SDL_CreateTextureFromSurface(ren_, surf);
    img->w_ = surf->w;
    img->h_ = surf->h;
    SDL_FreeSurface(surf);
    if (!img->tex_) return nullptr;
    SDL_SetTextureBlendMode(img->tex_, SDL_BLENDMODE_BLEND);
    it = textCache_.emplace(text, std::move(img)).first;
  }
  outW = it->second->w_;
  outH = it->second->h_;
  return it->second->tex_;
}

void Graphics::print(const std::string &text, float x, float y) {
  int w = 0, h = 0;
  SDL_Texture *tex = textTexture(text, w, h);
  if (!tex) return;
  SDL_SetTextureColorMod(tex, toByte(cur_.color.r), toByte(cur_.color.g),
                         toByte(cur_.color.b));
  SDL_SetTextureAlphaMod(tex, toByte(cur_.color.a));
  SDL_FRect dst{x + cur_.tx, y + cur_.ty, (float)w, (float)h};
  SDL_RenderCopyF(ren_, tex, nullptr, &dst);
}

Image Graphics::newImage(const std::string &path) {
  Image img;
  img.tex_ = IMG_LoadTexture(ren_, path.c_str());
  if (!img.tex_)
    throw std::runtime_error("l2d: failed to load image '" + path +
                             "': " + IMG_GetError());
  SDL_QueryTexture(img.tex_, nullptr, nullptr, &img.w_, &img.h_);
  SDL_SetTextureBlendMode(img.tex_, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(img.tex_, SDL_ScaleModeLinear);
  return img;
}

Canvas Graphics::newCanvas(int w, int h) {
  Canvas cv;
  cv.tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_TARGET, w, h);
  if (!cv.tex_)
    throw std::runtime_error(std::string("l2d: failed to create canvas: ") +
                             SDL_GetError());
  cv.w_ = w;
  cv.h_ = h;
  SDL_SetTextureBlendMode(cv.tex_, premultipliedBlend());
  SDL_SetTextureScaleMode(cv.tex_, SDL_ScaleModeLinear);
  return cv;
}

void Graphics::setCanvas(Canvas &canvas) {
  activeCanvas_ = &canvas;
  SDL_SetRenderTarget(ren_, canvas.tex_);
  // Transform and scissor state deliberately survive the target switch
  // (LÖVE semantics); SDL's clip rect is per target, so re-apply it.
  applyScissor();
}

void Graphics::setCanvas() {
  activeCanvas_ = nullptr;
  SDL_SetRenderTarget(ren_, nullptr);
  applyScissor();
}

void Graphics::clear(float r, float g, float b, float a) {
  // love.graphics.clear ignores the scissor.
  SDL_Rect prev;
  SDL_bool hadClip = SDL_RenderIsClipEnabled(ren_);
  if (hadClip) SDL_RenderGetClipRect(ren_, &prev);
  SDL_RenderSetClipRect(ren_, nullptr);
  SDL_SetRenderDrawColor(ren_, toByte(r), toByte(g), toByte(b), toByte(a));
  SDL_RenderClear(ren_);
  if (hadClip) SDL_RenderSetClipRect(ren_, &prev);
}

void Graphics::draw(const Image &image, float x, float y, float rotation,
                    float sx, float sy) {
  if (!image.tex_) return;
  SDL_SetTextureColorMod(image.tex_, toByte(cur_.color.r), toByte(cur_.color.g),
                         toByte(cur_.color.b));
  SDL_SetTextureAlphaMod(image.tex_, toByte(cur_.color.a));
  SDL_FRect dst{x + cur_.tx, y + cur_.ty, image.w_ * sx, image.h_ * sy};
  if (rotation == 0) {
    SDL_RenderCopyF(ren_, image.tex_, nullptr, &dst);
  } else {
    SDL_FPoint center{0, 0}; // LÖVE rotates around the draw origin
    SDL_RenderCopyExF(ren_, image.tex_, nullptr, &dst,
                      rotation * 180.0 / M_PI, &center, SDL_FLIP_NONE);
  }
}

void Graphics::draw(const Canvas &canvas, float x, float y) {
  if (!canvas.tex_) return;
  SDL_SetTextureColorMod(canvas.tex_, toByte(cur_.color.r),
                         toByte(cur_.color.g), toByte(cur_.color.b));
  SDL_SetTextureAlphaMod(canvas.tex_, toByte(cur_.color.a));
  SDL_FRect dst{x + cur_.tx, y + cur_.ty, (float)canvas.w_, (float)canvas.h_};
  SDL_RenderCopyF(ren_, canvas.tex_, nullptr, &dst);
}

std::pair<float, float> Graphics::getDimensions() const {
  // Window size, like love.graphics.getDimensions — independent of any
  // active canvas.
  int w = 0, h = 0;
  SDL_Window *win = SDL_RenderGetWindow(ren_);
  if (win)
    SDL_GetWindowSize(win, &w, &h);
  else
    SDL_GetRendererOutputSize(ren_, &w, &h);
  return {(float)w, (float)h};
}

} // namespace l2d
