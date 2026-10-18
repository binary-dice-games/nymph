// MIT License © 2026 Binary Dice Games
/// @file main.cpp
/// @brief `nymph` command-line entry point: forwards `nymph mcp` to
///        run_mcp_cli() and everything else to run_cli().
#include <cli/cli.hpp>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>
#if defined(NYMPH_HAS_MCP)
#include <mcp/mcp_server.hpp>
#endif
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
#if defined(NYMPH_HAS_MCP)
  if (!args.empty() && args[0] == "mcp") {
    return bdg::nymph::mcp::run_mcp_cli(std::span(args).subspan(1), std::cin, std::cout, std::cerr);
  }
#endif
  return bdg::nymph::run_cli(args, std::cin, std::cout, std::cerr);
}
