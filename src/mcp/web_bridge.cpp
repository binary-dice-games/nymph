// MIT License © 2026 Binary Dice Games
/// @file web_bridge.cpp
/// @brief Pairing and forwarding to web app pages (see web_bridge.hpp).
#include "mcp/web_bridge.hpp"
#include <algorithm>
#include <cstdlib>
#include <future>
#include <random>
#include "ipc/editor_registry.hpp"
#include "mcp/editor_tools.hpp"
#include "mcp/mcp_session.hpp"
#include "mcp/tool.hpp"
#include "mcp/tools.hpp"
#include "nymph.hpp"

namespace bdg::nymph::mcp {

namespace {

constexpr int kMaxWrongCodes = 3;
constexpr const char* kPath = "/nymph";
constexpr const char* kNoPage =
    "the nymph web app is not paired: call web_app_pair and give the user the code (or the link) it returns";

/// Crockford base32 (no I, L, O, U): easy to read and to type.
constexpr const char* kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

std::string random_chars(size_t n, const char* alphabet, size_t size) {
  std::random_device rd;
  std::string out;
  for (size_t i = 0; i < n; ++i) {
    out.push_back(alphabet[rd() % size]);
  }
  return out;
}

/// Compares in time independent of where the strings differ.
bool same_secret(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) {
    return false;
  }
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    diff |= static_cast<unsigned char>(a[i] ^ b[i]);
  }
  return diff == 0;
}

/// `https://host[:port]` of a URL.
std::string origin_of(const std::string& url) {
  const size_t scheme = url.find("://");
  if (scheme == std::string::npos) {
    return url;
  }
  const size_t path = url.find('/', scheme + 3);
  return url.substr(0, path);
}

std::vector<std::string> split_list(const std::string& text) {
  std::vector<std::string> out;
  std::string item;
  for (const char c : text + ",") {
    if (c == ',' || c == ' ' || c == '\t') {
      if (!item.empty()) {
        out.push_back(item);
      }
      item.clear();
    } else {
      item.push_back(c);
    }
  }
  return out;
}

json error_reply(const std::string& message) {
  return json{{"error", {{"code", -32603}, {"message", message}}}};
}

} // namespace

struct web_bridge::page {
  std::unique_ptr<websocket> ws; ///< set once the upgrade succeeded
  std::thread thread;
  std::mutex mtx; ///< pending, next_id
  std::map<int, std::function<void(const json&)>> pending;
  int next_id = 1;
  // Guarded by web_bridge::mtx_:
  std::string name = "untitled";
  double focus = 0.0;
  json tools = json::array();
  bool ready = false;
  std::atomic<bool> done{false};
};

web_bridge::web_bridge(web_bridge_options options, std::function<void()> changed)
    : options_(std::move(options)), changed_(std::move(changed)) {
  if (options_.origins.empty()) {
    options_.origins.push_back(origin_of(options_.app_url));
    if (const char* extra = std::getenv("NYMPH_WEB_APP_ORIGINS")) {
      for (std::string& o : split_list(extra)) {
        options_.origins.push_back(std::move(o));
      }
    }
  }
}

web_bridge::~web_bridge() {
  stopping_ = true;
  std::vector<std::shared_ptr<page>> pages;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    if (listener_) {
      listener_->stop();
    }
    pages = pages_;
    for (const auto& p : pages) {
      if (p->ws) {
        p->ws->shutdown();
      }
    }
  }
  if (accept_.joinable()) {
    accept_.join();
  }
  for (const auto& p : pages) {
    if (p->thread.joinable()) {
      p->thread.join();
    }
  }
}

