// l2d::Engine — window, renderer, main loop, and event dispatch with
// Love2D callback semantics. Subclass l2d::AppBase and override the
// callbacks you need, then Engine::run(app).
#pragma once

#include <cstdint>
#include <string>
#include <utility>

#include "graphics.hpp"

struct SDL_Window;

namespace l2d {

class Engine;
class Font;

// Love2D-style application callbacks. Coordinates are window pixels,
// matching love.mousepressed / love.touchpressed conventions (touch
// coordinates are converted from SDL's normalized form).
class AppBase {
public:
  virtual ~AppBase() = default;
  virtual void load(Engine &) {}
  virtual void update(double /*dt*/) {}
  virtual void draw(Graphics &) {}
  virtual void mousepressed(float /*x*/, float /*y*/, int /*button*/) {}
  virtual void mousemoved(float /*x*/, float /*y*/, float /*dx*/, float /*dy*/) {}
  virtual void mousereleased(float /*x*/, float /*y*/, int /*button*/) {}
  virtual void wheelmoved(float /*dx*/, float /*dy*/) {}
  virtual void touchpressed(std::int64_t /*id*/, float /*x*/, float /*y*/) {}
  virtual void touchmoved(std::int64_t /*id*/, float /*x*/, float /*y*/) {}
  virtual void touchreleased(std::int64_t /*id*/, float /*x*/, float /*y*/) {}
  virtual void keypressed(const std::string & /*key*/) {}
  virtual void resize(int /*w*/, int /*h*/) {}
  virtual void quit() {}
};

struct Config {
  std::string title = "l2d";
  int width = 800; // LÖVE defaults
  int height = 600;
  bool resizable = true;
  Color background{0, 0, 0, 1};
};

class Engine {
public:
  explicit Engine(const Config &config = {});
  ~Engine();
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  // Runs the main loop until quit. Returns a process exit code.
  int run(AppBase &app);

  // love.event.quit
  void quitEvent() { quitRequested_ = true; }

  Graphics &graphics() { return *gfx_; }
  Font &font() { return *font_; }
  std::pair<float, float> mousePosition() const; // love.mouse.getPosition
  bool isShiftDown() const; // love.keyboard.isDown("lshift","rshift")

  // Directory of the running executable, with trailing separator —
  // for locating bundled assets.
  static std::string basePath();

private:
  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  std::unique_ptr<Font> font_;
  std::unique_ptr<Graphics> gfx_;
  Config config_;
  bool quitRequested_ = false;
};

} // namespace l2d
