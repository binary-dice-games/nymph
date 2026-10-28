// MIT License © 2026 Binary Dice Games
/// @file test_local_socket.cpp
/// @brief spec/14_editor_workspace plan step 8: lines over a local socket
///        both ways, a closed peer, two connections, stopping a blocked
///        accept, a stale socket file, the editors' discovery files.
#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <string>
#include <thread>
#include "ipc/editor_registry.hpp"
#include "ipc/local_socket.hpp"

namespace bdg::nymph::ipc {
namespace {

/// A short folder of this test's own: socket paths have a length limit.
std::filesystem::path test_dir() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const size_t h = std::hash<std::string>{}(std::string(info->test_suite_name()) + "." + info->name());
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / ("nys" + std::to_string(h % 100000000));
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

TEST(local_socket, LinesGoBothWays) {
  const std::filesystem::path dir = test_dir();
  local_listener listener(dir / "s.sock");
  std::thread server([&] {
    std::optional<local_stream> c = listener.accept();
    ASSERT_TRUE(c);
    std::string line;
    while (c->read_line(line)) {
      ASSERT_TRUE(c->write("echo " + line + "\n"));
    }
  });
  local_stream client = local_stream::connect(dir / "s.sock");
  ASSERT_TRUE(client.write("one\r\ntwo\n"));
  std::string line;
  ASSERT_TRUE(client.read_line(line));
  EXPECT_EQ(line, "echo one");
  ASSERT_TRUE(client.read_line(line));
  EXPECT_EQ(line, "echo two");
  const std::string big(300000, 'x'); // more than one read and one write
  ASSERT_TRUE(client.write(big + "\n"));
  ASSERT_TRUE(client.read_line(line));
  EXPECT_EQ(line.size(), big.size() + 5);
  client.shutdown(); // the server's read ends
  server.join();
  std::filesystem::remove_all(dir);
}

TEST(local_socket, AClosedPeerEndsReads) {
  const std::filesystem::path dir = test_dir();
  local_listener listener(dir / "s.sock");
  std::future<std::optional<local_stream>> accepted = std::async(std::launch::async, [&] { return listener.accept(); });
  {
    local_stream client = local_stream::connect(dir / "s.sock");
  } // closed
  std::optional<local_stream> c = accepted.get();
  ASSERT_TRUE(c);
  std::string line;
  EXPECT_FALSE(c->read_line(line));
  std::filesystem::remove_all(dir);
}

TEST(local_socket, ServesASecondConnectionWhileTheFirstIsOpen) {
  const std::filesystem::path dir = test_dir();
  local_listener listener(dir / "s.sock");
  local_stream a = local_stream::connect(dir / "s.sock");
  local_stream b = local_stream::connect(dir / "s.sock");
  std::optional<local_stream> first = listener.accept();
  std::optional<local_stream> second = listener.accept();
  ASSERT_TRUE(first && second);
  ASSERT_TRUE(first->write("1\n"));
  ASSERT_TRUE(second->write("2\n"));
  std::string line;
  ASSERT_TRUE(a.read_line(line));
  EXPECT_EQ(line, "1");
  ASSERT_TRUE(b.read_line(line));
  EXPECT_EQ(line, "2");
  std::filesystem::remove_all(dir);
}

TEST(local_socket, StopEndsABlockedAcceptAndRemovesTheFile) {
  const std::filesystem::path dir = test_dir();
  {
    local_listener listener(dir / "s.sock");
    std::future<std::optional<local_stream>> waiting = std::async(std::launch::async, [&] { return listener.accept(); });
    EXPECT_EQ(waiting.wait_for(std::chrono::milliseconds(50)), std::future_status::timeout);
    listener.stop();
    ASSERT_EQ(waiting.wait_for(std::chrono::seconds(10)), std::future_status::ready);
    EXPECT_FALSE(waiting.get());
    EXPECT_FALSE(listener.accept()); // and from then on
  }
  EXPECT_FALSE(std::filesystem::exists(dir / "s.sock"));
  std::filesystem::remove_all(dir);
}

TEST(local_socket, AStaleFileIsReplacedButALiveListenerIsNot) {
  const std::filesystem::path dir = test_dir();
  std::ofstream(dir / "s.sock") << "left over";
  local_listener listener(dir / "s.sock");
  EXPECT_THROW(local_listener second(dir / "s.sock"), ipc_error);
  EXPECT_THROW(local_stream::connect(dir / "none.sock"), ipc_error);
  EXPECT_THROW(local_listener(dir / std::string(120, 'x')), ipc_error);
  std::filesystem::remove_all(dir);
}

// ── Discovery files ──────────────────────────────────────────────────────────

TEST(editor_registry, RecordsAreListedMostRecentlyFocusedFirst) {
  const std::filesystem::path dir = test_dir();
  {
    editor_record a(dir, editor_entry{101, "a.sock", "/charts/a.yaml", "1", 5.0, {}});
    editor_record b(dir, editor_entry{102, "b.sock", "/charts/b.yaml", "1", 7.0, {}});
    std::vector<editor_entry> all = list_editors(dir);
    ASSERT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].pid, 102);
    a.set_focused(9.0);
    all = list_editors(dir);
    EXPECT_EQ(all[0].pid, 101);
    EXPECT_EQ(pick_editor(all, "")->pid, 101);
    EXPECT_EQ(pick_editor(all, "102")->pid, 102);
    EXPECT_EQ(pick_editor(all, "b.yaml")->pid, 102);
    EXPECT_EQ(pick_editor(all, "/charts/a.yaml")->pid, 101);
    EXPECT_FALSE(pick_editor(all, "c.yaml"));
    EXPECT_FALSE(pick_editor({}, ""));
  }
  EXPECT_TRUE(list_editors(dir).empty()); // removed with the records
  std::filesystem::remove_all(dir);
}

