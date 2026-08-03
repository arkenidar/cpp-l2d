#include <cstdio>
#include <exception>

#ifdef L2D_ANDROID_APK
// SDL_main.h is mandatory in the file that defines main() on Android: it
// remaps main to SDL_main, which SDL's JNI glue (SDLActivity) calls.
#include <SDL_main.h>
#endif

#include <l2d/engine.hpp>

#include "demo.hpp"

int main() {
  try {
    l2d::Config config;
    config.title = "cpp-l2d";
    l2d::Engine engine(config);
    DemoApp app;
    return engine.run(app);
  } catch (const std::exception &e) {
    std::fprintf(stderr, "An error occurred: %s\n", e.what());
    return 1;
  }
}
