// MIT License © 2026 Binary Dice Games
/// @file resources.cpp
/// @brief The server instructions and the documentation resources, from
///        files compiled in by src/CMakeLists.txt.
#include "mcp/resources.hpp"
#include <array>
#include <cstddef>

namespace bdg::nymph::embedded {
extern const unsigned char mcp_skill[];
extern const std::size_t mcp_skill_size;
extern const unsigned char mcp_agent_guide[];
extern const std::size_t mcp_agent_guide_size;
extern const unsigned char mcp_elements[];
extern const std::size_t mcp_elements_size;
#if defined(NYMPH_MCP_HAS_VIEWER)
extern const unsigned char mcp_viewer[];
extern const std::size_t mcp_viewer_size;
#endif
} // namespace bdg::nymph::embedded

namespace bdg::nymph::mcp {

namespace {

std::string_view text_of(const unsigned char* bytes, std::size_t size) {
  return std::string_view(reinterpret_cast<const char*>(bytes), size);
}

struct document {
  const char* uri;
  const char* name;
  const char* title;
  const char* description;
  std::string_view text;
};

const std::array<document, 2>& documents() {
  static const std::array<document, 2> docs = {{
      {"nymph://docs/agent-guide",
       "agent-guide",
       "nymph agent guide",
       "How to write nymph chart sources: the format, every element family, recipes for each chart and diagram "
       "type, and the common errors with their fixes.",
       text_of(embedded::mcp_agent_guide, embedded::mcp_agent_guide_size)},
      {"nymph://docs/elements",
       "elements",
       "nymph element reference",
       "Every element type and field, with types, defaults and allowed values.",
       text_of(embedded::mcp_elements, embedded::mcp_elements_size)},
  }};
  return docs;
}

constexpr const char* kAppMimeType = "text/html;profile=mcp-app";

/// The App's `_meta.ui`: no network (an empty CSP), a border around it.
json viewer_meta() {
  return {{"ui", {{"csp", json::object()}, {"prefersBorder", true}}}};
}

} // namespace

std::string_view embedded_viewer_html() {
#if defined(NYMPH_MCP_HAS_VIEWER)
  return text_of(embedded::mcp_viewer, embedded::mcp_viewer_size);
#else
  return {};
#endif
}

std::string server_instructions() {
  std::string_view text = text_of(embedded::mcp_skill, embedded::mcp_skill_size);
  // Drop the "---" front matter (name and description of the skill).
  if (text.substr(0, 4) == "---\n") {
    const size_t end = text.find("\n---\n", 3);
    if (end != std::string_view::npos) {
      text.remove_prefix(end + 5);
    }
  }
  while (!text.empty() && text.front() == '\n') {
    text.remove_prefix(1);
  }
  return std::string(text);
}

json list_resources(std::string_view viewer_html) {
  json list = json::array();
  if (!viewer_html.empty()) {
    list.push_back(json{
        {"uri", kViewerUri},
        {"name", "viewer"},
        {"title", "nymph chart viewer"},
        {"description", "The live chart of render_chart's result: pan, zoom and hover the data (an MCP App)."},
        {"mimeType", kAppMimeType},
        {"size", viewer_html.size()},
        {"_meta", viewer_meta()}});
  }
  for (const document& d : documents()) {
    list.push_back(json{
        {"uri", d.uri},
        {"name", d.name},
        {"title", d.title},
        {"description", d.description},
        {"mimeType", "text/markdown"},
        {"size", d.text.size()}});
  }
  return {{"resources", std::move(list)}};
}

json read_resource(const json& params, std::string_view viewer_html) {
  if (!params.is_object() || !params.contains("uri") || !params["uri"].is_string()) {
    throw rpc_error(rpc_code::invalid_params, "resources/read needs \"uri\" (a string)");
  }
  const std::string uri = params["uri"].get<std::string>();
  if (uri == kViewerUri && !viewer_html.empty()) {
    return {
        {"contents",
         json::array({json{
             {"uri", kViewerUri}, {"mimeType", kAppMimeType}, {"text", viewer_html}, {"_meta", viewer_meta()}}})}};
  }
  for (const document& d : documents()) {
    if (uri == d.uri) {
      return {{"contents", json::array({json{{"uri", d.uri}, {"mimeType", "text/markdown"}, {"text", d.text}}})}};
    }
  }
  throw rpc_error(rpc_code::resource_not_found, "resource not found: " + uri);
}

} // namespace bdg::nymph::mcp
