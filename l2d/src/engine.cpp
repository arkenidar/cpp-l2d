#include "l2d/engine.hpp"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace l2d {

namespace {

// LÖVE's default font is Vera Sans at 13px; the library ships the same
// Vera.ttf in its resources directory.
constexpr int kDefaultFontSize = 13;

std::string findDefaultFont() {
  std::string base = Engine::basePath();
  const char *candidates[] = {
      "resources/Vera.ttf",
      "l2d/resources/Vera.ttf", // running from a build tree
      "../l2d/resources/Vera.ttf",
  };
  for (const char *rel : candidates) {
    std::string path = base + rel;
    SDL_RWops *rw = SDL_RWFromFile(path.c_str(), "rb");
    if (rw) {
      SDL_RWclose(rw);
      return path;
    }
  }
  return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
}

// Detects Termux (with or without the X11 add-on), which reports as a
// normal Linux desktop to SDL but needs the borderless-window sizing
// below rather than SDL_WINDOW_FULLSCREEN_DESKTOP.
bool runningInTermux() {
  const char *v = SDL_getenv("TERMUX_VERSION");
  if (v && v[0]) return true;
  const char *p = SDL_getenv("PREFIX");
  return p && std::strstr(p, "com.termux") != nullptr;
}

} // namespace

Engine::Engine(const Config &config) : config_(config) {
  // No synthetic mouse events from touch or vice versa — the app gets
  // each input stream once, like LÖVE.
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    throw std::runtime_error(std::string("l2d: SDL_Init failed: ") + SDL_GetError());

  // Fill the real display resolution rather than opening at a fixed
  // desktop-tuned size, so the window/viewport layout starts out
  // matching the actual screen (phone or desktop) from frame one.
  SDL_DisplayMode mode;
  if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
    config_.width = mode.w;
    config_.height = mode.h;
  }

  Uint32 flags = 0;
  if (config_.resizable) flags |= SDL_WINDOW_RESIZABLE;
  // No SDL_WINDOW_ALLOW_HIGHDPI: event coordinates and render
  // coordinates must agree, as the LÖVE callbacks assume.
  window_ = SDL_CreateWindow(config_.title.c_str(), SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, config_.width,
                             config_.height, flags);
  if (!window_)
    throw std::runtime_error(std::string("l2d: SDL_CreateWindow failed: ") + SDL_GetError());

#ifdef L2D_ANDROID_APK
  // On a real Android APK (SDL activity + JNI glue, defined by
  // android/app/jni/CMakeLists.txt), the OS status/navigation bars are
  // drawn on top of the window. Fullscreen puts the activity into
  // immersive sticky mode, which hides both so the whole screen is
  // drawable. Gated on this macro rather than __ANDROID__: Termux's and
  // CxxDroid's clang also target bionic and define that, yet their builds
  // are ordinary executables with no Android JNI bridge, where this call
  // either does nothing useful or (Termux:X11) renders black.
  if (SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
    SDL_Log("l2d: SDL_SetWindowFullscreen failed: %s", SDL_GetError());
#endif

  // Under Termux:X11, SDL_WINDOW_FULLSCREEN_DESKTOP renders black (the
  // window is focused and takes input, but nothing is drawn), so fill the
  // screen with a borderless window instead, which stays on the normal
  // windowed render path. Sized to the display's usable bounds rather than
  // the full desktop mode so the window starts below the Android status
  // bar, which Termux:X11 draws (semi-transparently) on top of anything
  // placed under it.
  if (runningInTermux()) {
    SDL_Rect ub;
    if (SDL_GetDisplayUsableBounds(0, &ub) == 0) {
      SDL_SetWindowBordered(window_, SDL_FALSE);
      SDL_SetWindowPosition(window_, ub.x, ub.y);
      SDL_SetWindowSize(window_, ub.w, ub.h);
      config_.width = ub.w;
      config_.height = ub.h;
    } else if (SDL_DisplayMode mode; SDL_GetDesktopDisplayMode(0, &mode) == 0) {
      SDL_SetWindowBordered(window_, SDL_FALSE);
      SDL_SetWindowPosition(window_, 0, 0);
      SDL_SetWindowSize(window_, mode.w, mode.h);
      config_.width = mode.w;
      config_.height = mode.h;
    }
  }

  renderer_ = SDL_CreateRenderer(window_, -1,
                                 SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_)
    renderer_ = SDL_CreateRenderer(window_, -1, 0); // software fallback
  if (!renderer_)
    throw std::runtime_error(std::string("l2d: SDL_CreateRenderer failed: ") + SDL_GetError());

  std::string fontPath = findDefaultFont();
  font_ = std::make_unique<Font>();
  if (!font_->load(renderer_, fontPath, (float)kDefaultFontSize))
    throw std::runtime_error("l2d: failed to load font '" + fontPath + "'");

  gfx_ = std::make_unique<Graphics>(renderer_, font_.get());
}