web_pairing web_bridge::pair() {
  reap();
  std::unique_lock<std::mutex> lock(mtx_);
  if (listener_ && wrong_ >= kMaxWrongCodes) {
    // Closed after wrong codes: a new listener on a new port.
    listener_->stop();
    lock.unlock();
    if (accept_.joinable()) {
      accept_.join();
    }
    lock.lock();
    listener_.reset();
  }
  if (!listener_) {
    try {
      listener_ = std::make_unique<ipc::loopback_listener>(0);
    } catch (const ipc::ipc_error& e) {
      throw tool_error(std::string("cannot open a port for the web app: ") + e.what());
    }
    accept_ = std::thread(&web_bridge::accept_loop, this, listener_.get());
  }
  wrong_ = 0;
  web_pairing p;
  p.port = listener_->port();
  const std::string secret = random_chars(8, kAlphabet, 32);
  code_ = std::to_string(p.port) + "-" + secret.substr(0, 4) + "-" + secret.substr(4);
  code_expires_ = std::chrono::steady_clock::now() + options_.code_life;
  p.code = code_;
  p.link = options_.app_url + "#pair=" + code_;
  p.expires_in = std::chrono::duration_cast<std::chrono::seconds>(options_.code_life).count();
  return p;
}

void web_bridge::set_client(std::string name) {
  std::string clean;
  for (const char c : name) {
    const char l = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9') || l == '-') {
      clean.push_back(l);
    } else if (!clean.empty() && clean.back() != '-') {
      clean.push_back('-');
    }
  }
  while (!clean.empty() && clean.back() == '-') {
    clean.pop_back();
  }
  const std::lock_guard<std::mutex> lock(mtx_);
  client_ = clean.empty() ? "agent" : clean.substr(0, 32);
}

bool web_bridge::has_pages() const {
  const std::lock_guard<std::mutex> lock(mtx_);
  return std::any_of(pages_.begin(), pages_.end(), [](const auto& p) { return p->ready && !p->done; });
}

double web_bridge::latest_focus() const {
  const std::lock_guard<std::mutex> lock(mtx_);
  double latest = 0.0;
  for (const auto& p : pages_) {
    if (p->ready && !p->done) {
      latest = std::max(latest, p->focus);
    }
  }
  return latest;
}

std::vector<std::string> web_bridge::page_names() const {
  const std::lock_guard<std::mutex> lock(mtx_);
  std::vector<std::string> names;
  for (const auto& p : pages_) {
    if (p->ready && !p->done) {
      names.push_back("web:" + p->name);
    }
  }
  return names;
}

json web_bridge::page_tools() const {
  const std::lock_guard<std::mutex> lock(mtx_);
  json out = json::array();
  std::vector<std::string> seen;
  for (const auto& p : pages_) {
    if (!p->ready || p->done) {
      continue;
    }
    for (const json& t : p->tools) {
      const std::string name = t.value("name", std::string());
      if (name.empty() || name.rfind("editor_", 0) == 0 || std::find(seen.begin(), seen.end(), name) != seen.end()) {
        continue;
      }
      seen.push_back(name);
      json def = t;
      if (!def.contains("inputSchema") || !def["inputSchema"].is_object()) {
        def["inputSchema"] = {{"type", "object"}};
      }
      if (!def["inputSchema"].contains("properties")) {
        def["inputSchema"]["properties"] = json::object();
      }
      def["inputSchema"]["properties"]["editor"] = {
          {"type", "string"},
          {"description", "Which paired web app tab: web:<document name>; default the one used last."}};
      def["inputSchema"].erase("additionalProperties");
      out.push_back(std::move(def));
    }
  }
  return out;
}

std::shared_ptr<web_bridge::page> web_bridge::pick(const std::string& want) const {
  std::shared_ptr<page> best;
  for (const auto& p : pages_) {
    if (!p->ready || p->done) {
      continue;
    }
    if (want.empty()) {
      if (!best || p->focus > best->focus) {
        best = p;
      }
    } else if (want == "web:" + p->name || want == p->name) {
      return p;
    }
  }
  return best;
}

