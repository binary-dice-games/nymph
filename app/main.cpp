// MIT License © 2026 Binary Dice Games
/// @file main.cpp
/// @brief `nymph` command-line entry point.
#include <nymph.hpp>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--version") {
      std::cout << "nymph " << bdg::nymph::version() << "\n";
      return 0;
    }
  }
  std::cout << "nymph " << bdg::nymph::version() << "\n"
            << "usage: nymph [--version]\n";
  return 0;
}