Engine::~Engine() {
  gfx_.reset();
  font_.reset();
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);
  SDL_Quit();
}

int Engine::run(AppBase &app) {
  app.load(*this);

  Uint64 prev = SDL_GetPerformanceCounter();
  const double freq = (double)SDL_GetPerformanceFrequency();

  while (!quitRequested_) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      case SDL_QUIT:
        quitRequested_ = true;
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (e.button.which != SDL_TOUCH_MOUSEID)
          app.mousepressed((float)e.button.x, (float)e.button.y, e.button.button);
        break;
      case SDL_MOUSEMOTION:
        if (e.motion.which != SDL_TOUCH_MOUSEID)
          app.mousemoved((float)e.motion.x, (float)e.motion.y,
                         (float)e.motion.xrel, (float)e.motion.yrel);
        break;
      case SDL_MOUSEBUTTONUP:
        if (e.button.which != SDL_TOUCH_MOUSEID)
          app.mousereleased((float)e.button.x, (float)e.button.y, e.button.button);
        break;
      case SDL_MOUSEWHEEL: {
        float dx = (float)e.wheel.x, dy = (float)e.wheel.y;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
          dx = -dx;
          dy = -dy;
        }
        app.wheelmoved(dx, dy);
        break;
      }
      case SDL_FINGERDOWN:
      case SDL_FINGERMOTION:
      case SDL_FINGERUP: {
        int ww = 0, wh = 0;
        SDL_GetWindowSize(window_, &ww, &wh);
        float x = e.tfinger.x * ww, y = e.tfinger.y * wh;
        std::int64_t id = (std::int64_t)e.tfinger.fingerId;
        if (e.type == SDL_FINGERDOWN)
          app.touchpressed(id, x, y);
        else if (e.type == SDL_FINGERMOTION)
          app.touchmoved(id, x, y);
        else
          app.touchreleased(id, x, y);
        break;
      }
      case SDL_KEYDOWN: {
        std::string name = SDL_GetKeyName(e.key.keysym.sym);
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        app.keypressed(name);
        break;
      }
      case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
          app.resize(e.window.data1, e.window.data2);
        break;
      default:
        break;
      }
    }

    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (now - prev) / freq;
    prev = now;
    app.update(dt);

    // Like love.run: reset the translation each frame, clear, draw.
    gfx_->origin();
    gfx_->clear(config_.background.r, config_.background.g,
                config_.background.b, config_.background.a);
    app.draw(*gfx_);
    SDL_RenderPresent(renderer_);
  }

  app.quit();
  return 0;
}

std::pair<float, float> Engine::mousePosition() const {
  int x = 0, y = 0;
  SDL_GetMouseState(&x, &y);
  return {(float)x, (float)y};
}

bool Engine::isShiftDown() const {
  return (SDL_GetModState() & KMOD_SHIFT) != 0;
}

std::string Engine::basePath() {
  char *p = SDL_GetBasePath();
  if (!p) return "./";
  std::string s(p);
  SDL_free(p);
  return s;
}

} // namespace l2d