json web_bridge::call(const std::string& want, const std::string& tool, json arguments) {
  std::shared_ptr<page> p;
  std::string client;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    p = pick(want);
    client = client_;
    if (!p) {
      if (want.empty() || pages_.empty()) {
        throw tool_error(kNoPage);
      }
      std::string open;
      for (const auto& q : pages_) {
        if (q->ready && !q->done) {
          open += (open.empty() ? "" : ", ") + std::string("web:") + q->name;
        }
      }
      throw tool_error("no paired web app has '" + want + "' open; paired: " + (open.empty() ? "none" : open));
    }
  }
  auto done = std::make_shared<std::promise<json>>();
  std::future<json> result = done->get_future();
  const int id = request(*p, "tools/call",
          {{"name", tool}, {"arguments", std::move(arguments)}, {"_meta", {{"nymph/origin", client}}}},
          [done](const json& reply) { done->set_value(reply); });
  if (result.wait_for(options_.call_timeout) != std::future_status::ready) {
    {
      const std::lock_guard<std::mutex> lock(p->mtx);
      p->pending.erase(id);
    }
    throw tool_error("the web app did not answer (the user may not have answered a question there); call again");
  }
  const json reply = result.get();
  if (reply.contains("error")) {
    throw tool_error(reply["error"].value("message", std::string("the web app refused the request")));
  }
  return reply.value("result", json::object());
}

