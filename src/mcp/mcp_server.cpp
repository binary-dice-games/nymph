// MIT License © 2026 Binary Dice Games
/// @file mcp_server.cpp
/// @brief The stdio loop of `nymph mcp` and its arguments.
#include "mcp/mcp_server.hpp"
#include <exception>
#include <mutex>
#include <istream>
#include <ostream>
#include <string>
#include "mcp/resources.hpp"

namespace bdg::nymph::mcp {

namespace {

constexpr const char* kUsage =
    "usage: nymph mcp [--rasterizer software|opengl]\n"
    "Runs an MCP server: JSON-RPC messages, one per line, on stdin and stdout.\n"
    "--rasterizer chooses how renders draw (default opengl, falling back to software).\n";

/// Bad arguments: usage and exit 2.
struct usage_error {
  std::string message;
};

server_options parse(std::span<const std::string_view> args) {
  server_options options;
  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] != "--rasterizer") {
      throw usage_error{"unknown argument '" + std::string(args[i]) + "'"};
    }
    if (i + 1 >= args.size()) {
      throw usage_error{"--rasterizer needs software or opengl"};
    }
    const std::string_view value = args[++i];
    if (value == "software") {
      options.raster = rasterizer::software;
    } else if (value == "opengl") {
      options.raster = rasterizer::opengl;
    } else {
      throw usage_error{"unknown rasterizer '" + std::string(value) + "' (rasterizers: software, opengl)"};
    }
    options.raster_explicit = true;
  }
  return options;
}

bool blank(const std::string& line) {
  return line.find_first_not_of(" \t\r") == std::string::npos;
}

} // namespace

int run_mcp(std::istream& in, std::ostream& out, std::ostream& log, const server_options& options) {
  // Responses come from this thread, notifications from the web bridge's.
  std::mutex out_mtx;
  server_options with_notify = options;
  with_notify.notify = [&out, &out_mtx](const json& message) {
    const std::lock_guard<std::mutex> lock(out_mtx);
    out << to_line(message) << '\n';
    out.flush();
  };
  session s(with_notify);
  std::string line;
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (blank(line)) {
      continue;
    }
    std::optional<json> response;
    json id;
    try {
      response = s.handle(parse_message(line, id));
    } catch (const rpc_error& e) {
      response = make_error(id, e.code(), e.what());
    } catch (const std::exception& e) {
      response = make_error(id, rpc_code::internal_error, std::string("internal error: ") + e.what());
    }
    if (response) {
      const std::lock_guard<std::mutex> lock(out_mtx);
      out << to_line(*response) << '\n';
      out.flush();
      if (!out) {
        log << "nymph mcp: cannot write to standard output\n";
        return 1;
      }
    }
  }
  return 0;
}

int run_mcp_cli(std::span<const std::string_view> args, std::istream& in, std::ostream& out, std::ostream& err) {
  for (std::string_view a : args) {
    if (a == "--help" || a == "-h") {
      out << kUsage;
      return 0;
    }
  }
  try {
    server_options options = parse(args);
    options.viewer_html = embedded_viewer_html();
    return run_mcp(in, out, err, options);
  } catch (const usage_error& e) {
    err << "nymph: " << e.message << "\n" << kUsage;
    return 2;
  }
}

} // namespace bdg::nymph::mcp
