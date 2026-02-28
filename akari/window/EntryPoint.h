#pragma once

#include "window/Application.h"

extern akari::window::Application* akari::window::CreateApplication(int argc, char** argv);
bool                               g_ApplicationRunning {true};

namespace akari::window {

int Main(int argc, char** argv) {
  while (g_ApplicationRunning) {
    akari::window::Application* app = akari::window::CreateApplication(argc, argv);
    app->Run();
    delete app;
  }

  return EXIT_SUCCESS;
}

} // namespace akari::window

int main(int argc, char** argv) {
  return akari::window::Main(argc, argv);
}