int web_bridge::request(page& p, const std::string& method, json params, std::function<void(const json&)> reply) {
  int id = 0;
  {
    const std::lock_guard<std::mutex> lock(p.mtx);
    id = p.next_id++;
    p.pending[id] = std::move(reply);
  }
  const json message{{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", std::move(params)}};
  if (p.done || !p.ws || !p.ws->send_text(to_line(message))) {
    std::function<void(const json&)> cb;
    {
      const std::lock_guard<std::mutex> lock(p.mtx);
      if (const auto it = p.pending.find(id); it != p.pending.end()) {
        cb = std::move(it->second);
        p.pending.erase(it);
      }
    }
    if (cb) {
      cb(error_reply("the web app was closed"));
    }
  }
  return id;
}

bool web_bridge::allowed_origin(const std::string& origin) const {
  for (const std::string& o : options_.origins) {
    if (o == origin) {
      return true;
    }
    if (o.size() > 2 && o.compare(o.size() - 2, 2, ":*") == 0) {
      const std::string prefix = o.substr(0, o.size() - 1); // keeps the ':'
      if (origin.size() > prefix.size() && origin.compare(0, prefix.size(), prefix) == 0 &&
          std::all_of(origin.begin() + static_cast<std::ptrdiff_t>(prefix.size()), origin.end(),
                      [](char c) { return c >= '0' && c <= '9'; })) {
        return true;
      }
    }
  }
  return false;
}

void web_bridge::accept_loop(ipc::loopback_listener* listener) {
  while (!stopping_) {
    std::optional<ipc::local_stream> stream = listener->accept();
    if (!stream) {
      listener->close(); // refused from now on, until a new pairing
      return;
    }
    const std::lock_guard<std::mutex> lock(mtx_);
    if (stopping_) {
      return;
    }
    auto p = std::make_shared<page>();
    p->ws = std::make_unique<websocket>(std::move(*stream));
    pages_.push_back(p);
    p->thread = std::thread(&web_bridge::serve, this, p);
  }
}

bool web_bridge::admit(page& p) {
  std::string first;
  if (!p.ws->read_message(first)) {
    return false;
  }
  json hello;
  try {
    hello = json::parse(first);
  } catch (const json::exception&) {
    hello = json::object();
  }
  const std::lock_guard<std::mutex> lock(mtx_);
  std::string token;
  if (hello.is_object() && hello.contains("pair") && hello["pair"].is_string()) {
    const std::string code = hello["pair"].get<std::string>();
    const bool valid = !code_.empty() && std::chrono::steady_clock::now() < code_expires_ && same_secret(code, code_);
    if (!valid) {
      ++wrong_;
      p.ws->send_text(to_line(json{{"paired", false}, {"error", "that code is not valid: ask your agent for a new one"}}));
      p.ws->close(1008);
      if (wrong_ >= kMaxWrongCodes) {
        code_.clear();
        if (listener_) {
          listener_->stop(); // closed until the agent pairs again
        }
      }
      return false;
    }
    code_.clear(); // once
    wrong_ = 0;
    token = random_chars(32, "0123456789abcdef", 16);
    resume_tokens_.push_back(token);
  } else if (hello.is_object() && hello.contains("resume") && hello["resume"].is_string()) {
    const std::string want = hello["resume"].get<std::string>();
    for (const std::string& t : resume_tokens_) {
      if (same_secret(t, want)) {
        token = t;
      }
    }
    if (token.empty()) {
      p.ws->send_text(to_line(json{{"paired", false}, {"error", "this connection has expired: pair again"}}));
      p.ws->close(1008);
      return false;
    }
  } else {
    p.ws->close(1008);
    return false;
  }
  if (hello.contains("name") && hello["name"].is_string()) {
    p.name = hello["name"].get<std::string>();
  }
  p.focus = ipc::now_epoch();
  p.ws->send_text(to_line(json{{"paired", true}, {"resume", token}}));
  return true;
}

void web_bridge::serve(const std::shared_ptr<page>& p) {
  const uint16_t port = [&] {
    const std::lock_guard<std::mutex> lock(mtx_);
    return listener_ ? listener_->port() : uint16_t(0);
  }();
  const bool upgraded = p->ws->accept_upgrade(kPath, [&](const ws_request& r) {
    const std::string p1 = "127.0.0.1:" + std::to_string(port);
    const std::string p2 = "localhost:" + std::to_string(port);
    return (r.host == p1 || r.host == p2) && allowed_origin(r.origin);
  });
  if (upgraded) {
    if (!stopping_ && admit(*p)) {
      // initialize, then the tool list; the page is ready with its tools.
      std::function<void()> list_tools = [this, p] {
        request(*p, "tools/list", json::object(), [this, p](const json& reply) {
          if (reply.contains("result") && reply["result"].contains("tools") && reply["result"]["tools"].is_array()) {
            {
              const std::lock_guard<std::mutex> lock(mtx_);
              p->tools = reply["result"]["tools"];
              p->ready = true;
            }
            if (changed_) {
              changed_();
            }
          }
        });
      };
      request(*p, "initialize",
              {{"protocolVersion", std::string(supported_protocol_versions().front())},
               {"capabilities", json::object()},
               {"clientInfo", {{"name", "nymph-mcp"}, {"version", std::string(version())}}}},
              [p, list_tools](const json& reply) {
                if (reply.contains("result")) {
                  p->ws->send_text(to_line(json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}));
                  list_tools();
                }
              });
      std::string text;
      while (!stopping_ && p->ws->read_message(text)) {
        json message;
        try {
          message = json::parse(text);
        } catch (const json::exception&) {
          continue;
        }
        if (!message.is_object()) {
          continue;
        }
        if (message.contains("id") && !message.contains("method")) {
          std::function<void(const json&)> cb;
          if (message["id"].is_number_integer()) {
            const std::lock_guard<std::mutex> lock(p->mtx);
            if (const auto it = p->pending.find(message["id"].get<int>()); it != p->pending.end()) {
              cb = std::move(it->second);
              p->pending.erase(it);
            }
          }
          if (cb) {
            cb(message);
          }
          continue;
        }
        const std::string method = message.value("method", std::string());
        if (method == "notifications/tools/list_changed") {
          list_tools();
        } else if (method == "nymph/focus") {
          const json params = message.value("params", json::object());
          const std::lock_guard<std::mutex> lock(mtx_);
          p->focus = ipc::now_epoch();
          if (params.is_object() && params.contains("name") && params["name"].is_string()) {
            p->name = params["name"].get<std::string>();
          }
        } else if (message.contains("id")) {
          // The bridge serves nothing to the page.
          p->ws->send_text(to_line(make_error(message["id"], rpc_code::method_not_found, "method not found: " + method)));
        }
      }
      p->ws->shutdown();
    }
  }
  bool was_ready = false;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    was_ready = p->ready;
    p->ready = false;
    p->done = true;
  }
  std::map<int, std::function<void(const json&)>> pending;
  {
    const std::lock_guard<std::mutex> lock(p->mtx);
    pending.swap(p->pending);
  }
  for (auto& [id, cb] : pending) {
    cb(error_reply("the web app was closed while it worked on this; call the tool again once it is paired"));
  }
  if (was_ready && !stopping_ && changed_) {
    changed_();
  }
}

