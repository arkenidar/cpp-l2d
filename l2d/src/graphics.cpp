#include "l2d/graphics.hpp"

#include <SDL.h>

#include <stb_image.h>
#include <stb_truetype.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace l2d {

namespace {

Uint8 toByte(float v) {
  return (Uint8)std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}

SDL_Color toSDLColor(const Color &c) {
  return SDL_Color{toByte(c.r), toByte(c.g), toByte(c.b), toByte(c.a)};
}

SDL_BlendMode premultipliedBlend() {
  return SDL_ComposeCustomBlendMode(
      SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
      SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ONE,
      SDL_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
}

int circleSegments(float radius) {
  int points = (int)std::sqrt(radius * 20.0f);
  return std::max(points, 8);
}

#if !SDL_VERSION_ATLEAST(2, 0, 18)
void fillPolygonScanline(SDL_Renderer *ren, const std::vector<SDL_FPoint> &pts,
                         SDL_Color c) {
  size_t n = pts.size();
  if (n < 3) return;
  float ymin = pts[0].y, ymax = pts[0].y;
  for (const auto &p : pts) {
    ymin = std::min(ymin, p.y);
    ymax = std::max(ymax, p.y);
  }
  SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
  std::vector<float> xs;
  for (int y = (int)std::floor(ymin); y < (int)std::ceil(ymax); y++) {
    float yc = y + 0.5f;
    xs.clear();
    for (size_t i = 0; i < n; i++) {
      const SDL_FPoint &a = pts[i], &b = pts[(i + 1) % n];
      if ((a.y <= yc && b.y > yc) || (b.y <= yc && a.y > yc))
        xs.push_back(a.x + (yc - a.y) / (b.y - a.y) * (b.x - a.x));
    }
    std::sort(xs.begin(), xs.end());
    for (size_t i = 0; i + 1 < xs.size(); i += 2) {
      int xa = (int)std::floor(xs[i]), xb = (int)std::ceil(xs[i + 1]) - 1;
      if (xb >= xa) SDL_RenderDrawLine(ren, xa, y, xb, y);
    }
  }
}
#endif

bool readFileBytes(const std::string &path, std::vector<unsigned char> &out) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return false;
  std::streamsize size = f.tellg();
  if (size <= 0) return false;
  f.seekg(0, std::ios::beg);
  out.resize((size_t)size);
  return f.read((char *)out.data(), size).good();
}

} // namespace

// --- Font (stb_truetype baked atlas) ---

struct Font::Data {
  SDL_Texture *atlas = nullptr;
  int atlasW = 0, atlasH = 0;
  // Printable ASCII (32-126) plus the Latin-1 Supplement block
  // (160-255), so accented letters like "Ö" (U+00D6) have a baked
  // glyph rather than being silently skipped.
  static constexpr int kFirstChar = 32;
  static constexpr int kNumChars = 95;
  static constexpr int kFirstChar2 = 160;
  static constexpr int kNumChars2 = 96;
  stbtt_packedchar chars[kNumChars] = {};
  stbtt_packedchar chars2[kNumChars2] = {};
  float pixelHeight = 0;
};

Font::Font() : data_(std::make_unique<Data>()) {}
Font::~Font() {
  if (data_ && data_->atlas) SDL_DestroyTexture(data_->atlas);
}
Font::Font(Font &&other) noexcept : data_(std::move(other.data_)) {}
Font &Font::operator=(Font &&other) noexcept {
  if (this != &other) {
    if (data_ && data_->atlas) SDL_DestroyTexture(data_->atlas);
    data_ = std::move(other.data_);
  }
  return *this;
}

