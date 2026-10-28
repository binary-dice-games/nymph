// MIT License © 2026 Binary Dice Games
/// @file test_window_wake.cpp
/// @brief spec/14_editor_workspace plan step 8: wake() from another thread
///        ends a pump(true) waiting for events. Skipped without a display.
#include <gtest/gtest.h>
#include <chrono>
#include <future>
#include <thread>
#include "window/viewer_window.hpp"
#include "window/window_host.hpp"

namespace bdg::nymph {
namespace {

struct quiet_events : window_events {
  void on_resized() override {}
  void on_expose() override {}
  bool on_close_request() override {
    return true;
  }
  void on_pointer(pointer_event::kind, float, float, int, float, float) override {}
  void on_key(window_key, key_mods, bool, bool) override {}
  void on_text(char32_t) override {}
};

TEST(window_wake, WakeFromAnotherThreadEndsAWaitingPump) {
  quiet_events events;
  std::unique_ptr<native_window> window;
  try {
    window = create_native_window(events);
  } catch (const std::exception& e) {
    GTEST_SKIP() << "no window here: " << e.what();
  }
  window->pump(false); // drain what creating it queued
  std::thread waker([&] {
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // let pump() start waiting
    window->wake();
  });
  const auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(window->pump(true));
  waker.join();
  EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(5));
}

} // namespace
} // namespace bdg::nymph
