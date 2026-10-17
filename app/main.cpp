// MIT License © 2026 Binary Dice Games
/// @file main.cpp
/// @brief `nymph` command-line entry point: forwards to run_cli().
#include <cli/cli.hpp>
#include <iostream>
#include <string_view>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#endif

int main(int argc, char** argv) {
#if defined(_WIN32)
  // PNG bytes go through stdin/stdout: no CRLF translation.
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  std::ios::sync_with_stdio(false);
  const std::vector<std::string_view> args(argv + 1, argv + argc);
  return bdg::nymph::run_cli(args, std::cin, std::cout, std::cerr);
}