bool Font::load(SDL_Renderer *ren, const std::string &path, float pixelHeight) {
  std::vector<unsigned char> ttf;
  if (!readFileBytes(path, ttf)) return false;
  constexpr int ATLAS_W = 512, ATLAS_H = 256;
  std::vector<unsigned char> pixels(ATLAS_W * ATLAS_H, 0);
  stbtt_pack_context spc;
  if (!stbtt_PackBegin(&spc, pixels.data(), ATLAS_W, ATLAS_H, ATLAS_W, 1, nullptr))
    return false;
  stbtt_PackSetOversampling(&spc, 2, 2);
  int ok = stbtt_PackFontRange(&spc, ttf.data(), 0, pixelHeight,
                               Data::kFirstChar, Data::kNumChars, data_->chars);
  ok = ok && stbtt_PackFontRange(&spc, ttf.data(), 0, pixelHeight,
                                 Data::kFirstChar2, Data::kNumChars2,
                                 data_->chars2);
  stbtt_PackEnd(&spc);
  if (!ok) return false;
  std::vector<unsigned char> rgba(ATLAS_W * ATLAS_H * 4);
  for (int i = 0; i < ATLAS_W * ATLAS_H; i++) {
    rgba[i * 4 + 0] = 255;
    rgba[i * 4 + 1] = 255;
    rgba[i * 4 + 2] = 255;
    rgba[i * 4 + 3] = pixels[i];
  }
  if (data_->atlas) SDL_DestroyTexture(data_->atlas);
  data_->atlas = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                                   SDL_TEXTUREACCESS_STATIC, ATLAS_W, ATLAS_H);
  if (!data_->atlas) return false;
  SDL_UpdateTexture(data_->atlas, nullptr, rgba.data(), ATLAS_W * 4);
  SDL_SetTextureBlendMode(data_->atlas, SDL_BLENDMODE_BLEND);
  data_->atlasW = ATLAS_W;
  data_->atlasH = ATLAS_H;
  data_->pixelHeight = pixelHeight;
  return true;
}

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

Graphics::Graphics(SDL_Renderer *renderer, Font *defaultFont)
    : ren_(renderer), font_(defaultFont) {
  SDL_SetRenderDrawBlendMode(ren_, SDL_BLENDMODE_BLEND);
}
Graphics::~Graphics() = default;

void Graphics::setColor(float r, float g, float b, float a) { cur_.color = {r, g, b, a}; }
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
  if (cur_.scissorEnabled) {
    SDL_Rect r{(int)std::floor(cur_.scissorX), (int)std::floor(cur_.scissorY),
               (int)std::ceil(cur_.scissorW), (int)std::ceil(cur_.scissorH)};
    if (r.w <= 0 || r.h <= 0) {
      r.w = 0;
      r.h = 0;
    }
    SDL_RenderSetClipRect(ren_, &r);
  } else {
    SDL_RenderSetClipRect(ren_, nullptr);
  }
}

void Graphics::fillQuad(float x1, float y1, float x2, float y2, float x3,
                        float y3, float x4, float y4) {
  SDL_Color c = toSDLColor(cur_.color);
  float tx = cur_.tx, ty = cur_.ty;
#if SDL_VERSION_ATLEAST(2, 0, 18)
  SDL_Vertex v[4] = {
      {{x1 + tx, y1 + ty}, c, {0, 0}},
      {{x2 + tx, y2 + ty}, c, {0, 0}},
      {{x3 + tx, y3 + ty}, c, {0, 0}},
      {{x4 + tx, y4 + ty}, c, {0, 0}},
  };
  int indices[6] = {0, 1, 2, 0, 2, 3};
  SDL_RenderGeometry(ren_, nullptr, v, 4, indices, 6);
#else
  fillPolygonScanline(ren_, {{x1 + tx, y1 + ty}, {x2 + tx, y2 + ty}, {x3 + tx, y3 + ty}, {x4 + tx, y4 + ty}}, c);
#endif
}

void Graphics::fillTriangleFan(const std::vector<float> &xyPairs) {
  size_t n = xyPairs.size() / 2;
  if (n < 3) return;
  SDL_Color c = toSDLColor(cur_.color);
  float tx = cur_.tx, ty = cur_.ty;
#if SDL_VERSION_ATLEAST(2, 0, 18)
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
#else
  std::vector<SDL_FPoint> pts(n);
  for (size_t i = 0; i < n; i++)
    pts[i] = {xyPairs[i * 2] + tx, xyPairs[i * 2 + 1] + ty};
  fillPolygonScanline(ren_, pts, c);
#endif
}

void Graphics::rectangle(DrawMode mode, float x, float y, float w, float h) {
  if (mode == DrawMode::Fill) {
    fillQuad(x, y, x + w, y, x + w, y + h, x, y + h);
  } else {
    float lw = cur_.lineWidth, hlw = lw / 2;
    fillQuad(x - hlw, y - hlw, x + w + hlw, y - hlw, x + w + hlw, y + hlw, x - hlw, y + hlw);
    fillQuad(x - hlw, y + h - hlw, x + w + hlw, y + h - hlw, x + w + hlw, y + h + hlw, x - hlw, y + h + hlw);
    fillQuad(x - hlw, y + hlw, x + hlw, y + hlw, x + hlw, y + h - hlw, x - hlw, y + h - hlw);
    fillQuad(x + w - hlw, y + hlw, x + w + hlw, y + hlw, x + w + hlw, y + h - hlw, x + w - hlw, y + h - hlw);
  }
}