TEST(editor_registry, ABrokenFileIsSkippedAndForgetRemovesBoth) {
  const std::filesystem::path dir = test_dir();
  std::ofstream(dir / "7.json") << "{not json";
  std::ofstream(dir / "8.sock") << "";
  editor_record r(dir, editor_entry{8, (dir / "8.sock").string(), "x.yaml", "1", 0.0, {}});
  const std::vector<editor_entry> all = list_editors(dir);
  ASSERT_EQ(all.size(), 1u);
  forget_editor(all[0]);
  EXPECT_FALSE(std::filesystem::exists(dir / "8.json"));
  EXPECT_FALSE(std::filesystem::exists(dir / "8.sock"));
  std::filesystem::remove_all(dir);
}

TEST(editor_registry, ThisProcessIsAliveAndANonsensePidIsNot) {
  EXPECT_TRUE(process_alive(current_pid()));
  EXPECT_FALSE(process_alive(0));
  EXPECT_FALSE(process_alive(-5));
}

TEST(editor_registry, TheFolderComesFromTheEnvironmentAndIsPrivate) {
  const std::filesystem::path dir = test_dir() / "editors";
#if defined(_WIN32)
  _putenv_s("NYMPH_EDITORS_DIR", dir.string().c_str());
#else
  setenv("NYMPH_EDITORS_DIR", dir.c_str(), 1);
#endif
  EXPECT_EQ(editors_dir(), dir);
  EXPECT_TRUE(std::filesystem::is_directory(dir));
#if !defined(_WIN32)
  const auto perms = std::filesystem::status(dir).permissions();
  EXPECT_EQ(perms & (std::filesystem::perms::group_all | std::filesystem::perms::others_all), std::filesystem::perms::none);
#endif
  std::filesystem::remove_all(dir.parent_path());
}

TEST(editor_registry, ALongFolderGetsItsSocketInAShortOne) {
  const std::filesystem::path dir = test_dir();
  EXPECT_EQ(socket_path(dir, 42), dir / "42.sock");
  const std::filesystem::path deep = dir / std::string(90, 'd');
  const std::filesystem::path s = socket_path(deep, 42);
  EXPECT_LE(s.string().size(), kMaxSocketPath);
  EXPECT_EQ(s.filename(), "42.sock");
  local_listener listener(s); // usable
  std::filesystem::remove_all(dir);
}

} // namespace
} // namespace bdg::nymph::ipc
