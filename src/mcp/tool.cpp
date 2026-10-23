// MIT License © 2026 Binary Dice Games
/// @file tool.cpp
/// @brief text_result() (see tool.hpp).
#include "mcp/tool.hpp"

namespace bdg::nymph::mcp {

json text_result(const std::string& text, bool error) {
  json result = {{"content", json::array({{{"type", "text"}, {"text", text}}})}};
  if (error) {
    result["isError"] = true;
  }
  return result;
}

} // namespace bdg::nymph::mcp