void Graphics::circle(DrawMode mode, float cx, float cy, float radius) {
  int segments = circleSegments(radius);
  SDL_Color c = toSDLColor(cur_.color);
  float tx = cur_.tx, ty = cur_.ty;
  if (mode == DrawMode::Fill) {
#if SDL_VERSION_ATLEAST(2, 0, 18)
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
#else
    std::vector<SDL_FPoint> pts(segments);
    for (int i = 0; i < segments; i++) {
      float a = (float)i / segments * 2.0f * (float)M_PI;
      pts[i] = {cx + radius * std::cos(a) + tx, cy + radius * std::sin(a) + ty};
    }
    fillPolygonScanline(ren_, pts, c);
#endif
  } else {
    float hlw = cur_.lineWidth / 2;
    float rIn = std::max(0.0f, radius - hlw), rOut = radius + hlw;
#if SDL_VERSION_ATLEAST(2, 0, 18)
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
#else
    for (int i = 0; i < segments; i++) {
      float a0 = (float)i / segments * 2.0f * (float)M_PI;
      float a1 = (float)(i + 1) / segments * 2.0f * (float)M_PI;
      float c0 = std::cos(a0), s0 = std::sin(a0);
      float c1 = std::cos(a1), s1 = std::sin(a1);
      fillPolygonScanline(ren_, {{cx + rOut * c0 + tx, cy + rOut * s0 + ty}, {cx + rOut * c1 + tx, cy + rOut * s1 + ty}, {cx + rIn * c1 + tx, cy + rIn * s1 + ty}, {cx + rIn * c0 + tx, cy + rIn * s0 + ty}}, c);
    }
#endif
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
    fillTriangleFan(xyPairs);
  } else {
    size_t n = xyPairs.size() / 2;
    for (size_t i = 0; i < n; i++) {
      size_t j = (i + 1) % n;
      line(xyPairs[i * 2], xyPairs[i * 2 + 1], xyPairs[j * 2], xyPairs[j * 2 + 1]);
    }
  }
}

namespace {

// Decodes the next UTF-8 codepoint starting at text[i], advancing i past
// it. Malformed sequences decode as a single replacement codepoint
// (U+FFFD, itself unmapped so it's silently skipped by print()) rather
// than desyncing the rest of the string.
std::uint32_t decodeUtf8(const std::string &text, size_t &i) {
  unsigned char c0 = (unsigned char)text[i];
  auto cont = [&](size_t idx) {
    return idx < text.size() && ((unsigned char)text[idx] & 0xC0) == 0x80;
  };
  if (c0 < 0x80) {
    i += 1;
    return c0;
  }
  if ((c0 & 0xE0) == 0xC0 && cont(i + 1)) {
    std::uint32_t cp = ((c0 & 0x1Fu) << 6) | ((unsigned char)text[i + 1] & 0x3Fu);
    i += 2;
    return cp;
  }
  if ((c0 & 0xF0) == 0xE0 && cont(i + 1) && cont(i + 2)) {
    std::uint32_t cp = ((c0 & 0x0Fu) << 12) | (((unsigned char)text[i + 1] & 0x3Fu) << 6) |
                       ((unsigned char)text[i + 2] & 0x3Fu);
    i += 3;
    return cp;
  }
  if ((c0 & 0xF8) == 0xF0 && cont(i + 1) && cont(i + 2) && cont(i + 3)) {
    std::uint32_t cp = ((c0 & 0x07u) << 18) | (((unsigned char)text[i + 1] & 0x3Fu) << 12) |
                       (((unsigned char)text[i + 2] & 0x3Fu) << 6) |
                       ((unsigned char)text[i + 3] & 0x3Fu);
    i += 4;
    return cp;
  }
  i += 1;
  return 0xFFFD;
}

} // namespace

