#include <cstdio>
#include <exception>

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