void web_bridge::reap() {
  std::vector<std::shared_ptr<page>> finished;
  {
    const std::lock_guard<std::mutex> lock(mtx_);
    for (auto it = pages_.begin(); it != pages_.end();) {
      if ((*it)->done) {
        finished.push_back(*it);
        it = pages_.erase(it);
      } else {
        ++it;
      }
    }
  }
  for (const auto& p : finished) {
    if (p->thread.joinable()) {
      p->thread.join();
    }
  }
}

// ── Tools ────────────────────────────────────────────────────────────────────

void add_web_app_tools(tool_registry& registry, std::shared_ptr<web_bridge> bridge) {
  registry.add(
      {{{"name", "web_app_pair"},
        {"title", "Pair with the nymph web app"},
        {"description",
         "Connects to the chart the user has open in the nymph web app (a browser tab). Returns a one-time code and a "
         "link: give both to the user, who types the code in the app (Agents > Connect an agent) or opens the link. "
         "Once paired, the app's tools (document_state, document_set_source, app_*, ...) join your tool list and the "
         "editor_* tools work on that tab. Call it only when the user wants you to work in the web app."},
        {"inputSchema", {{"type", "object"}, {"properties", json::object()}, {"additionalProperties", false}}},
        {"annotations",
         {{"title", "Pair with the nymph web app"},
          {"readOnlyHint", false},
          {"destructiveHint", false},
          {"idempotentHint", false},
          {"openWorldHint", false}}}},
       [bridge](const json&) {
         const web_pairing p = bridge->pair();
         const int64_t minutes = std::max<int64_t>(1, p.expires_in / 60);
         json r = text_result("Pairing code: " + p.code + "\nLink: " + p.link +
                              "\n\nAsk the user to type the code in the open nymph web app (Agents > Connect an "
                              "agent), or to open the link, which opens the app paired. The code works once, for " +
                              std::to_string(minutes) +
                              " minutes. Once paired, read app_read_guide with name web-app before changing the "
                              "chart. If the app's tools do not appear in your list, call them through web_app_call.");
         r["structuredContent"] = {
             {"code", p.code}, {"link", p.link}, {"port", p.port}, {"expiresInSeconds", p.expires_in}};
         return r;
       }});
  registry.add(
      {{{"name", "web_app_call"},
        {"title", "Call a web app tool"},
        {"description",
         "Calls a tool of the paired nymph web app by name (document_state, document_set_source, app_read_guide, "
         "editor_set_fields, ...). Use it when those tools are not in your tool list after web_app_pair."},
        {"inputSchema",
         {{"type", "object"},
          {"properties",
           {{"name", {{"type", "string"}, {"description", "The app's tool."}}},
            {"arguments", {{"type", "object"}, {"description", "Its arguments."}}},
            {"editor", {{"type", "string"}, {"description", "Which tab: web:<document name>; default the one used last."}}}}},
          {"required", {"name"}},
          {"additionalProperties", false}}},
        {"annotations",
         {{"title", "Call a web app tool"},
          {"readOnlyHint", false},
          {"destructiveHint", false},
          {"idempotentHint", false},
          {"openWorldHint", false}}}},
       [bridge](const json& args) {
         if (!args.contains("name") || !args["name"].is_string()) {
           throw tool_error("web_app_call needs 'name'");
         }
         json arguments = args.value("arguments", json::object());
         if (!arguments.is_object()) {
           throw tool_error("'arguments' must be an object");
         }
         return bridge->call(args.value("editor", std::string()), args["name"].get<std::string>(), std::move(arguments));
       }});
  registry.set_dynamic([bridge] { return bridge->page_tools(); },
                       [bridge](const std::string& name, const json& args) -> std::optional<json> {
                         bool known = false;
                         for (const json& t : bridge->page_tools()) {
                           known = known || t.value("name", std::string()) == name;
                         }
                         if (!known) {
                           return std::nullopt;
                         }
                         json arguments = args;
                         std::string want;
                         if (arguments.contains("editor") && arguments["editor"].is_string()) {
                           want = arguments["editor"].get<std::string>();
                         }
                         arguments.erase("editor");
                         return bridge->call(want, name, std::move(arguments));
                       });
}

} // namespace bdg::nymph::mcp