void Graphics::print(const std::string &text, float x, float y, float scale) {
  if (!font_ || !font_->data_ || !font_->data_->atlas) return;
  Font::Data &d = *font_->data_;
  SDL_SetTextureColorMod(d.atlas, toByte(cur_.color.r), toByte(cur_.color.g),
                         toByte(cur_.color.b));
  SDL_SetTextureAlphaMod(d.atlas, toByte(cur_.color.a));
  float startX = x + cur_.tx, startY = y + cur_.ty;
  float penX = startX / scale, penY = startY / scale;
  for (size_t i = 0; i < text.size();) {
    std::uint32_t cp = decodeUtf8(text, i);
    stbtt_packedchar *chars;
    int index;
    if (cp >= Font::Data::kFirstChar && cp < Font::Data::kFirstChar + Font::Data::kNumChars) {
      chars = d.chars;
      index = (int)cp - Font::Data::kFirstChar;
    } else if (cp >= Font::Data::kFirstChar2 &&
               cp < Font::Data::kFirstChar2 + Font::Data::kNumChars2) {
      chars = d.chars2;
      index = (int)cp - Font::Data::kFirstChar2;
    } else {
      continue;
    }
    stbtt_aligned_quad q;
    stbtt_GetPackedQuad(chars, d.atlasW, d.atlasH, index, &penX, &penY, &q, 0);
    int sx = (int)std::round(q.s0 * d.atlasW);
    int sy = (int)std::round(q.t0 * d.atlasH);
    int sw = (int)std::round(q.s1 * d.atlasW) - sx;
    int sh = (int)std::round(q.t1 * d.atlasH) - sy;
    if (sw <= 0 || sh <= 0) continue;
    SDL_Rect src{sx, sy, sw, sh};
    SDL_FRect dst{q.x0 * scale, q.y0 * scale, (q.x1 - q.x0) * scale,
                  (q.y1 - q.y0) * scale};
    SDL_RenderCopyF(ren_, d.atlas, &src, &dst);
  }
}

Image Graphics::newImage(const std::string &path) {
  int w = 0, h = 0, n = 0;
  unsigned char *pixels = stbi_load(path.c_str(), &w, &h, &n, 4);
  if (!pixels)
    throw std::runtime_error("l2d: failed to load image '" + path +
                             "': " + stbi_failure_reason());
  Image img;
  img.tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_ABGR8888,
                               SDL_TEXTUREACCESS_STATIC, w, h);
  if (!img.tex_) {
    stbi_image_free(pixels);
    throw std::runtime_error(std::string("l2d: failed to create texture: ") + SDL_GetError());
  }
  SDL_UpdateTexture(img.tex_, nullptr, pixels, w * 4);
  stbi_image_free(pixels);
  img.w_ = w;
  img.h_ = h;
  SDL_SetTextureBlendMode(img.tex_, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 12)
  SDL_SetTextureScaleMode(img.tex_, SDL_ScaleModeLinear);
#endif
  return img;
}

Canvas Graphics::newCanvas(int w, int h) {
  Canvas cv;
  cv.tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_TARGET, w, h);
  if (!cv.tex_)
    throw std::runtime_error(std::string("l2d: failed to create canvas: ") + SDL_GetError());
  cv.w_ = w;
  cv.h_ = h;
  SDL_SetTextureBlendMode(cv.tex_, premultipliedBlend());
#if SDL_VERSION_ATLEAST(2, 0, 12)
  SDL_SetTextureScaleMode(cv.tex_, SDL_ScaleModeLinear);
#endif
  return cv;
}

void Graphics::setCanvas(Canvas &canvas) {
  activeCanvas_ = &canvas;
  SDL_SetRenderTarget(ren_, canvas.tex_);
  applyScissor();
}
void Graphics::setCanvas() {
  activeCanvas_ = nullptr;
  SDL_SetRenderTarget(ren_, nullptr);
  applyScissor();
}

void Graphics::clear(float r, float g, float b, float a) {
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
    SDL_FPoint center{0, 0};
    SDL_RenderCopyExF(ren_, image.tex_, nullptr, &dst, rotation * 180.0 / M_PI,
                      &center, SDL_FLIP_NONE);
  }
}

void Graphics::draw(const Canvas &canvas, float x, float y) {
  if (!canvas.tex_) return;
  SDL_SetTextureColorMod(canvas.tex_, toByte(cur_.color.r), toByte(cur_.color.g),
                         toByte(cur_.color.b));
  SDL_SetTextureAlphaMod(canvas.tex_, toByte(cur_.color.a));
  SDL_FRect dst{x + cur_.tx, y + cur_.ty, (float)canvas.w_, (float)canvas.h_};
  SDL_RenderCopyF(ren_, canvas.tex_, nullptr, &dst);
}

std::pair<float, float> Graphics::getDimensions() const {
  int w = 0, h = 0;
#if SDL_VERSION_ATLEAST(2, 0, 22)
  SDL_Window *win = SDL_RenderGetWindow(ren_);
  if (win)
    SDL_GetWindowSize(win, &w, &h);
  else
#endif
    SDL_GetRendererOutputSize(ren_, &w, &h);
  return {(float)w, (float)h};
}

} // namespace l2d